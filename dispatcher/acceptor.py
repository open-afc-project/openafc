#!/usr/bin/env python3
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""
Description

The acceptor client (aka consumer) registeres own queue within broker
application (aka rabbitmq). Such queue used as a channel for control commands.
"""

import appcfg
from appcfg import BrokerConfigurator, ObjstConfig
import hashlib
import hmac
import os
import shutil
import sys
from sys import stdout
import logging
from logging.config import dictConfig
import argparse
import inspect
import gevent
import subprocess
import tempfile
import time
import urllib.parse
from ncli import MsgAcceptor
from hchecks import MsghndHealthcheck, ObjstHealthcheck
from fst import DataIf

dictConfig({
    'version': 1,
    'disable_existing_loggers': False,
    'formatters': {
        'standard': {
            'format': '%(asctime)s - [%(levelname)s] %(name)s [%(module)s.%(funcName)s:%(lineno)d]: %(message)s',
            'datefmt': '%Y-%m-%d %H:%M:%S',
        }
    },
    'handlers': {
        'console': {
            'level': 'DEBUG',
            'class': 'logging.StreamHandler',
            'formatter': 'standard',
        }
    },
    'root': {
        'level': 'INFO',
        'handlers': ['console']
    },
})
app_log = logging.getLogger()
appcfg.install_credential_redact_filter()

# Match dispatcher/nginx.conf `pid` directive (nginx -s reload needs the master running).
NGINX_PID_FILE = '/var/run/nginx.pid'

# mTLS bundle version is signed as time_ns() (see admin.py).  The monotonic
# "_ver <= _last" replay check is container-local: if /etc/nginx/certs is not
# a persistent host mount, container recreation resets _last to -1 and any
# previously-signed (bundle, sidecar) pair — however old — would otherwise
# pass.  Bind an absolute freshness ceiling under the same HMAC so a replayed
# old bundle is rejected even when the monotonic floor has been lost.
BUNDLE_MAX_AGE_NS = 90 * 24 * 3600 * 10**9  # 90 days

# Freshness window for signed cmd_remove control messages (nanoseconds).
# BROKER_URL is a fleet-shared credential; trust removal is authenticated
# per message (see _verify_remove) rather than trusting the transport.
CMD_REMOVE_MAX_AGE_NS = 300 * 10**9
# Replay protection for cmd_remove within process lifetime; the freshness
# window above bounds replay across acceptor restarts.
_last_remove_ts = -1
_seen_remove_nonces = set()


def _safe_broker_url(url: str) -> str:
    """Return AMQP URL with password replaced by <REDACTED>."""
    try:
        parsed = urllib.parse.urlparse(url)
        if parsed.password:
            netloc = parsed.hostname or ''
            if parsed.port:
                netloc = f"{parsed.username}:<REDACTED>@{netloc}:{parsed.port}"
            else:
                netloc = f"{parsed.username}:<REDACTED>@{netloc}"
            return urllib.parse.urlunparse(parsed._replace(netloc=netloc))
    except Exception:
        pass
    return '<broker-url>'


def wait_for_nginx_master(timeout_sec=60.0, poll_sec=0.2) -> bool:
    """Wait until nginx has written its master PID file (avoids reload races)."""
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            if os.path.isfile(NGINX_PID_FILE):
                with open(NGINX_PID_FILE, encoding='ascii') as pidf:
                    if pidf.read().strip():
                        return True
        except OSError:
            pass
        time.sleep(poll_sec)
    return False


def reload_nginx() -> bool:
    """Send HUP to nginx master after ensuring it is up."""
    if not wait_for_nginx_master():
        app_log.error(
            'nginx reload skipped: %s not ready within timeout',
            NGINX_PID_FILE,
        )
        return False
    try:
        proc = subprocess.run(
            ['nginx', '-s', 'reload'],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
    except OSError as exc:
        app_log.error('nginx -s reload failed to run: %s', exc)
        return False
    if proc.returncode != 0:
        app_log.error(
            'nginx -s reload failed (exit %s): stdout=%r stderr=%r',
            proc.returncode,
            proc.stdout,
            proc.stderr,
        )
        return False
    if proc.stderr:
        app_log.debug('nginx -s reload stderr: %s', proc.stderr.strip())
    return True


class Configurator(dict):
    __instance = None

    def __new__(cls):
        if cls.__instance is None:
            cls.__instance = dict.__new__(cls)
        return cls.__instance

    def __init__(self):
        dict.__init__(self)
        self.update(BrokerConfigurator().__dict__.items())
        self.update(ObjstConfig().__dict__.items())
        self['OBJST_CERT_CLI_BUNDLE'] = \
            'certificate/client.bundle.pem'
        self['DISPAT_CERT_CLI_BUNDLE'] = \
            '/etc/nginx/certs/client.bundle.pem'
        # Placeholder CA (no client certs issued against it); used by
        # run_remove() to restore a "no real trust" state without writing an
        # empty PEM that would crash nginx.  May be populated from the
        # host-mounted dispatcher/certs/clients/ directory; if absent,
        # run_remove() generates a fresh throwaway placeholder here on the
        # fly (see _generate_placeholder_ca) so trust removal always
        # converges instead of silently no-op'ing.
        self['DISPAT_CERT_DUMMY_CA'] = \
            '/etc/nginx/certs/dummy_ca.pem'


log_level_map = {
    'debug': logging.DEBUG,    # 10
    'info': logging.INFO,      # 20
    'warn': logging.WARNING,   # 30
    'err': logging.ERROR,      # 40
    'crit': logging.CRITICAL,  # 50
}


def set_log_level(opt) -> int:
    app_log.info(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                 f"{app_log.getEffectiveLevel()}")
    app_log.setLevel(log_level_map[opt])
    return log_level_map[opt]


def readiness_check(cfg):
    """Provide readiness check by calling for response preconfigured
       list of subjects (containers)
    """
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    objst_chk = ObjstHealthcheck(cfg)
    msghnd_chk = MsghndHealthcheck.from_hcheck_if()
    checks = [gevent.spawn(objst_chk.healthcheck),
              gevent.spawn(msghnd_chk.healthcheck)]
    gevent.joinall(checks)
    for i in checks:
        if i.value != 0:
            return i.value
    return 0


def run_restart(cfg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    bundle_written = False
    with DataIf().open(cfg['OBJST_CERT_CLI_BUNDLE']) as hfile:
        if hfile.head():
            app_log.debug(f"Found cert bundle file.")
            bundle_bytes = hfile.read()
            # Verify HMAC-SHA256 signature written by admin.py before
            # installing the bundle.  The HMAC key is a dedicated secret
            # shared only between the admin upload path and the dispatcher
            # (DISPATCHER_BUNDLE_HMAC_KEY_FILE) so bundle integrity is
            # independent of object storage access AND of the widely-shared
            # AFC_INTERNAL_TOKEN gateway bearer.
            # Both HMAC key and .hmac sidecar MUST be present; fail-closed
            # otherwise (no more transition-period bypass).
            _key_file = os.environ.get("DISPATCHER_BUNDLE_HMAC_KEY_FILE", "")
            _token = ""
            if _key_file and os.path.isfile(_key_file):
                with open(_key_file, "r") as _kf:
                    _token = _kf.read().strip()
            _install_bundle = False
            if not _token:
                app_log.error(
                    "DISPATCHER_BUNDLE_HMAC_KEY_FILE not set or unreadable "
                    "— refusing to install mTLS bundle without HMAC "
                    "verification. Mount the dedicated bundle-signing "
                    "secret in the dispatcher container.")
            else:
                _hmac_path = cfg['OBJST_CERT_CLI_BUNDLE'] + '.hmac'
                _ver_path = cfg['DISPAT_CERT_CLI_BUNDLE'] + '.ver'
                _ver = -1
                with DataIf().open(_hmac_path) as _hf:
                    if _hf.head():
                        # Objstorage content is untrusted input: a
                        # non-UTF-8 or unreadable sidecar must refuse
                        # installation, not crash the acceptor (and with
                        # it the WAN nginx container that shares its PID1
                        # shell — see dispatcher/entrypoint.sh).
                        try:
                            _sidecar = _hf.read().decode().strip()
                        except (UnicodeDecodeError, OSError) as _exc:
                            app_log.error(
                                "mTLS bundle HMAC sidecar unreadable or "
                                "not valid UTF-8 (%s) — refusing to "
                                "install bundle.", _exc)
                            _sidecar = ""
                        # Sidecar is "<version>:<hexdigest>"; the version is
                        # bound under the MAC so a replayed older bundle
                        # cannot be re-installed once a newer one has been.
                        _sver, _, _expected = _sidecar.partition(':')
                        try:
                            _ver = int(_sver)
                        except ValueError:
                            _ver = -1
                        _actual = hmac.new(
                            _token.encode(),
                            f"{_ver}|{len(bundle_bytes)}|".encode()
                            + bundle_bytes,
                            hashlib.sha256).hexdigest()
                        _last = -1
                        if os.path.isfile(_ver_path):
                            try:
                                with open(_ver_path, 'r') as _vf:
                                    _last = int(_vf.read().strip())
                            except (OSError, ValueError):
                                _last = -1
                        if not _sidecar:
                            pass  # already logged above; _install_bundle stays False
                        elif not hmac.compare_digest(_expected, _actual):
                            app_log.error(
                                "mTLS bundle HMAC mismatch — "
                                "refusing to install; check bundle integrity.")
                        elif _ver <= _last:
                            app_log.error(
                                "mTLS bundle version %d <= last-installed %d "
                                "— refusing to install (replay protection).",
                                _ver, _last)
                        elif _ver <= 0 or \
                                (time.time_ns() - _ver) > BUNDLE_MAX_AGE_NS:
                            # Defends the case where the monotonic floor
                            # above (_last) was reset by a container
                            # recreation that lost the non-persistent
                            # /etc/nginx/certs/*.ver file: an old but
                            # validly-signed (bundle, sidecar) pair is
                            # still rejected once it is older than the
                            # absolute freshness ceiling.
                            app_log.error(
                                "mTLS bundle version %d is outside the "
                                "install freshness window — refusing to "
                                "install (replay protection).", _ver)
                        else:
                            _install_bundle = True
                    else:
                        # No .hmac sidecar — fail closed.  The sidecar is
                        # written by admin.py alongside every bundle upload.
                        app_log.error(
                            "No HMAC signature for mTLS bundle in "
                            "objstorage — refusing to install without "
                            "verification. Upload the bundle via the admin "
                            "API to generate the required .hmac sidecar.")
            if _install_bundle:
                try:
                    # Decode before opening the target: open(..., 'w')
                    # truncates, so a decode failure mid-write would leave
                    # an empty PEM that crashes nginx.
                    _bundle_text = bundle_bytes.decode('utf-8')
                except UnicodeDecodeError as _exc:
                    app_log.error(
                        "mTLS bundle is not valid UTF-8 (%s) — refusing "
                        "to install.", _exc)
                else:
                    with open(cfg['DISPAT_CERT_CLI_BUNDLE'], 'w') as ofile:
                        ofile.write(_bundle_text)
                    with open(_ver_path, 'w') as _vf:
                        _vf.write(str(_ver))
                    app_log.info(
                        f"{os.path.getctime(cfg['DISPAT_CERT_CLI_BUNDLE'])}, "
                        f"{os.path.getsize(cfg['DISPAT_CERT_CLI_BUNDLE'])}")
                    bundle_written = True
        else:
            app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
            # Do not write an empty PEM: nginx cannot load ssl_client_certificate
            # from an empty file (PEM_read_bio_X509_AUX "no start line"), and a
            # write here overwrites the bind-mounted host file on every start.
            # Per security review, do not restore a built-in test CA from the
            # image; keep the on-disk bundle unchanged until object storage has
            # a bundle or the operator replaces the mounted file.
            disp = cfg['DISPAT_CERT_CLI_BUNDLE']
            if os.path.isfile(disp) and os.path.getsize(disp) > 0:
                app_log.debug(
                    'No client bundle in object storage (%s); '
                    'using mounted bundle at %s (no nginx reload needed)',
                    cfg['OBJST_CERT_CLI_BUNDLE'],
                    disp,
                )
            else:
                app_log.warning(
                    'No client CA bundle in object storage (%s) and none at %s — '
                    'provision a PEM bundle before enforcing mTLS.',
                    cfg['OBJST_CERT_CLI_BUNDLE'],
                    disp,
                )
    if bundle_written:
        reload_nginx()


def _generate_placeholder_ca(path) -> bool:
    """Generate a throwaway self-signed placeholder CA at `path`.

    No client certificates are ever issued against this CA, so installing it
    as the nginx ssl_client_certificate bundle trusts nobody while nginx can
    still load a valid non-empty PEM (an empty file would crash it).  The
    private key is discarded with the temp directory.  Mirrors the
    placeholder-CRL generation in dispatcher/entrypoint.sh: every deployment
    (and every cmd_remove) gets a fresh, unique, unforgeable placeholder
    instead of a static repo-committed dummy CA.
    """
    work = tempfile.mkdtemp(prefix='dummy_ca.')
    try:
        key = os.path.join(work, 'ca.key')
        proc = subprocess.run(
            ['openssl', 'req', '-new', '-x509', '-newkey', 'rsa:2048',
             '-nodes', '-keyout', key, '-out', path,
             '-days', '1', '-subj', '/CN=placeholder-no-trust',
             '-sha256'],
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )
        if proc.returncode != 0:
            app_log.error(
                'placeholder CA generation failed (exit %s): stderr=%r',
                proc.returncode, proc.stderr)
            return False
        return True
    except (OSError, subprocess.TimeoutExpired) as exc:
        app_log.error('placeholder CA generation failed to run: %s', exc)
        return False
    finally:
        shutil.rmtree(work, ignore_errors=True)


def run_remove(cfg):
    """Handle cmd_remove broker message.

    Replaces the live mTLS CA bundle with a dummy/placeholder CA so that
    nginx continues to start normally (ssl_client_certificate requires a valid
    non-empty PEM) while no real client certificates remain trusted.

    Writing an empty PEM would crash nginx ('no start line'), so a placeholder
    is used instead of an empty file.
    """
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}() "
                  f"{cfg['DISPAT_CERT_CLI_BUNDLE']}")
    dummy = cfg.get('DISPAT_CERT_DUMMY_CA', '')
    bundle = cfg['DISPAT_CERT_CLI_BUNDLE']
    if not dummy:
        app_log.error(
            "cmd_remove: dummy CA placeholder path not configured — "
            "mTLS bundle NOT changed.")
        return
    if not os.path.isfile(dummy):
        # No deployment artifact provisions the placeholder in every
        # topology (image bakes a static file at a different target; the
        # shipped compose bind-mount hides it) — generate a fresh
        # throwaway placeholder CA on the fly instead of silently leaving
        # the removed CA trusted (trust removal must converge).
        if not _generate_placeholder_ca(dummy):
            app_log.error(
                "cmd_remove: dummy CA placeholder not found at %s and "
                "generation failed — mTLS bundle NOT changed.  Provision "
                "%s before using cmd_remove.",
                dummy, dummy)
            return
    try:
        shutil.copy2(dummy, bundle)
        app_log.info(
            "cmd_remove: replaced mTLS bundle with dummy CA placeholder (%s). "
            "No real client certificates are now trusted.",
            bundle)
        reload_nginx()
    except OSError as exc:
        app_log.error(
            "cmd_remove: failed to install dummy CA at %s: %s", bundle, exc)


def _verify_remove(msg) -> bool:
    """Authenticate a cmd_remove control message.

    BROKER_URL is a fleet-shared credential (every worker/msghnd/rat_server
    container that mounts it can publish to the dispatcher fanout exchange),
    so trust *removal* must be authenticated per message, mirroring the HMAC
    gate already required for bundle *installation* (run_restart). Expected
    form: 'cmd_remove:<ts_ns>:<nonce>:<hexsig>' where
    hexsig = HMAC-SHA256(key, 'cmd_remove|<ts_ns>|<nonce>') keyed on the
    dedicated DISPATCHER_BUNDLE_HMAC_KEY. Fails closed on any error.
    """
    global _last_remove_ts
    _key_file = os.environ.get("DISPATCHER_BUNDLE_HMAC_KEY_FILE", "")
    _token = ""
    if _key_file and os.path.isfile(_key_file):
        with open(_key_file, "r") as _kf:
            _token = _kf.read().strip()
    if not _token:
        app_log.error(
            "cmd_remove rejected: DISPATCHER_BUNDLE_HMAC_KEY_FILE not set "
            "or unreadable — cannot authenticate trust-removal command.")
        return False
    parts = msg.split(':')
    if len(parts) != 4 or parts[0] != 'cmd_remove':
        app_log.error("cmd_remove rejected: unsigned or malformed command.")
        return False
    _ts_str, _nonce, _sig = parts[1], parts[2], parts[3]
    try:
        _ts = int(_ts_str)
    except ValueError:
        app_log.error("cmd_remove rejected: malformed timestamp.")
        return False
    _expected = hmac.new(
        _token.encode(),
        f"cmd_remove|{_ts_str}|{_nonce}".encode(),
        hashlib.sha256).hexdigest()
    if not hmac.compare_digest(_expected, _sig):
        app_log.error("cmd_remove rejected: HMAC mismatch.")
        return False
    if abs(time.time_ns() - _ts) > CMD_REMOVE_MAX_AGE_NS:
        app_log.error("cmd_remove rejected: stale timestamp (replay?).")
        return False
    if _ts <= _last_remove_ts or _nonce in _seen_remove_nonces:
        app_log.error("cmd_remove rejected: replayed timestamp/nonce.")
        return False
    _last_remove_ts = _ts
    _seen_remove_nonces.add(_nonce)
    return True


commands_map = {
    'cmd_restart': run_restart,
}


def get_commands(cfg, msg):
    """Get messages"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")
    if not isinstance(msg, str):
        app_log.warning(
            f"({os.getpid()}) Non-string broker message ignored: %r",
            type(msg).__name__)
        return
    if msg == 'cmd_remove' or msg.startswith('cmd_remove:'):
        # cmd_remove is authenticated separately (see _verify_remove): an
        # unsigned bare 'cmd_remove' is exactly the CWE-306 this closes, so
        # it is intentionally NOT in commands_map.
        if _verify_remove(msg):
            _dispatch(run_remove, cfg, msg)
        return
    handler = commands_map.get(msg)
    if handler is None:
        app_log.warning(f"({os.getpid()}) Unknown broker command ignored: %r", msg)
        return
    _dispatch(handler, cfg, msg)


