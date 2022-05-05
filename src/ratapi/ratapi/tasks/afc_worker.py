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
from inspect import cleandoc
import logging
import os
import sys
import shutil
import subprocess
import datetime
from celery import Celery
from ..db.daily_uls_parse import daily_uls_parse
from ..defs import RNTM_OPT_DBG
from runcelery import init_config
from celery.schedules import crontab
from flask.config import Config
from celery.utils.log import get_task_logger
from celery.exceptions import Ignore

LOGGER = get_task_logger(__name__)


APP_CONFIG = init_config(Config(root_path=None))
LOGGER.info('Celery Backend: %s', APP_CONFIG['CELERY_RESULT_BACKEND'])
LOGGER.info('Celery Broker: %s', APP_CONFIG['BROKER_URL'])

#: constant celery reference. Configure once flask app is created
client = Celery(
    'fbrat',
    backend=APP_CONFIG['CELERY_RESULT_BACKEND'],
    broker=APP_CONFIG['BROKER_URL'],
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
   

@client.task(bind=True)
def run(self, user_id, username, afc_exe, state_root, temp_dir, request_type,
        request_file_path, config_file_path, response_file_path, history_dir,
        runtime_opts):
    """ Run AFC Engine

        The parameters are all serializable so they can be passed through the message queue.

        :param user_id: Id of calling user

        :param username: username of calling user

        :param afc_exe: path to AFC Engine executable

        :param state_root: path to directory where fbrat state is held

        :param temp_dir: path to working directory where process can write files. Will be saved as history if history_dir is not None.

        :param request_type: request type of analysis
        :type request_type: str

        :param request_file_path: path to json file with parameters for analysis

        :param config_file_path: path to json file with AFC Config parameters

        :param response_file_path: path to json file that will be created as output

        :param history_dir: path to history directory if values in temp_dir are to be saved
        :type history_dir: path or None

        :param runtime_opts: indicates if AFC Engine should use DEBUG mode
         (uses INFO otherwise) or prepare GUI options
        :type runtime_opts: int
    """
    proc = None
    response_dir = os.path.join(state_root, 'responses')
    try:
        self.update_state(state='PROGRESS',
                          meta={
                              'progress_file': os.path.join(temp_dir, 'progress.txt'),
                              'user_id': user_id
                          })

        LOGGER.debug("entering run function")
        err_file = open(os.path.join(temp_dir, 'engine-error.txt'), 'wb')
        log_file = open(os.path.join(temp_dir, 'engine-log.txt'), 'wb')

        debug = ''
        if runtime_opts & RNTM_OPT_DBG:
            debug = True

        # run the AFC Engine
        try:
            LOGGER.debug('running afc engine')
            cmd = [
                afc_exe,
                "--request-type=" + request_type,
                "--state-root=" + state_root,
                "--input-file-path=" + request_file_path,
                "--config-file-path=" + config_file_path,
                "--output-file-path=" + response_file_path,
                "--temp-dir=" + temp_dir,
                "--log-level=" + ("debug" if debug else "info"),
                "--runtime_opt=" + str(runtime_opts),
            ]
            proc = subprocess.Popen(cmd, stderr=err_file, stdout=log_file)

            retcode = proc.wait()
            if retcode:
                raise subprocess.CalledProcessError(retcode, cmd)

        except subprocess.CalledProcessError as error:
            proc = None
            err_file.close()  # close the file just in case it is still open from writing
            log_file.close()

            shutil.copy2(os.path.join(temp_dir, 'engine-error.txt'),
                         os.path.join(response_dir, self.request.id + ".error"))

            LOGGER.debug("copied error file from "+os.path.join(temp_dir, 'engine-error.txt')+" to "+os.path.join(response_dir, self.request.id + ".error"))
            # store error file in history dir so it can be seen via WebDav
            if history_dir is not None:
                LOGGER.debug("Moving temp files to history: %s",
                            str(os.listdir(temp_dir)))
                shutil.move(
                    temp_dir,
                    os.path.join(history_dir, username + '-' + str(datetime.datetime.now().isoformat())))
                LOGGER.debug("Created history folder "+os.path.join(history_dir, username + '-' + str(datetime.datetime.now().isoformat())))
                
            return {
                'status': 'ERROR',
                'user_id': user_id,
                'error_path': os.path.join(response_dir, self.request.id + '.error'),
                'exit_code': error.returncode,
            }

        LOGGER.info('finished with task computation')
        proc = None
        log_file.close()
        err_file.close()
        # store result file in /var/lib/fbrat/responses/{task_id}.json file
        additional_paths = dict()
        shutil.copy2(response_file_path, os.path.join(
            response_dir, self.request.id + '.json'))

        # copy kml result if it was produced by AFC Engine
        if 'results.kmz' in os.listdir(temp_dir) and request_type not in ['AP-AFC', 'APAnalysis']:
            shutil.copy2(os.path.join(temp_dir, 'results.kmz'),
                         os.path.join(response_dir, self.request.id + '.kmz'))
            additional_paths['kml_path'] = os.path.join(
                response_dir, self.request.id + '.kmz')

        # copy contents of temporary directory to history directory
        if history_dir is not None:
            # remove error file since we completed successfully
            os.remove(os.path.join(temp_dir, 'engine-error.txt'))
            LOGGER.debug("Moving temp files to history: %s",
                         str(os.listdir(temp_dir)))
            shutil.move(
                temp_dir,
                os.path.join(history_dir, username + '-' + str(datetime.datetime.now().isoformat())))
            LOGGER.debug("Created history folder "+os.path.join(history_dir, username + '-' + str(datetime.datetime.now().isoformat())))

        LOGGER.debug('task completed')
        result = {
            'status': 'DONE',
            'user_id': user_id,
            'result_path': os.path.join(response_dir, self.request.id + '.json'),
        }
        result.update(additional_paths)
        return result

    except Exception as e:
        raise e

    finally:
        LOGGER.info('Terminating worker')

        # we may be being told to stop worker so we have to terminate C++ code if it is running
        if proc:
            LOGGER.debug('terminating afc-engine')
            proc.terminate()
            LOGGER.debug('afc-engine terminated')

        if os.path.exists(temp_dir):
            LOGGER.debug('cleaning up')
            shutil.rmtree(temp_dir)
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

