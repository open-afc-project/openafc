#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
from genericpath import exists
import logging
import os
import sys
import subprocess
import datetime
import shutil
from celery import Celery
from ..db.daily_uls_parse import daily_uls_parse
from runcelery import init_config
from celery.schedules import crontab
from flask.config import Config
from celery.utils.log import get_task_logger
from celery.exceptions import Ignore
from .. import defs
from .. import data_if
from .. import task

LOGGER = get_task_logger(__name__)

APP_CONFIG = init_config(Config(root_path=None))
LOGGER.info('Celery Broker: %s', APP_CONFIG['BROKER_URL'])

#: constant celery reference. Configure once flask app is created
client = Celery(
    'fbrat',
    # Remove backend when periodic task moved to Jenkins
    backend=APP_CONFIG['CELERY_RESULT_BACKEND'],
    broker=APP_CONFIG['BROKER_URL'],
    # Enable the following flags when periodic task moved to Jenkins
    #task_ignore_result=True,
    #task_store_errors_even_if_ignored=False
)

client.conf.update(APP_CONFIG)


@client.on_after_configure.connect
def setup_periodic_tasks(sender, **kwargs):
    # Check every minute if the daily parse time has passed
    sender.add_periodic_task(
        crontab(),
        checkParseTime.apply_async(args=[]),
    )
    # reset daily flag every day at midnight
    sender.add_periodic_task(
        crontab(minute=0, hour=0),
        resetDailyFlag.apply_async(args=[]),
    )


@client.task(ignore_result=True)
def run(prot, host, port, state_root,
        afc_exe, request_type, debug,
        task_id, hash, user_id, history_dir,
        runtime_opts):
    """ Run AFC Engine

        The parameters are all serializable so they can be passed through the message queue.

        :param user_id: Id of calling user

        :param history_dir: history directory suffix

        :param afc_exe: path to AFC Engine executable

        :param state_root: path to directory where fbrat state is held

        :param request_type: request type of analysis
        :type request_type: str

        :param runtime_opts: indicates if AFC Engine should use DEBUG mode
         (uses INFO otherwise) or prepare GUI options
        :type runtime_opts: int

        :param debug: log level of afc-engine

        :param hash: md5 of request and config files
    """
    LOGGER.debug('run(prot={}, host={}, port={}, state_root={}, task_id={}, hash={}, runtime_opts={})'.
                 format(prot, host, port, state_root, task_id, hash, runtime_opts))

    tmpdir = os.path.join(state_root, task_id)
    os.makedirs(tmpdir)

    dataif = data_if.DataIf(prot, host, port, state_root)
    t = task.Task(task_id, dataif, state_root, hash, user_id, history_dir)
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
                "--state-root=" + state_root,
                "--input-file-path=" + dataif.rname("pro", hash + "/analysisRequest.json"),
                "--config-file-path=" + dataif.rname("cfg", str(user_id) + "/afc_config.json"),
                "--output-file-path=" + dataif.rname("pro", hash + "/analysisResponse.json.gz"),
                "--progress-file-path=" + dataif.rname("pro", task_id + "/progress.txt"),
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


@client.task(bind=True)
def parseULS(self, state_path = "/var/lib/fbrat", isManual = False):
    task_id = str(self.request.id)
    # only allow one manual at a time
    if isManual:
        datapath = state_path + '/daily_uls_parse/data_files/currentManualId.txt'
        with open(datapath, 'r+') as data_file:
            id_in_progress = data_file.read()
            if(id_in_progress == "" or id_in_progress == None or id_in_progress == '\n'):
                data_file.write(task_id)
                LOGGER.debug('Setting manual parser to %s', str(task_id))
            else:
                LOGGER.error('Manual parse already in progress')
                self.update_state(state = 'REVOKED')
                raise Ignore('Manual parse already in progress')
    self.update_state(state='PROGRESS')
    result = daily_uls_parse(state_path, False)
    self.update_state(state='DONE')
    LOGGER.debug('Freeing up manual parse worker')
    return result

@client.task(bind=True)
def checkParseTime(self):
    print("Starting check parse time")
    verifyUlsParseFolders(APP_CONFIG["STATE_ROOT_PATH"])
    datapath = APP_CONFIG["STATE_ROOT_PATH"] + '/daily_uls_parse/data_files/nextRun.txt'
    nextRun = ''

    with open(datapath, 'r') as data_file:
        # string is stored as "<hours>:<mins>"
        nextRun = data_file.read().split(':')
    hours = int(nextRun[0])
    mins = int(nextRun[1])
    LOGGER.debug('Checking parse time for hours: ' + str(hours) + ' mins: ' + str(mins))
    timeNow = datetime.datetime.utcnow()
    LOGGER.debug('Current time: ' + timeNow.isoformat())
    timeToParse = datetime.datetime.utcnow().replace(hour=hours, minute=mins)
    timeDiff = timeNow - timeToParse
    LOGGER.debug('Desired parse time: ' + timeToParse.isoformat())
    # if the time now is past the time to parse, start a parse
    if timeDiff.total_seconds() >= 0:
        if client.conf["DAILY_ULS_RAN_TODAY"]:
            LOGGER.debug('Parse already started/complete today')
            return "ALREADY PARSED"
        else:
            # daily uls parse
            client.conf["DAILY_ULS_RAN_TODAY"] = True
            LOGGER.debug('Starting automated daily parse')
            parseULS.apply_async(args=[APP_CONFIG["STATE_ROOT_PATH"]])
            return "STARTED PARSE"

    else:
        LOGGER.debug("Time to parse not yet reached")
        return "TOO SOON TO PARSE"

        

@client.task(bind=True)
def resetDailyFlag(self):
    LOGGER.debug('Setting parse flag to false')
    client.conf["DAILY_ULS_RAN_TODAY"] = False

def verifyUlsParseFolders(rootDir):
    # verify there is a daily_uls_parse directory
    dailyParseDir= rootDir + '/daily_uls_parse'
    dataFilesDir = dailyParseDir+'/data_files'
    dataFileNames = {
        dataFilesDir+'/currentManualId.txt':"",
        dataFilesDir+'/fsid_table.csv':"",
        dataFilesDir+'/lastFSID.txt':"1",
        dataFilesDir+'/lastSuccessfulRun.txt':"",
        dataFilesDir+'/nextRun.txt':"00:00"
    }

    LOGGER.debug('checking for next run file in '+dailyParseDir)

    if(not os.path.exists(dailyParseDir)):
        LOGGER.debug('adding '+dailyParseDir)
        os.makedirs(dailyParseDir)

    if(not os.path.exists(dataFilesDir)):
        LOGGER.debug('adding '+dataFilesDir)
        os.makedirs(dataFilesDir)

    for fname in dataFileNames:
        if not exists(fname):
            LOGGER.debug('adding '+fname)
            with open(fname, 'w+') as f:
                f.write(dataFileNames[fname])