def _dispatch(handler, cfg, msg):
    """Run a broker command handler, containing any exception.

    A malformed/hostile object surfaced through a handler (e.g. objstorage
    content in run_restart) must not terminate the broker consumer loop —
    and with it, via dispatcher/entrypoint.sh's shared PID1 shell, the WAN
    nginx container.
    """
    try:
        handler(cfg)
    except Exception as exc:
        app_log.error("(%s) Broker command %r failed: %s",
                      os.getpid(), msg, exc)


def run_it(cfg):
    """Execute command line run command"""
    app_log.debug(f"({os.getpid()}) {inspect.stack()[0][3]}()")

    # check if lucky to find new certificate bundle already
    # Contained the same way as broker-triggered commands (_dispatch): a
    # malformed object in storage must not prevent the acceptor from
    # starting and taking over as the container's foreground process.
    _dispatch(run_restart, cfg, 'startup cmd_restart')

    maker = MsgAcceptor(cfg['BROKER_URL'], cfg['BROKER_EXCH_DISPAT'],
                        msg_handler=get_commands, handler_params=cfg)
    app_log.info(f"({os.getpid()}) Connected to {_safe_broker_url(cfg['BROKER_URL'])}")
    maker.run()


# available commands to execute in alphabetical order
execution_map = {
    'run': run_it,
    'check': readiness_check,
}


