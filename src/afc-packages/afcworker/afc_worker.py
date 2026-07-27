#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
import urllib.parse
import hashlib
import os
import re
import resource
import signal
import subprocess
import shutil
import tempfile
import time
import zlib
from typing import Optional
from celery import Celery
from celery.utils.log import get_task_logger
from kombu import Queue
from appcfg import BrokerConfigurator, install_credential_redact_filter
from fst import DataIf
import defs
import afctask
import als
import json
from rcache_models import RcacheClientSettings
from rcache_client import RcacheClient

LOGGER = get_task_logger(__name__)

# S0127-13: install the credential redact filter on the root logger in this
# process at module init, same as rat_server (app.py), afc_server
# (afc_server_app.py), als_siphon.py and dispatcher/acceptor.py. Without
# this, no root-logger redaction covers the worker process and
# _safe_broker_url()'s local fail-open below is the only mitigation.
install_credential_redact_filter()


class WorkerConfig(BrokerConfigurator):
    """Worker internal config"""

    def __init__(self):
        BrokerConfigurator.__init__(self)
        self.AFC_ENGINE = os.getenv("AFC_ENGINE")
        self.AFC_ENGINE_LOG_LVL = os.getenv("AFC_ENGINE_LOG_LVL", "info")
        # Per-subprocess data-segment cap (RLIMIT_DATA) in bytes.
        # Default: 32 GiB — the afc-engine + libaep.so terrain-tile preload
        # reserves ~10–11 GiB of anonymous virtual address space (VmData) even
        # for simple requests; the old 8 GiB default was insufficient.
        # Note: VmData is virtual-only (physical RSS is ~100 MB), so this cap
        # does not directly prevent OOM — use Docker memory.max / cgroup limits
        # for that.  Set AFC_ENGINE_MEM_LIMIT_BYTES=0 to disable the cap
        # entirely when a container cgroup already enforces physical limits.
        _mem_env = int(
            os.getenv("AFC_ENGINE_MEM_LIMIT_BYTES") or str(32 * 1024 ** 3))
        self.AFC_ENGINE_MEM_LIMIT = _mem_env if _mem_env > 0 else None
        self.AFC_WORKER_CELERY_LOG = os.getenv("AFC_WORKER_CELERY_LOG")


conf = WorkerConfig()

_rcache_settings = None
_rcache_client = None


def get_rcache_client():
    """ Delayed rcache client initialization
    Avoids initialization when Celery client includes this file just for
    signatures """
    global _rcache_settings, _rcache_client
    if _rcache_settings is None:
        _rcache_settings = RcacheClientSettings(postgres_dsn=None)
        _rcache_settings.validate_for(rmq=True, rcache=True)
        _rcache_client = \
            RcacheClient(_rcache_settings, rmq_receiver=False) \
            if _rcache_settings.enabled else None
    return _rcache_client


def _safe_broker_url(url: str) -> str:
    """ Returns AMQP/broker URL with password redacted for safe logging """
    try:
        parsed = urllib.parse.urlparse(url)
        if parsed.password:
            netloc = f"{parsed.username}:<REDACTED>@{parsed.hostname}"
            if parsed.port:
                netloc += f":{parsed.port}"
            return urllib.parse.urlunparse(parsed._replace(netloc=netloc))
    except Exception:
        pass
    # Fail closed: never return the raw URL on the exception path (or the
    # no-password fall-through) — it may embed a password that
    # urllib.parse could not handle (e.g. '@'/'#'/invalid %-escapes in
    # BROKER_PWD). Mirrors dispatcher/acceptor.py's placeholder behaviour.
    return '<broker-url>'


LOGGER.info('Celery Broker: %s', _safe_broker_url(conf.BROKER_URL))

# Celery task priority levels.  RabbitMQ honours these when the queue is
# declared with x-max-priority (configured below).
CELERY_PRIORITY_USER = 5        # GUI / interactive user request
CELERY_PRIORITY_NORMAL = 3      # Internal / API request (default)
CELERY_PRIORITY_PRECOMPUTE = 1  # Background precompute — must never block user

