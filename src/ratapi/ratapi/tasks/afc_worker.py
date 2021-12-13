from inspect import cleandoc
import logging
import os
import sys
import shutil
import subprocess
import datetime
from celery import Celery
from ..db.daily_uls_parse import daily_uls_parse
from runcelery import init_config
from celery.schedules import crontab
from flask.config import Config
from celery.utils.log import get_task_logger

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
    hours = APP_CONFIG["DAILY_ULS_HOURS"]
    mins = APP_CONFIG["DAILY_ULS_MINS"]
    # daily uls parse
    sender.add_periodic_task(
        crontab(hour=hours, minute=mins),
        parseULS.apply_async(APP_CONFIG["STATE_ROOT_PATH"]),
    )



@client.task(bind=True)
def run(self, user_id, username, afc_exe, state_root, temp_dir, request_type, request_file_path, config_file_path, response_file_path, history_dir, debug):
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

        :param debug: indicates if AFC Engine should use DEBUG mode (uses INFO otherwise)
        :type debug: boolean
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

            # store error file in responses dir
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
            shutil.rmtree(temp_dir)
        LOGGER.info('Worker resources cleaned up')


# # Calls uls parse every day
# @client.on_after_configure.connect
# def setup_periodic_tasks(sender, **kwargs):
#     secondsInDay = 86400
#     sender.add_periodic_task(secondsInDay, parseULS.apply_async())


@client.task(bind=True)
def parseULS(self, state_path = "/var/lib/fbrat"):
    self.update_state(state='PROGRESS')
    result = daily_uls_parse(state_path)
    self.update_state(state='DONE')
    return result