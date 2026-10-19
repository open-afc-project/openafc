# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.
#

''' Application configuration data.
'''
import abc
import os
import sys
import logging
from typing import Optional
import datetime

#: The externally-visible script root path
APPLICATION_ROOT = '/fbrat'
#: Enable debug mode for flask
DEBUG = False
#: Enable detailed exception stack messages
PROPAGATE_EXCEPTIONS = False
#: Root logger filter
AFC_RATAPI_LOG_LEVEL = os.getenv("AFC_RATAPI_LOG_LEVEL", "WARNING")
# Default request timeout in seconds.
# This budget starts when rat_server dispatches the Celery task, so it must
# cover both Celery queue-wait time and engine computation time.  In a
# single-worker (serial) deployment the queue can hold many precompute tasks
# ahead of a test request, each taking 30–60 s; 1800 s gives comfortable
# headroom while still bounding runaway tasks.
AFC_MSGHND_RATAFC_TOUT = int(os.getenv("AFC_MSGHND_RATAFC_TOUT", "1800"))
#: Set of log handlers to use for root logger
LOG_HANDLERS = [
    logging.StreamHandler(),
]


class _CredentialRedactFilter(logging.Filter):
    """Logging filter that redacts DSN/URL passwords before emission.

    Catches any log record whose rendered message contains an AMQP/HTTP/
    PostgreSQL password embedded as ``://user:password@host``.  Applied to
    every root-logger handler: handler-level filters run in Handler.handle()
    for all records the handler emits, including records propagated from
    named/library child loggers, which never traverse logger-level filters
    attached to the root logger itself.
    """
    import re as _re
    _DSN_RE = _re.compile(r"(://[^:@/\s]+:)[^@\s]+(@)")

    def filter(self, record: logging.LogRecord) -> bool:
        try:
            msg = record.getMessage()
            if "://" in msg and "@" in msg:
                record.msg = self._DSN_RE.sub(r"\1***\2", msg)
                record.args = ()
        except Exception:
            # Fail closed: if redaction cannot be evaluated, withhold the
            # message rather than risk emitting an unredacted secret.
            record.msg = ("[log message withheld: credential-redact filter "
                          "error]")
            record.args = ()
        return True


def install_credential_redact_filter() -> None:
    """Install _CredentialRedactFilter on all root-logger handlers (idempotent).

    The filter must be attached to *handlers*, not only the root logger:
    CPython consults logger-level filters only in Logger.handle() of the
    logger a record was emitted on, so records from named/library loggers
    (logging.getLogger(__name__), pika, kombu, sqlalchemy, celery) propagate
    to root handlers without ever running root's own filters.  Call this
    AFTER adding any service-specific handlers to the root logger.  The
    filter is also kept on the root logger itself so records emitted
    directly on it stay redacted even when no handler is installed
    (logging.lastResort path).
    """
    root = logging.getLogger()
    if not any(isinstance(f, _CredentialRedactFilter)
               for f in root.filters):
        root.addFilter(_CredentialRedactFilter())
    for handler in set(root.handlers) | set(LOG_HANDLERS):
        if not any(isinstance(f, _CredentialRedactFilter)
                   for f in handler.filters):
            handler.addFilter(_CredentialRedactFilter())


SECRET_KEY = None  # Must be set in app config

# Flask-SQLAlchemy settings
# postgresql://[user[:password]@][netloc][:port][/dbname][?param1=value1&...]
SQLALCHEMY_DATABASE_URI = None  # Must be set in app config
SQLALCHEMY_TRACK_MODIFICATIONS = False  # Avoids SQLAlchemy warning
SQLALCHEMY_ENGINE_OPTIONS = \
    {
        # Avoids EOF on stale connections that causes Error 500 in Web UI
        'pool_pre_ping': True,
    }

# Application name (used in templates and page footers)
USER_APP_NAME = "AFC"
REMEMBER_COOKIE_DURATION = datetime.timedelta(days=30)
USER_USER_SESSION_EXPIRATION = 3600  # One hour idle timeout
PERMANENT_SESSION_LIFETIME = datetime.timedelta(
    seconds=USER_USER_SESSION_EXPIRATION)

#: API key used for google maps
GOOGLE_APIKEY = None
#: Dynamic system data (both model data and configuration)
STATE_ROOT_PATH = '/var/lib/fbrat'
#: Mount path
NFS_MOUNT_PATH = '/mnt/nfs'
#: Use random PAWS response flag
PAWS_RANDOM = False
#: History directory for log file storage
HISTORY_DIR = None
#: Task queue directory
TASK_QUEUE = '/var/spool/fbrat'