# Queue names.  GUI requests go to CELERY_GUI_QUEUE so they are physically
# separated from AP requests and never compete with prefetched AP tasks.
# Workers consuming only CELERY_GUI_QUEUE provide a dedicated slot for WebUI.
CELERY_QUEUE = "celery"
CELERY_GUI_QUEUE = "celery_gui"

#: constant celery reference. Configure once flask app is created
client = Celery(
    'fbrat',
    broker=conf.BROKER_URL,
    task_ignore_result=True,
    broker_pool_limit=0,
    task_acks_late=True,
    worker_prefetch_multiplier=1,
)
# Enable RabbitMQ priority queue so CELERY_PRIORITY_USER tasks are always
# dequeued before CELERY_PRIORITY_PRECOMPUTE tasks, regardless of arrival order.
client.conf.task_queues = [
    Queue(CELERY_QUEUE, queue_arguments={'x-max-priority': CELERY_PRIORITY_USER}),
    Queue(CELERY_GUI_QUEUE, queue_arguments={'x-max-priority': CELERY_PRIORITY_USER}),
]
client.conf.task_default_priority = CELERY_PRIORITY_NORMAL


def _no_dup_keys(pairs):
    """ json.loads object_pairs_hook that rejects duplicate object keys.

    Python's json.loads normally resolves duplicate keys last-wins, but the
    C++ engine's QJsonDocument::fromJson (which parses the exact same raw
    bytes written to analysisRequest.json) is not guaranteed to resolve
    duplicates the same way. If the two disagree, the digest computed here
    over the Python-parsed view would authorise caching/delivery of a
    response the engine actually computed for a *different* view of the
    same bytes (SUB-0138-93). Rejecting duplicate keys outright removes the
    ambiguity instead of trying to match an external parser's precedence
    rules.
    """
    seen = set()
    result = {}
    for key, value in pairs:
        if key in seen:
            raise ValueError(f"Duplicate key {key!r} in JSON object")
        seen.add(key)
        result[key] = value
    return result


def _verify_req_cfg_digest(
        hash_val: str,
        original_request_str: Optional[str],
        request_str: Optional[str],
        config_str: Optional[str],
        runtime_opts: Optional[int],
        use_tasks: bool) -> bool:
    """Return True iff hash_val matches SHA-256(config_str||individual_req||runtime_opts).

    Uses the same algorithm as RequestConfigHash so legitimate tasks always
    pass. Returns False (suppresses caching) when inputs are absent or
    when the computed digest does not match the supplied hash_val.

    The hash is computed over an *individual* AFC request (one element of
    availableSpectrumInquiryRequests), NOT over the outer AFC message.
    The worker receives the full outer message in original_request_str, so
    we iterate over all individual requests and accept if any produces a match.

    Also verifies that request_str and original_request_str are structurally
    equivalent so that the engine input is bound to the authenticated request.
    """
    if use_tasks:
        # task-based path: engine input comes from objstore (not broker), so
        # digest-based verification is not applicable here.
        return True
    if not original_request_str or not config_str:
        # RMQ path with missing/empty inputs — refuse to authorise rcache
        # update when inputs cannot be validated.
        LOGGER.warning(
            "_verify_req_cfg_digest: empty original_request_str/config_str on "
            "RMQ path — suppressing rcache update.")
        return False
    # Reject immediately if the engine-input differs from the authenticated
    # original.  Normalise by re-serialising both so minor whitespace/key-order
    # differences are eliminated.
    if request_str and request_str != original_request_str:
        try:
            orig_parsed = json.loads(
                original_request_str, object_pairs_hook=_no_dup_keys)
            req_parsed = json.loads(
                request_str, object_pairs_hook=_no_dup_keys)
            if orig_parsed != req_parsed:
                LOGGER.warning(
                    "request_str differs from original_request_str — "
                    "suppressing rcache update.")
                return False
        except (json.JSONDecodeError, ValueError):
            LOGGER.warning(
                "Failed to parse request strings for equivalence check — "
                "suppressing rcache update.")
            return False
    try:
        msg = json.loads(
            original_request_str, object_pairs_hook=_no_dup_keys)
        # The outer AFC message may contain multiple individual requests; the
        # hash was computed for exactly one of them by RequestConfigHash.
        individual_requests = msg.get("availableSpectrumInquiryRequests")
        if not individual_requests:
            # Fallback: treat the parsed object as the individual request
            individual_requests = [msg]
        # Pre-compute config-only prefix of the hash (same as RequestConfigHash)
        h_cfg = hashlib.sha256()
        h_cfg.update(config_str.encode("utf-8"))
        # rcache consumers (rcache_models.from_req_resp_key,
        # afc_server_msg_proc._process_req) read element [0] only, so the
        # digest must bind element [0] specifically.  Verify index 0 only.
        for individual_request in individual_requests[:1]:
            h = h_cfg.copy()
            h.update(
                json.dumps(
                    {k: v for k, v in individual_request.items()
                     if k != "requestId"},
                    sort_keys=True).encode("utf-8"))
            if runtime_opts is not None:
                h.update(runtime_opts.to_bytes(4, "little"))
            if h.hexdigest() == hash_val:
                return True
        LOGGER.warning(
            "Request config digest mismatch — skipping rcache update "
            "(supplied=%s, no individual request in message matched). "
            "Request digest mismatch; rcache update suppressed.", hash_val)
        return False
    except Exception as exc:  # noqa: BLE001
        LOGGER.warning("Digest verification error: %s — "
                       "skipping rcache update", exc)
        return False