def make_arg_parser():
    """Define command line options"""
    args_parser = argparse.ArgumentParser(
        epilog=__doc__.strip(),
        formatter_class=argparse.RawTextHelpFormatter)
    args_parser.add_argument('--log', type=set_log_level,
                             default='info', dest='log_level',
                             help="<info|debug|warn|err|crit> - set "
                             "logging level (default=info).\n")
    args_parser.add_argument('--cmd', choices=execution_map.keys(),
                             nargs='?',
                             help="run - start accepting commands.\n"
                             "check - run readiness check.\n")

    return args_parser


def prepare_args(parser, cfg):
    """Prepare required parameters"""
    app_log.debug(f"{inspect.stack()[0][3]}() {parser.parse_args()}")
    cfg.update(vars(parser.parse_args()))


def main():
    """Main function of the utility"""
    res = 0
    parser = make_arg_parser()
    config = Configurator()

    if prepare_args(parser, config) == 1:
        # error in preparing arguments
        res = 1
    else:
        if isinstance(config['cmd'], type(None)):
            parser.print_help()

    if res == 0:
        safe_cfg = dict(config)
        for k in list(safe_cfg):
            if 'PWD' in k or 'SECRET' in k or k.endswith('_KEY'):
                safe_cfg[k] = '<REDACTED>'
        if 'BROKER_URL' in safe_cfg:
            safe_cfg['BROKER_URL'] = _safe_broker_url(safe_cfg['BROKER_URL'])
        app_log.debug(f"{inspect.stack()[0][3]}() {safe_cfg}")
        res = execution_map[config['cmd']](config)
    sys.exit(res)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(1)


# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