#: Tracks if the daily uls parser ran today. Overwritten by the tasks that use it.
DAILY_ULS_RAN_TODAY = False


def read_secret(env_var: str, file_env_var: Optional[str] = None) -> Optional[str]:
    """Read a secret from a Docker secret file or plain environment variable.

    Prefers the file path from ``file_env_var`` (e.g. ``BROKER_PWD_FILE``)
    over the direct value in ``env_var`` (e.g. ``BROKER_PWD``).  Returns
    ``None`` when neither is set.
    """
    file_path = os.getenv(file_env_var or (env_var + "_FILE"))
    if file_path:
        try:
            with open(file_path) as fh:
                return fh.read().strip()
        except OSError as exc:
            logging.getLogger(__name__).critical(
                # logs file PATH and OS error, not the secret value
                "FATAL: cannot read secret file %s (%s): %s",
                file_env_var or (env_var + "_FILE"), file_path, exc)
            sys.exit(1)
    return os.getenv(env_var)


class InvalidEnvVar(Exception):
    """Wrong/missing env var exception"""
    pass


class BrokerConfigurator(object):
    """Keep configuration for a broker"""

    def __init__(self) -> None:
        import logging as _logging
        self.BROKER_PROT = os.getenv('BROKER_PROT', 'amqp')
        self.BROKER_USER = os.getenv('BROKER_USER', 'celery')
        self.BROKER_PWD = read_secret('BROKER_PWD')
        if not self.BROKER_PWD:
            _logging.getLogger(__name__).critical(
                "FATAL: BROKER_PWD is not set. "
                "Set BROKER_PWD_FILE to a Docker secret path, or set BROKER_PWD. "
                "Generate a random secret: "
                "python3 -c \"import secrets; print(secrets.token_hex(32))\"")
            sys.exit(1)

        self.BROKER_FQDN = os.getenv('BROKER_FQDN', 'rmq')
        self.BROKER_PORT = os.getenv('BROKER_PORT', '5672')
        self.BROKER_VHOST = os.getenv('BROKER_VHOST', 'fbrat')
        self.BROKER_URL = self.BROKER_PROT +\
            "://" +\
            self.BROKER_USER +\
            ":" +\
            self.BROKER_PWD +\
            "@" +\
            self.BROKER_FQDN +\
            ":" +\
            self.BROKER_PORT +\
            "/" +\
            self.BROKER_VHOST
        self.BROKER_EXCH_DISPAT = os.getenv(
            'BROKER_EXCH_DISPAT', 'dispatcher_bcast')


class ObjstConfigBase():
    """Parent of configuration for objstorage"""

    def __init__(self):
        self.AFC_OBJST_PORT = os.getenv("AFC_OBJST_PORT", "5000")
        if not self.AFC_OBJST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_PORT env var.")
        self.AFC_OBJST_HIST_PORT = os.getenv("AFC_OBJST_HIST_PORT", "4999")
        if not self.AFC_OBJST_HIST_PORT.isdigit():
            raise InvalidEnvVar("Invalid AFC_OBJST_HIST_PORT env var.")


class ObjstConfig(ObjstConfigBase):
    """Filestorage external config"""

    def __init__(self):
        ObjstConfigBase.__init__(self)
        self.AFC_OBJST_HOST = os.getenv("AFC_OBJST_HOST")

        self.AFC_OBJST_SCHEME = None
        if "AFC_OBJST_SCHEME" in os.environ:
            self.AFC_OBJST_SCHEME = os.environ["AFC_OBJST_SCHEME"]
            if self.AFC_OBJST_SCHEME not in ("HTTPS", "HTTP"):
                raise InvalidEnvVar("Invalid AFC_OBJST_SCHEME env var.")