def _validate_broker_path(value, allowed_prefix, label):
    """Validate and sanitize a broker-supplied path string.

    URL-decodes and normalises dot-segments, then enforces that the result
    starts with *allowed_prefix* and contains no '..' path segments.

    Returns the sanitised path string on success.  Returns None and logs a
    warning on failure so callers can disable the associated feature rather
    than operating on an untrusted path value.

    Centralising this logic prevents the recurring regression pattern where a
    new code path skips one of the three required checks (unquote, normpath,
    startswith + '..'-in-segments).
    """
    if value is None:
        return None
    sanitised = os.path.normpath(urllib.parse.unquote(str(value)))
    if ".." in sanitised.split("/") or not sanitised.startswith(allowed_prefix):
        LOGGER.warning(
            "Rejected broker-supplied %s=%r — must start with %r "
            "and contain no '..' segments; feature disabled.",
            label, value, allowed_prefix)
        return None
    return sanitised


@client.task(ignore_result=True, soft_time_limit=1800)
def run(prot, host, port, request_type, task_id, hash_val,
        config_path, history_dir, runtime_opts, mntroot, rcache_queue,
        request_str, original_request_str, config_str, deadline):
    """ Run AFC Engine

        The parameters are all serializable so they can be passed through the message queue.

        :param request_type: Anslysis type to pass to AFC Engine

        :param task_id ID of associated task (if task-based synchronization is used), also used as name of directory for error files

        :param hash_val: md5 hex digest of request and config

        :param config_path: Objstore path of afc_config.json if task-based synchronization is used, None otherwise

        :param history_dir: Objstore directory for debug files files

        :param runtime_opts: Runtime options to pass to AFC Engine. Also specifies which debug fles to keep

        :param mntroot: path to directory where GeoData and config data are stored

        :param rcache_queue: None for task-based synchronization, RabbitMQ queue name for RMQ-based synchronization

        :param request_str None for task-based synchronization, request text for RMQ-based synchronization

        :param original_request_str None or original request string (without added vendor extensions) to put to rcache

        :param config_str None for task-based synchronization, config text for RMQ-based synchronization

        :param deadline Processung deadline (when msghnd timeout expires) as seconds since Epoch
    """
    LOGGER.debug(f"run(prot={prot}, host={host}, port={port}, "
                 f"task_id={task_id}, hash={hash_val}, opts={runtime_opts}, "
                 f"mntroot={mntroot}, timeout={deadline-time.time()}, "
                 f"rcache_queue={rcache_queue}")

    use_tasks = rcache_queue is None
    # Trust-boundary checks on broker-supplied values must not depend on
    # `assert` — assert statements are stripped under -O/-OO/PYTHONOPTIMIZE,
    # which would silently disable every check below. Use explicit
    # if/return guards instead (matching the surrounding validation style).
    if use_tasks:
        if not (task_id and config_path):
            LOGGER.error("Rejected use_tasks task: task_id/config_path "
                         "missing; aborting task.")
            return
    else:
        if not (get_rcache_client() and request_str and config_str and
                original_request_str):
            LOGGER.error("Rejected RMQ-sync task: rcache client/"
                         "request_str/config_str/original_request_str "
                         "missing; aborting task.")
            return
        if runtime_opts & defs.RNTM_OPT_AFCENGINE_HTTP_IO:
            # RNTM_OPT_AFCENGINE_HTTP_IO switches the engine's file I/O to
            # its unauthenticated Qt HTTP client, bypassing the
            # digest-verified local tmpdir inputs this RMQ path relies on.
            # It is only legitimate on the use_tasks/GUI path (ratapi.py);
            # never permit it here regardless of the runtime_opts clamp
            # below.
            LOGGER.error("Rejected RMQ-sync task: RNTM_OPT_AFCENGINE_HTTP_IO "
                         "is not permitted on the RMQ path; aborting task.")
            return

    # Validate Celery-message-supplied parameters against allowlists before
    # using them in subprocess arguments or objstore paths.  mntroot is
    # IGNORED from the broker message and read from the worker environment
    # instead (same pattern as prot/host/port for objstore above).
    _ALLOWED_REQUEST_TYPES = frozenset(
        {"AP-AFC", "PointAnalysis", "ExclusionZoneAnalysis", "HeatmapAnalysis"})
    if request_type not in _ALLOWED_REQUEST_TYPES:
        LOGGER.error("Rejected broker-supplied request_type=%r — not in "
                     "allowlist; aborting task.", request_type)
        return
    # On the RMQ path the rcache key (req_cfg_digest) binds only
    # (config, request, runtime_opts) and does not include request_type.
    # Restrict to "AP-AFC" to keep engine output consistent with cached
    # entries. afc_server only ever sends "AP-AFC" on this path.
    if rcache_queue is not None and request_type != "AP-AFC":
        LOGGER.error("Rejected broker-supplied request_type=%r on RMQ path "
                     "(rcache_queue=%r) — only 'AP-AFC' is permitted; "
                     "aborting task.", request_type, rcache_queue)
        return
    # Same restriction on the use_tasks path: the /responses/{hash_val}/
    # objstore write (and the _tasks_hash_ok digest that gates it) does not
    # bind request_type, so allowing non-"AP-AFC" here would let a broker
    # attacker overwrite the cached AP-AFC response with wrong-analysis-type
    # engine output. ratafc.py only ever dispatches "AP-AFC" on this path.
    if use_tasks and request_type != "AP-AFC":
        LOGGER.error("Rejected broker-supplied request_type=%r on use_tasks "
                     "path — only 'AP-AFC' is permitted for hash_val-keyed "
                     "objstore caching; aborting task.", request_type)
        return
    # Use the worker's own NFS_MOUNT_PATH env var; ignore broker-supplied mntroot.
    env_mntroot = os.environ.get("NFS_MOUNT_PATH", "")
    if not env_mntroot:
        LOGGER.critical("NFS_MOUNT_PATH not set in worker environment; "
                        "refusing broker-supplied mntroot=%r; aborting task.",
                        mntroot)
        return
    if env_mntroot != mntroot:
        LOGGER.warning("Ignoring broker-supplied mntroot=%r; using "
                       "NFS_MOUNT_PATH=%r from environment.", mntroot, env_mntroot)
        mntroot = env_mntroot
    _orig_runtime_opts = runtime_opts
    # Clamp runtime_opts to the server-side bitmask of legitimate flags to
    # prevent the broker from enabling DBG/SLOW_DBG to redirect history_dir.
    _ALLOWED_RUNTIME_OPT_MASK = (
        defs.RNTM_OPT_AFCENGINE_HTTP_IO | defs.RNTM_OPT_NOCACHE |
        defs.RNTM_OPT_GUI | defs.RNTM_OPT_CERT_ID)
    if runtime_opts & ~_ALLOWED_RUNTIME_OPT_MASK:
        LOGGER.warning("Clearing unknown runtime_opts bits %#x from "
                       "broker-supplied value %#x.",
                       runtime_opts & ~_ALLOWED_RUNTIME_OPT_MASK, runtime_opts)
        runtime_opts = runtime_opts & _ALLOWED_RUNTIME_OPT_MASK
    # Restrict history_dir to a known safe prefix. _validate_broker_path
    # URL-decodes, normalises dot-segments, and rejects '..' segments — all
    # three checks must be applied together to close every encoding bypass.
    # Setting history_dir to None on failure (rather than only clearing flags)
    # ensures downstream code cannot inadvertently use an invalid path even if
    # the runtime_opts guard is later refactored.
    history_dir = _validate_broker_path(history_dir, "/history/", "history_dir")
    if history_dir is None:
        runtime_opts = runtime_opts & ~(defs.RNTM_OPT_DBG | defs.RNTM_OPT_SLOW_DBG)
    # Validate hash_val as a hex digest so it is safe to use as an objstore
    # path component.
    _HEX_RE = re.compile(r"[0-9a-fA-F]{32,128}\Z")
    if not _HEX_RE.match(hash_val or ""):
        LOGGER.error("Rejected hash_val — unexpected format; aborting task.")
        return
    # Validate config_path against its expected shape to ensure it stays
    # within the intended objstore subtree. Reject `..` segments explicitly
    # ([^/]+ alone permits them).
    if config_path is not None and (
            not re.fullmatch(
                r"/afc_config(/[^/]+){1,2}/afc_config\.json", config_path)
            or ".." in config_path.split("/")):
        LOGGER.error("Rejected config_path=%r — unexpected format; aborting task.",
                     config_path)
        return
    # In RMQ-sync mode the engine config MUST come from objstore (operator
    # trust domain) via config_path written by afc_server_compute.py before
    # dispatch.  Reject broker-supplied config_str fallback — it crosses the
    # broker/operator trust boundary and lets a queue publisher feed arbitrary
    # *Dir/*File paths to importConfigAFCjson().
    if not use_tasks and config_path is None:
        LOGGER.error("Rejected RMQ-sync task: config_path not set — refusing "
                     "broker-supplied config_str (engine config must come "
                     "from objstore); aborting task.")
        return
    # Validate task_id as a UUID-shaped token regardless of use_tasks so it is
    # safe to use as an objstore path component (tmp_objdir below).
    if not re.fullmatch(r"[0-9a-fA-F-]{36}", str(task_id or "")):
        LOGGER.error("Rejected task_id — unexpected format; aborting task.")
        return

    proc = None
    try:
        tmpdir = tempfile.mkdtemp(prefix="afc_worker_")

        # Use the no-arg DataIf() constructor to read host/port from the
        # worker's own environment (AFC_OBJST_HOST/PORT/SCHEME). The Celery
        # task kwargs prot/host/port are ignored; object-storage config comes
        # from the environment only.
        dataif = DataIf()
        if use_tasks:
            tsk = afctask.Task(task_id, dataif, hash_val, history_dir)

        err_path = os.path.join(tmpdir, "engine-error.txt")
        err_file = open(err_path, "wb")
        log_file = open(os.path.join(tmpdir, "engine-log.txt"), "wb")
        streams_closed = False

        if use_tasks:
            # Download request and config from ObjStore to local tmpdir so the
            # C++ engine can read them as plain files (the engine's Qt HTTP
            # client does not send the Bearer token required by objst).
            analysis_request_path = os.path.join(tmpdir, "analysisRequest.json")
            analysis_config_path = os.path.join(tmpdir, "afc_config.json")
            analysis_response_path = os.path.join(tmpdir,
                                                  "analysisResponse.json.gz")
            with dataif.open(os.path.join("/responses", hash_val,
                                          "analysisRequest.json")) as hfile:
                _req_bytes = hfile.read()
            # Defence-in-depth visibility: log if openAfc.overrideAfcConfig
            # is present in the objstore-sourced request.  Legitimate callers
            # hold AFC_PRECOMPUTE_TOKEN/AFC_DISPATCHER_TOKEN (S0117-04 gate);
            # unexpected presence indicates a possible entry-route bypass.
            # Do NOT modify _req_bytes here — the digest check below must hash
            # the same bytes the entry-route folded into hash_val.
            try:
                import json as _json
                _req_obj = _json.loads(_req_bytes)
                if any(
                    _ve.get("extensionId") == "openAfc.overrideAfcConfig"
                    for _inq in _req_obj.get(
                        "availableSpectrumInquiryRequests", [])
                    for _ve in _inq.get("vendorExtensions", [])
                ):
                    LOGGER.warning(
                        "openAfc.overrideAfcConfig present in objstore "
                        "request hash_val=%s; verify entry-route token gate.",
                        hash_val)
            except Exception:
                pass
            with open(analysis_request_path, "wb") as outfile:
                outfile.write(_req_bytes)
            with dataif.open(config_path) as hfile:
                _cfg_bytes = hfile.read()
            with open(analysis_config_path, "wb") as outfile:
                outfile.write(_cfg_bytes)
            # Bind broker-supplied runtime_opts to objstore-derived
            # request/config; require digest match before writing to
            # /responses/{hash_val}/ so only tasks whose runtime_opts
            # were folded into hash_val by RequestConfigHash may write.
            # Use the PRE-clamp runtime_opts: that is what ratafc.py /
            # afc_server folded into hash_val.
            _tasks_hash_ok = _verify_req_cfg_digest(
                hash_val=hash_val,
                original_request_str=_req_bytes.decode("utf-8"),
                request_str=_req_bytes.decode("utf-8"),
                config_str=_cfg_bytes.decode("utf-8"),
                runtime_opts=_orig_runtime_opts,
                use_tasks=False)
            # The digest above binds message CONTENT (request/config/
            # runtime_opts) but NOT the task_id write key. Without this
            # check a broker-position attacker could republish a
            # digest-valid (hash_val, config_path, runtime_opts) tuple with
            # a victim's task_id and launder worker-credentialed objstore
            # writes onto /responses/{task_id}/* — cross-task result
            # substitution. Bind task_id to hash_val via the dispatch-time
            # status.json record (written by build_task()/afctask.Task
            # before the Celery message is published; a broker-position
            # attacker cannot forge it because objstore writes require the
            # worker's own Bearer credential).
            if _tasks_hash_ok:
                _dispatch_stat = None
                try:
                    with dataif.open(
                            os.path.join("/responses", task_id,
                                         "status.json")) as hfile:
                        _dispatch_stat = json.loads(hfile.read())
                except Exception:  # noqa: BLE001
                    _dispatch_stat = None
                if not isinstance(_dispatch_stat, dict) or \
                        _dispatch_stat.get("hash") != hash_val:
                    LOGGER.error(
                        "task_id %s is not bound to hash_val %s by a "
                        "dispatch-time status.json — refusing task_id-keyed "
                        "objstore writes.", task_id, hash_val)
                    _tasks_hash_ok = False
        else:
            analysis_request_path = os.path.join(tmpdir,
                                                 "analysisRequest.json")
            analysis_config_path = os.path.join(tmpdir, "afc_config.json")
            analysis_response_path = os.path.join(tmpdir,
                                                  "analysisResponse.json.gz")
            # config_path is guaranteed non-None here: the guard at line
            # 361-365 aborts the task before this point if config_path is None.
            # Engine config always comes from the operator-trusted objstore path.
            with dataif.open(config_path) as hfile:
                engine_config_bytes = hfile.read()
            with open(analysis_config_path, "wb") as outfile:
                outfile.write(engine_config_bytes)
            # _verify_req_cfg_digest must hash the same bytes the engine
            # will consume (objstore content), not the message-supplied
            # config_str, which may differ from what was written.
            engine_config_str = engine_config_bytes.decode("utf-8")
            with open(analysis_request_path, "w", encoding="utf-8") as outfile:
                outfile.write(request_str)

        if use_tasks and _tasks_hash_ok:
            tsk.toJson(afctask.Task.STAT_PROGRESS, runtime_opts=runtime_opts)

        tmp_objdir = os.path.join("/responses", task_id)
        retcode = 0
        success = False
        timeout_expired = False
        error_msg = ""

        # run the AFC Engine
        try:
            timeout = deadline - time.time()
            if timeout <= 0:
                error_msg += "AFC processing deadline expired"
                timeout_expired = True
                raise subprocess.CalledProcessError(retcode, [])
            cmd = [
                conf.AFC_ENGINE,
                "--request-type=" + request_type,
                "--state-root=" + mntroot + "/rat_transfer",
                "--mnt-path=" + mntroot,
                "--input-file-path=" + analysis_request_path,
                "--config-file-path=" + analysis_config_path,
                "--output-file-path=" + analysis_response_path,
                "--temp-dir=" + tmpdir,
                "--log-level=" + conf.AFC_ENGINE_LOG_LVL,
                "--runtime_opt=" + str(runtime_opts),
            ]
            LOGGER.debug(cmd)
            retcode = 0
            _mem_lim = conf.AFC_ENGINE_MEM_LIMIT
            proc = subprocess.Popen(
                cmd, stderr=err_file, stdout=log_file,
                start_new_session=True,
                preexec_fn=(
                    lambda lim=_mem_lim: resource.setrlimit(
                        resource.RLIMIT_DATA, (lim, lim))
                ) if _mem_lim is not None else None)
            try:
                retcode = proc.wait(timeout=timeout)
            except subprocess.SubprocessError as e:
                timeout_expired = isinstance(e, subprocess.TimeoutExpired)
                error_msg += f"afc-engine failure: {e}"
                raise subprocess.CalledProcessError(retcode, cmd)
            finally:
                # Kill the entire process group to prevent orphaned engine
                # processes when the Celery worker exits unexpectedly.
                if proc.poll() is None:
                    try:
                        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                    except OSError:
                        try:
                            proc.kill()
                        except Exception:
                            pass
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        pass
            if retcode:
                raise subprocess.CalledProcessError(retcode, cmd)
            success = True

        except subprocess.CalledProcessError:
            log_file.close()
            err_file.close()
            streams_closed = True
            with open(err_path, encoding="utf-8", errors="replace") as infile:
                error_msg += infile.read(1000).strip()
            LOGGER.error(
                f"run(): afc-engine crashed. Task ID={task_id}, error "
                f"message:\n{error_msg}")
            try:
                if use_tasks:
                    with dataif.open(os.path.join("/responses", hash_val,
                                                  "analysisRequest.json")) \
                            as hfile:
                        request_str = \
                            hfile.read().decode("utf-8", errors="replace")
                    with dataif.open(config_path) as hfile:
                        config_str = \
                            hfile.read().decode("utf-8", errors="replace")
                req_text = original_request_str or request_str
                cfg_text = config_str
                request_obj = json.loads(req_text) if req_text else None
                config_obj = json.loads(cfg_text) if cfg_text else None
                als.als_initialize()
                als.als_json_log("afc_engine_crash",
                                 {"task_id": task_id, "error_msg": error_msg,
                                  "timeout": timeout_expired,
                                  "request": request_obj,
                                  "config": config_obj})
            except Exception as ex:
                LOGGER.error(
                    f"Failed to make ALS report on engine crash: {ex}")
        else:
            LOGGER.info('finished with task computation')
            if use_tasks and os.path.isfile(analysis_response_path):
                if not _tasks_hash_ok:
                    LOGGER.error(
                        "use_tasks digest mismatch (hash_val=%s, "
                        "runtime_opts=%#x); refusing to write "
                        "/responses/%s/analysisResponse.json.gz",
                        hash_val, _orig_runtime_opts, hash_val)
                    success = False
                else:
                    with open(analysis_response_path, "rb") as infile:
                        with dataif.open(
                                os.path.join("/responses", hash_val,
                                             "analysisResponse.json.gz")) as hfile:
                            hfile.write(infile.read())

        proc = None
        if not streams_closed:
            log_file.close()
            err_file.close()

        if use_tasks and _tasks_hash_ok and (not success) and os.path.isfile(err_path):
            try:
                with dataif.open(os.path.join(tmp_objdir,
                                              "engine-error.txt")) as hfile:
                    with open(err_path, "rb") as infile:
                        hfile.write(infile.read())
            except OSError as oex:
                LOGGER.warning("Could not persist engine-error.txt: %s", oex)

        if not use_tasks:
            try:
                with open(analysis_response_path, "rb") as infile:
                    response_gz = infile.read()
            except OSError:
                response_gz = None
            response_str = \
                zlib.decompress(response_gz,
                                16 + zlib.MAX_WBITS).decode("utf-8") \
                if success and response_gz else None
            # Verify hash_val is consistent with original_request_str
            # + config_str + runtime_opts before writing to rcache.
            # Recompute the expected digest using the same algorithm as
            # RequestConfigHash and refuse to cache if it does not match.
            _hash_verified = _verify_req_cfg_digest(
                hash_val=hash_val,
                original_request_str=original_request_str,
                request_str=request_str,
                config_str=engine_config_str if not use_tasks else config_str,
                runtime_opts=runtime_opts,
                use_tasks=use_tasks)
            if not _hash_verified:
                # Digest mismatch: the computed response was not produced
                # from the (request, config, runtime_opts) triple that
                # hashes to hash_val. Previously this only suppressed the
                # rcache DB update (update_cache=False) while still
                # delivering the wrong-config response to the waiting
                # afc_server future — mirror the use_tasks refusal above and
                # send a failure indication instead of the unverified
                # response.
                LOGGER.error(
                    "rmq digest mismatch (hash_val=%s, runtime_opts=%#x); "
                    "refusing to deliver computed response",
                    hash_val, runtime_opts)
                response_str = None
            get_rcache_client().rmq_send_response(
                queue_name=rcache_queue, req_cfg_digest=hash_val,
                request=original_request_str, response=response_str,
                update_cache=_hash_verified and
                not bool(runtime_opts & defs.RNTM_OPT_NOCACHE))

        # GUI artifacts are keyed on broker-supplied task_id; require the
        # same objstore-derived digest binding as the hash_val path above
        # before writing to /responses/{task_id}/.
        if (runtime_opts & defs.RNTM_OPT_GUI) and use_tasks and _tasks_hash_ok:
            for fname in ("results.kmz", "mapData.json.gz"):
                # copy if generated
                if os.path.exists(os.path.join(tmpdir, fname)):
                    with dataif.open(os.path.join(tmp_objdir, fname)) as hfile:
                        with open(os.path.join(tmpdir, fname), "rb") as infile:
                            hfile.write(infile.read())

        # copy contents of temporary directory to history directory
        if runtime_opts & (defs.RNTM_OPT_DBG | defs.RNTM_OPT_SLOW_DBG):
            for fname in os.listdir(tmpdir):
                with dataif.open(os.path.join(history_dir, fname)) as hfile:
                    with open(os.path.join(tmpdir, fname), "rb") as infile:
                        hfile.write(infile.read())

        LOGGER.debug('task completed')
        if use_tasks and _tasks_hash_ok:
            tsk.toJson(
                afctask.Task.STAT_SUCCESS if success
                else afctask.Task.STAT_FAILURE,
                runtime_opts=runtime_opts, exit_code=retcode)

    except Exception as exc:
        raise exc

    finally:
        LOGGER.info('Terminating worker')

        # we may be being told to stop worker so we have to terminate C++ code
        # if it is running
        if proc:
            LOGGER.debug('terminating afc-engine')
            proc.terminate()
            LOGGER.debug('afc-engine terminated')
        if os.path.exists(tmpdir):
            shutil.rmtree(tmpdir)
        LOGGER.info('Worker resources cleaned up')
