#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
import os
import subprocess
import distutils.spawn
import shutil
from celery import Celery
from celery.utils.log import get_task_logger
from .. import defs
from .. import data_if
from .. import task
from .. import config

LOGGER = get_task_logger(__name__)

AFC_AEP_ENABLE = True if "AFC_AEP_ENABLE" in os.environ else False

BROKER_URL=os.getenv('BROKER_PROT', config.BROKER_DEFAULT_PROT) + \
           "://" + \
           os.getenv('BROKER_USER', config.BROKER_DEFAULT_USER) + \
           ":" + \
           os.getenv('BROKER_PWD', config.BROKER_DEFAULT_PWD) + \
           "@" + \
           os.getenv('BROKER_FQDN', config.BROKER_DEFAULT_FQDN) + \
           ":" + \
           os.getenv('BROKER_PORT', config.BROKER_DEFAULT_PORT) + \
           "/fbrat"

LOGGER.info('Celery Broker: %s', BROKER_URL)
#: AFC Engine executable
AFC_ENGINE = distutils.spawn.find_executable('afc-engine')

#: constant celery reference. Configure once flask app is created
client = Celery(
    'fbrat',
    broker=BROKER_URL,
    task_ignore_result=True,
)

@client.task(ignore_result=True)
def run(prot, host, port, state_root,
        afc_exe, request_type, debug,
        task_id, hash, region, history_dir,
        runtime_opts, mntroot):
    """ Run AFC Engine

        The parameters are all serializable so they can be passed through the message queue.

        :param region: region of ap

        :param history_dir: history directory suffix

        :param afc_exe: path to AFC Engine executable

        :param state_root: path to directory where fbrat state is held

        :param mntroot: path to directory where GeoData and config data are stored

        :param request_type: request type of analysis
        :type request_type: str

        :param runtime_opts: indicates if AFC Engine should use DEBUG mode
         (uses INFO otherwise) or prepare GUI options
        :type runtime_opts: int

        :param debug: log level of afc-engine

        :param hash: md5 of request and config files
    """
    LOGGER.debug('run(prot={}, host={}, port={}, state_root={}, task_id={}, hash={}, runtime_opts={}, mntroot={})'.
                 format(prot, host, port, state_root, task_id, hash, runtime_opts, mntroot))

    if isinstance(afc_exe, type(None)):
        afc_exe = AFC_ENGINE
        if not os.path.exists(AFC_ENGINE):
            LOGGER.error('Missing afc-engine executable "%s"', AFC_ENGINE)
    if AFC_AEP_ENABLE is True:
        afc_exe = "/usr/bin/afc-engine.sh"

    tmpdir = os.path.join(state_root, task_id)
    os.makedirs(tmpdir)
    probeHttps=None

    dataif = data_if.DataIf(prot, host, port, state_root, probeHttps, mntroot)
    t = task.Task(task_id, dataif, state_root, hash, region, history_dir, mntroot)
    t.toJson(task.Task.STAT_PROGRESS, runtime_opts=runtime_opts)

    proc = None
    err_file = open(os.path.join(tmpdir, "engine-error.txt"), "wb")
    log_file = open(os.path.join(tmpdir, "engine-log.txt"), "wb")

    try:
        # run the AFC Engine
        try:
            cmd = [
                afc_exe,
                "--request-type=" + request_type,
                "--state-root=" + mntroot+"/rat_transfer",
                "--mnt-path=" + mntroot,
                "--input-file-path=" + dataif.rname("pro", hash + "/analysisRequest.json"),
                "--config-file-path=" + dataif.rname("cfg", region + "/afc_config.json"),
                "--output-file-path=" + dataif.rname("pro", hash + "/analysisResponse.json.gz"),
                "--temp-dir=" + tmpdir,
                "--log-level=" + ("debug" if debug else "info"),
                "--runtime_opt=" + str(runtime_opts),
            ]
            LOGGER.debug(cmd)
            proc = subprocess.Popen(cmd, stderr=err_file, stdout=log_file)

            retcode = proc.wait()
            if retcode:
                raise subprocess.CalledProcessError(retcode, cmd)

        except subprocess.CalledProcessError as error:
            LOGGER.error("run(): afc-egine error")
            proc = None
            err_file.close()  # close the file just in case it is still open from writing
            log_file.close()

            with dataif.open("pro", task_id + "/engine-error.txt") as hfile:
                with open(os.path.join(tmpdir, "engine-error.txt"), "rb") as infile:
                    hfile.write(infile.read())

            # store error file in history dir so it can be seen via WebDav
            if runtime_opts & defs.RNTM_OPT_DBG:
                for fname in os.listdir(tmpdir):
                    # request, responce and config are archived from ratapi
                    if (fname != "analysisRequest.json" and fname != "afc_config.json"):
                        with dataif.open("dbg", history_dir + "/" + fname) as hfile:
                            with open(os.path.join(tmpdir, fname), "rb") as infile:
                                hfile.write(infile.read())
            elif runtime_opts & defs.RNTM_OPT_SLOW_DBG:
                if os.path.exists(os.path.join(tmpdir, "eirp.csv.gz")):
                    with dataif.open("dbg", history_dir + "/eirp.csv.gz") as hfile:
                        with open(os.path.join(tmpdir, "eirp.csv.gz"), "rb") as infile:
                            hfile.write(infile.read())

            t.toJson(task.Task.STAT_FAILURE, runtime_opts=runtime_opts, exit_code=error.returncode)
            return

        LOGGER.info('finished with task computation')
        proc = None
        log_file.close()
        err_file.close()

        if runtime_opts & defs.RNTM_OPT_GUI:
            # copy kml result if it was produced by AFC Engine
            if os.path.exists(os.path.join(tmpdir, "results.kmz")):
                with dataif.open("pro", task_id + "/results.kmz") as hfile:
                    with open(os.path.join(tmpdir, "results.kmz"), "rb") as infile:
                        hfile.write(infile.read())
            # copy geoJson file if generated
            if os.path.exists(os.path.join(tmpdir, "mapData.json.gz")):
                with dataif.open("pro", task_id + "/mapData.json.gz") as hfile:
                    with open(os.path.join(tmpdir, "mapData.json.gz"), "rb") as infile:
                        hfile.write(infile.read())


        # copy contents of temporary directory to history directory
        if runtime_opts & defs.RNTM_OPT_DBG:
            # remove error file since we completed successfully
            for fname in os.listdir(tmpdir):
                # request, response and config are archived from ratapi
                if (fname != "analysisRequest.json" and fname != "afc_config.json"
                    and fname != "analysisResponse.json.gz"):
                    with dataif.open("dbg", history_dir + "/" + fname) as hfile:
                        with open(os.path.join(tmpdir, fname), "rb") as infile:
                            hfile.write(infile.read())
        elif runtime_opts & defs.RNTM_OPT_SLOW_DBG:
            if os.path.exists(os.path.join(tmpdir, "eirp.csv.gz")):
                with dataif.open("dbg", history_dir + "/eirp.csv.gz") as hfile:
                    with open(os.path.join(tmpdir, "eirp.csv.gz"), "rb") as infile:
                        hfile.write(infile.read())

        LOGGER.debug('task completed')
        t.toJson(task.Task.STAT_SUCCESS, runtime_opts=runtime_opts)

    except Exception as e:
        raise e

    finally:
        LOGGER.info('Terminating worker')

        # we may be being told to stop worker so we have to terminate C++ code if it is running
        if proc:
            LOGGER.debug('terminating afc-engine')
            proc.terminate()
            LOGGER.debug('afc-engine terminated')
        if os.path.exists(tmpdir):
            shutil.rmtree(tmpdir)
        LOGGER.info('Worker resources cleaned up')
