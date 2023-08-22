#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
import os
import subprocess
import shutil
import tempfile
import zlib
from celery import Celery
from celery.utils.log import get_task_logger
from appcfg import BrokerConfigurator
from fst import DataIf
import defs
import afctask
from rcache_models import RcacheClientSettings
from rcache_client import ReqCacheClient

LOGGER = get_task_logger(__name__)

class WorkerConfig(BrokerConfigurator):
    """Worker internal config"""
    def __init__(self):
        BrokerConfigurator.__init__(self)
        self.AFC_ENGINE = os.getenv("AFC_ENGINE")
        self.AFC_ENGINE_LOG_LVL = os.getenv("AFC_ENGINE_LOG_LVL", "info")
        self.AFC_WORKER_CELERY_LOG = os.getenv("AFC_WORKER_CELERY_LOG")
        # worker task engine timeout
        # the default value predefined by image environment (Dockefile)
        self.AFC_WORKER_ENG_TOUT = os.getenv("AFC_WORKER_ENG_TOUT")

conf = WorkerConfig()

rcache_settings = RcacheClientSettings()
# In this validation rcache is True to handle 'update_on_send' case
rcache_settings.validate_for(rmq=True, rcache=True)
rcache = ReqCacheClient(rcache_settings, rmq_receiver=False) \
    if rcache_settings.enabled else None

LOGGER.info('Celery Broker: %s', conf.BROKER_URL)

#: constant celery reference. Configure once flask app is created
client = Celery(
    'fbrat',
    broker=conf.BROKER_URL,
    task_ignore_result=True,
)

@client.task(ignore_result=True)
def run(prot, host, port, request_type, task_id, hash_val,
        config_path, history_dir, runtime_opts, mntroot, rcache_queue,
        request_str, config_str):
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

        :param config_str None for task-based synchronization, config text for RMQ-based synchronization
    """
    LOGGER.debug(f"run(prot={prot}, host={host}, port={port}, "
                 f"task_id={task_id}, hash={hash_val}, opts={runtime_opts}, "
                 f"mntroot={mntroot}, timeout={conf.AFC_WORKER_ENG_TOUT}, "
                 f"rcache_queue={rcache_queue}")

    use_tasks = rcache_queue is None
    if use_tasks:
        assert task_id and config_path and \
            (runtime_opts & defs.RNTM_OPT_AFCENGINE_HTTP_IO)
    else:
        assert rcache and request_str and config_str and \
            (not (runtime_opts & defs.RNTM_OPT_AFCENGINE_HTTP_IO))

    proc = None
    try:
        tmpdir = tempfile.mkdtemp(prefix="afc_worker_")

        dataif = DataIf(prot, host, port)
        if use_tasks:
            tsk = afctask.Task(task_id, dataif, hash_val, history_dir)
            tsk.toJson(afctask.Task.STAT_PROGRESS, runtime_opts=runtime_opts)

        err_file = open(os.path.join(tmpdir, "engine-error.txt"), "wb")
        log_file = open(os.path.join(tmpdir, "engine-log.txt"), "wb")

        if use_tasks:
            # pathes in objstorage
            analysis_request_path = \
                dataif.rname(os.path.join("/responses", hash_val,
                                          "analysisRequest.json"))
            analysis_config_path = dataif.rname(config_path)
            analysis_response_path = \
                dataif.rname(os.path.join("/responses", hash_val,
                                          "analysisResponse.json.gz"))
        else:
            analysis_request_path = os.path.join(tmpdir,
                                                 "analysisRequest.json")
            analysis_config_path = os.path.join(tmpdir, "afc_config.json")
            analysis_response_path = os.path.join(tmpdir,
                                                  "analysisResponse.json.gz")
            for fname, data in [(analysis_request_path, request_str),
                                 (analysis_config_path, config_str)]:
                with open(fname, "w", encoding="utf-8") as outfile:
                    outfile.write(data)

        tmp_objdir = os.path.join("/responses", task_id)
        retcode = 0
        success = False

        # run the AFC Engine
        try:
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
            proc = subprocess.Popen(cmd, stderr=err_file, stdout=log_file)
            try:
                retcode = proc.wait(timeout=int(conf.AFC_WORKER_ENG_TOUT))
            except Exception as e:
                LOGGER.error(f"run(): afc-engine timeout "
                             f"{conf.AFC_WORKER_ENG_TOUT} error {type(e)}")
                raise subprocess.CalledProcessError(retcode, cmd)
            if retcode:
                raise subprocess.CalledProcessError(retcode, cmd)
            success = True

        except subprocess.CalledProcessError as error:
            LOGGER.error("run(): afc-engine error")

            with dataif.open(os.path.join(tmp_objdir, "engine-error.txt")) as hfile:
                with open(os.path.join(tmpdir, "engine-error.txt"), "rb") as infile:
                    hfile.write(infile.read())
        else:
            LOGGER.info('finished with task computation')

        proc = None
        log_file.close()
        err_file.close()

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
            rcache.rmq_send_response(
                queue_name=rcache_queue, req_cfg_digest=hash_val,
                request=request_str, response=response_str)

        if runtime_opts & defs.RNTM_OPT_GUI:
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
        if use_tasks:
            tsk.toJson(
                afctask.Task.STAT_SUCCESS if success
                else afctask.Task.STAT_FAILURE,
                runtime_opts=runtime_opts, exit_code=retcode)

    except Exception as exc:
        raise exc

    finally:
        LOGGER.info('Terminating worker')

        # we may be being told to stop worker so we have to terminate C++ code if it is running
        if proc:
            LOGGER.debug('terminating afc-engine')
            proc.terminate()
            LOGGER.debug('afc-engine terminated')
        if os.path.exists(tmpdir):
            shutil.rmtree(tmpdir)
        if rcache:
            rcache.disconnect()
        LOGGER.info('Worker resources cleaned up')