class SecretConfigurator(object):

    def __init__(self, secret_env, file_env_prefix, bool_attr, str_attr,
                 int_attr):
        attr = bool_attr + str_attr + int_attr

        # Initialize to false and empty
        for k in bool_attr:
            setattr(self, k, False)

        for k in str_attr:
            setattr(self, k, "")

        for k in int_attr:
            setattr(self, k, 0)

        # load priv config if available.
        try:
            from ratapi import priv_config
            for k in attr:
                val = getattr(priv_config, k, None)
                if val is not None:
                    setattr(self, k, val)
        except BaseException:
            priv_config = None

        # override boolean config with environment variables
        for k in bool_attr:
            # Override with environment variables
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, (ret.lower() == 'true'))

        # override string config with environment variables
        for k in str_attr:
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, ret)

        # override int config with environment variables
        for k in int_attr:
            ret = self._getenv(k, file_env_prefix)
            if ret:
                setattr(self, k, int(ret))

        # Override values from config with secret file
        secret_file = os.getenv(secret_env)
        if secret_file:
            import json
            with open(secret_file) as secret_content:
                data = json.load(secret_content)
                for k in bool_attr:
                    if k in data:
                        setattr(self, k, data[k].lower() == 'true')
                for k in str_attr:
                    if k in data:
                        setattr(self, k, data[k])
                for k in int_attr:
                    if k in data:
                        setattr(self, k, int(data[k]))

    def _getenv(self, attr, file_env_prefix):
        ret = os.environ.get(attr)
        if ret:
            return ret
        filename = os.environ.get(file_env_prefix + attr)
        if filename and os.path.isfile(filename):
            with open(filename, encoding="utf-8") as f:
                return f.read()
        return None


class OIDCConfigurator(SecretConfigurator):
    def __init__(self):
        oidc_bool_attr = ['OIDC_LOGIN']
        oidc_str_attr = ['OIDC_CLIENT_ID',
                         'OIDC_CLIENT_SECRET', 'OIDC_DISCOVERY_URL',
                         'OIDC_REDIRECT_BASE']
        oidc_env = 'OIDC_ARG'
        super().__init__(secret_env=oidc_env, file_env_prefix='OIDCFILE_',
                         bool_attr=oidc_bool_attr, str_attr=oidc_str_attr,
                         int_attr=[])


class RatApiConfigurator(SecretConfigurator):
    def __init__(self):
        ratapi_bool_attr = ['MAIL_USE_TLS', 'MAIL_USE_SSL', 'USE_CAPTCHA']
        ratapi_str_attr = [
            'REGISTRATION_APPROVE_LINK',
            'REGISTRATION_DEST_EMAIL',
            'REGISTRATION_DEST_PDL_EMAIL',
            'REGISTRATION_SRC_EMAIL',
            'MAIL_PASSWORD',
            'MAIL_USERNAME',
            'MAIL_SERVER',
            'CAPTCHA_SECRET',
            'CAPTCHA_SITEKEY',
            'CAPTCHA_VERIFY',
            'USER_APP_NAME']
        ratapi_int_attr = ['MAIL_PORT']
        ratapi_env = 'RATAPI_ARG'
        super().__init__(secret_env=ratapi_env, file_env_prefix='RATAPIFILE_',
                         bool_attr=ratapi_bool_attr, str_attr=ratapi_str_attr,
                         int_attr=ratapi_int_attr)


# Msghnd configuration interfaces

class MsghndConfiguration(abc.ABC):
    @abc.abstractmethod
    def get_name(self):
        # AFC_MSGHND_NAME
        pass

    @abc.abstractmethod
    def get_port(self):
        # AFC_MSGHND_PORT
        pass

    @abc.abstractmethod
    def get_bind(self):
        # AFC_MSGHND_BIND
        pass

    @abc.abstractmethod
    def get_access_log(self):
        # AFC_MSGHND_ACCESS_LOG
        pass

    @abc.abstractmethod
    def get_error_log(self):
        # AFC_MSGHND_ERROR_LOG
        pass

    @abc.abstractmethod
    def get_workers(self):
        # AFC_MSGHND_WORKERS
        pass

    @abc.abstractmethod
    def get_timeout(self):
        # AFC_MSGHND_TIMEOUT
        pass


class HealthchecksMsghndCfgIface(MsghndConfiguration):
    def __init__(self):
        setattr(self, 'AFC_MSGHND_NAME', os.getenv('AFC_MSGHND_NAME'))
        setattr(self, 'AFC_MSGHND_PORT', os.getenv('AFC_MSGHND_PORT'))

    def get_name(self):
        return self.AFC_MSGHND_NAME

    def get_port(self):
        return self.AFC_MSGHND_PORT

    def get_bind(self):
        pass

    def get_access_log(self):
        pass

    def get_error_log(self):
        pass

    def get_workers(self):
        pass

    def get_timeout(self):
        pass


class RatafcMsghndCfgIface(MsghndConfiguration):
    def get_name(self):
        pass

    def get_port(self):
        pass

    def get_bind(self):
        pass

    def get_access_log(self):
        pass

    def get_error_log(self):
        pass

    def get_workers(self):
        pass

    def get_timeout(self):
        pass
