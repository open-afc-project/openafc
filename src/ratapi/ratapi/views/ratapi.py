#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
''' The custom REST api for using the web UI and configuring AFC.
'''

import contextlib
import logging
import os
import shutil
import pkg_resources
import flask
import json
import glob
import re
import datetime
from flask.views import MethodView
import werkzeug.exceptions
from ..defs import RNTM_OPT_DBG_GUI, RNTM_OPT_DBG
from ..tasks.afc_worker import run, parseULS
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from ..models.aaa import User, AccessPoint
from .auth import auth
from ..models import aaa

#: Logger for this module
LOGGER = logging.getLogger(__name__)

#: All views under this API blueprint
module = flask.Blueprint('ratapi-v1', 'ratapi')


def build_task(request_file_path, response_file_path, request_type, user_id,
               user, temp_dir, config_file_path=None,
               runtime_opts=RNTM_OPT_DBG_GUI):
    """
    Shared logic between PAWS and All other analysis for constructing and async call to run task

    If the config path is specified the file name itself must be names afc_config.json exactly
    """
    LOGGER.debug("Temp directory opened: %s", temp_dir)
    if config_file_path is None:
        config_file_path = os.path.join(
            flask.current_app.config['STATE_ROOT_PATH'],
            'afc_config',
            # use scoped user config and fallback to default if not available
            str(user_id) if os.path.exists(os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user_id))) else '',
            'afc_config.json')
        shutil.copy(config_file_path, temp_dir)

    # copy config to temp so that it is fixed for run
    LOGGER.debug("Writing request file: %s", request_file_path)

    if flask.current_app.config['DEBUG']:
        runtime_opts &= RNTM_OPT_DBG
    LOGGER.info(runtime_opts)
    # call c++ command
    LOGGER.debug("Calling afc-engine: %s --request-type=%s "
                 "--input-file-path=%s --config-file-path=%s "
                 "--output-file-path=%s --temp-dir=%s"
                 "--log-level=%s --runtime_opt=%d",
                 os.path.join(flask.current_app.config['AFC_ENGINE']),
                 request_type,
                 request_file_path,
                 os.path.join(temp_dir, 'afc_config.json'),
                 response_file_path,
                 temp_dir,
                 flask.current_app.config['DEBUG'],
                 runtime_opts)
    task = run.apply_async(args=[
        user_id,
        user.email,
        flask.current_app.config['AFC_ENGINE'],
        flask.current_app.config['STATE_ROOT_PATH'],
        temp_dir,
        request_type,
        request_file_path,
        os.path.join(temp_dir, 'afc_config.json'),
        response_file_path,
        flask.current_app.config['HISTORY_DIR'],
        runtime_opts,
    ])
    return task


class GuiConfig(MethodView):
    ''' Allow the web UI to obtain configuration, including resolved URLs.
    '''

    def get(self):
        ''' GET for gui config
        '''

        # Figure out the current server version
        try:
            serververs = pkg_resources.require('ratapi')[0].version
        except Exception as err:
            LOGGER.error('Failed to fetch server version: {0}'.format(err))
            serververs = 'unknown'

        resp = flask.jsonify(
            paws_url=flask.url_for('paws'),
            uls_url=flask.url_for('files.uls_db'),
            antenna_url=flask.url_for('files.antenna_pattern'),
            history_url=flask.url_for('files.history'),
            afcconfig_defaults=flask.url_for(
                'ratapi-v1.AfcConfigFile', filename='afc_config.json'),
            lidar_bounds=flask.url_for('ratapi-v1.LiDAR_Bounds'),
            ras_bounds=flask.url_for('ratapi-v1.RAS_Bounds'),
            google_apikey=flask.current_app.config['GOOGLE_APIKEY'],
            rat_api_analysis=flask.url_for('ratapi-v1.Phase1Analysis',
                                           request_type='p_request_type'),
            uls_convert_url=flask.url_for(
                'ratapi-v1.UlsDb', uls_file='p_uls_file'),
            uls_daily_url=flask.url_for(
                'ratapi-v1.UlsParse'),
            login_url=flask.url_for('auth.LoginAPI'),
            user_url=flask.url_for('user.register'),
            admin_url=flask.url_for('admin.User', user_id=-1),
            ap_admin_url=flask.url_for('admin.AccessPoint', id=-1),
            rat_afc=flask.url_for('ap-afc.RatAfc'),
            afc_kml=flask.url_for('ratapi-v1.VapKmlResult',
                                  kml_file='p_kml_file'),
            afcconfig_trial=flask.url_for('ratapi-v1.TrialAfcConfigFile'),
            version=serververs,
        )
        return resp


class ReloadAnalysis(MethodView):

    ACCEPTABLE_FILES = {
        'analysisRequest.json.gz': dict(
            content_type='application/json',
        )
    }

    def _open(self, rel_path, mode, username, user=None):
        ''' Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        need to find the latest file? how to do that? - Glob
        '''
        files = glob.glob(os.path.join(
            flask.current_app.config['HISTORY_DIR'], username + "*"))
        dates = []
        for x in files:

            dateMatch = re.search(
                '\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.\d{6}', x)
            if dateMatch:
                date = datetime.datetime.strptime(
                    dateMatch.group(), '%Y-%m-%dT%H:%M:%S.%f')
                dates.append(date)  # dates loaded

        strDate = str(max(dates))
        strDate = strDate.replace(" ", "T")
        fileName = username + '-' + strDate

        LOGGER.debug(os.path.join(
            flask.current_app.config['HISTORY_DIR'], fileName))
        if mode == 'wb' and user is not None and not \
                os.path.exists(os.path.join(flask.current_app.config['HISTORY_DIR'], fileName)):
            # create scoped user directory so people don't clash over each others config
            os.mkdir(os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName))

        config_path = ''
        if user is not None and os.path.exists(os.path.join(flask.current_app.config['HISTORY_DIR'], fileName)):
            config_path = os.path.join(
               flask.current_app.config['HISTORY_DIR'], fileName)
        else:
            config_path = os.path.join(
                flask.current_app.config['HISTORY_DIR'], fileName)
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening analysisRequest file "%s"', file_path)
        if not os.path.exists(file_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == 'wb':
            os.chmod(file_path, 0o666)

        return handle

    def get(self):
        ''' GET method for afc config
        '''
        LOGGER.debug('getting analysisRequest')
        user_id = auth(roles=['AP', 'Analysis', 'Admin'])
        user = User.query.filter_by(id=user_id).first()
        responseObject = {
            'status': 'success',
            'data': {
                'userId': user.id,
                'email': user.email,
                'roles': user.roles,
                'email_confirmed_at': user.email_confirmed_at,
                'active': user.active,
                'firstName': user.first_name,
                'lastName': user.last_name,
            }
        }
        # ensure that webdav is populated with default files
        require_default_uls()
        filename = 'analysisRequest.json.gz'
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]

        resp = flask.make_response()
        with self._open(filename, 'rb', user.email, user_id) as conf_file:
            resp.data = conf_file.read()
        LOGGER.debug(resp.data)
        json_resp = json.loads(resp.data)
        LOGGER.debug(json_resp)
        # has key deviceDesc and deviceDesc.serialNumber == "analysis-ap" => PointAnalysis
        # has key deviceDesc and deviceDesc.serialNumber != "analysis-ap" => Virtual AP
        # has key spacing => HeatMap
        # has key FSID => ExclusionZone
        if('deviceDesc' in json_resp and json_resp['deviceDesc']['serialNumber'] == "analysis-ap"):
            resp.headers['AnalysisType'] = 'PointAnalysis'

        elif('deviceDesc' in json_resp and json_resp['deviceDesc']['serialNumber'] != "analysis-ap"):
            resp.headers['AnalysisType'] = 'VirtualAP'

        elif('key spacing' in json_resp):
            resp.headers['AnalysisType'] = 'HeatMap'

        elif('FSID' in json_resp):
            resp.headers['AnalysisType'] = 'ExclusionZone'
        else:
            resp.headers['AnalysisType'] = 'None'
        LOGGER.debug(json_resp['deviceDesc']['serialNumber'])
        LOGGER.debug(resp.headers['AnalysisType'])
        # json.dumps(json_resp, resp.data)
        # LOGGER.debug(resp.data)
        resp.content_type = filedesc['content_type']
        return resp


class AfcConfigFile(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    ACCEPTABLE_FILES = {
        'afc_config.json': dict(
            content_type='application/json',
        )
    }

    def _open(self, rel_path, mode, user=None):
        ''' Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''
        if mode == 'wb' and user is not None and not \
                os.path.exists(os.path.join(flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))):
            # create scoped user directory so people don't clash over each others config
            os.mkdir(os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user)))

        config_path = ''
        if user is not None and os.path.exists(os.path.join(flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))):
            config_path = os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))
        else:
            config_path = os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config')
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening config file "%s"', file_path)
        if not os.path.exists(file_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == 'wb':
            os.chmod(file_path, 0o666)

        return handle

    def get(self, filename):
        ''' GET method for afc config
        '''
        LOGGER.debug('getting afc_conf')
        user_id = auth(roles=['AP', 'Analysis'])
        # ensure that webdav is populated with default files
        require_default_uls()

        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]

        resp = flask.make_response()
        with self._open('afc_config.json', 'rb', user_id) as conf_file:
            resp.data = conf_file.read()
        resp.content_type = filedesc['content_type']
        return resp

    def put(self, filename):
        ''' PUT method for afc config
        '''
        user_id = auth(roles=['AP', 'Analysis'])
        LOGGER.debug("current user: %s", user_id)
        if filename not in self.ACCEPTABLE_FILES:
            raise werkzeug.exceptions.NotFound()
        filedesc = self.ACCEPTABLE_FILES[filename]
        if flask.request.content_type != filedesc['content_type']:
            raise werkzeug.exceptions.UnsupportedMediaType()

        with contextlib.closing(self._open(filename, 'wb', user_id)) as outfile:
            shutil.copyfileobj(flask.request.stream, outfile)
        return flask.make_response('AFC configuration file updated', 204)


class TrialAfcConfigFile(MethodView):
    ''' Allow the web UI to pull the config to be used for Trial users.
    '''

    def _open(self, rel_path, mode, user=None):
        ''' Open a configuration file.

        :param rel_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''
        if mode == 'wb' and user is not None and not \
                os.path.exists(os.path.join(flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))):
            # create scoped user directory so people don't clash over each others config
            os.mkdir(os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user)))

        config_path = ''
        if user is not None and os.path.exists(os.path.join(flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))):
            config_path = os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user))
        else:
            config_path = os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'], 'afc_config')
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        file_path = os.path.join(config_path, rel_path)
        LOGGER.debug('Opening config file "%s"', file_path)
        if not os.path.exists(file_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(file_path, mode)

        if mode == 'wb':
            os.chmod(file_path, 0o666)

        return handle

    def get(self):
        ''' GET method for afc config
        '''
        filedesc = {
        'afc_config.json': dict(
            content_type='application/json',
        )
        }
        LOGGER.debug('getting afc_conf for trial user')
        user_id = auth(roles=['Trial'])
        # ensure that webdav is populated with default files
        require_default_uls()

        ap = aaa.AccessPoint.query.filter_by(
            serial_number="TestSerialNumber", certification_id="FCC TestCertificationId").first()
        if ap is not None:
            LOGGER.debug('getting ap for trial user  %s ', ap)
            user_id = ap.user_id
       
            resp = flask.make_response()
            with self._open('afc_config.json', 'rb', user_id) as conf_file:
                resp.data = conf_file.read()
            resp.content_type = "application/json"
            return resp
        else:
            LOGGER.debug('unable to find AP for trial user')
            raise werkzeug.exceptions.NotFound()


class LiDAR_Bounds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def _open(self, abs_path, mode, user=None):
        ''' Open a file.

        :param abs_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Opening file "%s"', abs_path)
        if not os.path.exists(abs_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(abs_path, mode)

        return handle

    def get(self):
        ''' GET method for LiDAR_Bounds
        '''
        LOGGER.debug('getting LiDAR bounds')
        user_id = auth(roles=['AP', 'Analysis'])

        import xdg.BaseDirectory

        try:
            resp = flask.make_response()
            datapath = next(xdg.BaseDirectory.load_data_paths('fbrat', 'rat_transfer', 'proc_lidar_2019'))
            full_path = os.path.join(datapath, 'LiDAR_Bounds.json.gz')
            if not os.path.exists(full_path):
                raise werkzeug.exceptions.NotFound('LiDAR bounds file not found')
            with self._open(full_path, 'rb', user_id) as data_file:
                resp.data = data_file.read()
            resp.content_type = 'application/json'
            resp.content_encoding = 'gzip'
            return resp
        except StopIteration:
            raise werkzeug.exceptions.NotFound('Path not found to file')

class RAS_Bounds(MethodView):
    ''' Allow the web UI to manipulate configuration directly.
    '''

    def _open(self, abs_path, mode, user=None):
        ''' Open a file.

        :param abs_path: The specific config name to open.
        :param mode: The file open mode.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Opening file "%s"', abs_path)
        if not os.path.exists(abs_path) and mode != 'wb':
            raise werkzeug.exceptions.NotFound()

        handle = open(abs_path, mode)

        return handle

    def get(self):
        ''' GET method for RAS_Bounds
        '''
        LOGGER.debug('getting RAS bounds')
        user_id = auth(roles=['AP', 'Analysis'])

        import xdg.BaseDirectory

        try:
            resp = flask.make_response()
            datapath = next(xdg.BaseDirectory.load_data_paths('fbrat', 'rat_transfer', 'proc_lidar_2019'))
            full_path = os.path.join(datapath, 'RAS_ExclusionZone.json')
            if not os.path.exists(full_path):
                raise werkzeug.exceptions.NotFound('RAS exclusion zone file not found')
            with self._open(full_path, 'rb', user_id) as data_file:
                resp.data = data_file.read()
            resp.content_type = 'application/json'
            # resp.content_encoding = 'gzip'
            return resp
        except StopIteration:
            raise werkzeug.exceptions.NotFound('Path not found to file')


class Phase1Analysis(MethodView):
    ''' Run analysis using AFC engine and display graphical results on map and graphs
    '''

    methods = ['POST']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.

        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def post(self, request_type):
        ''' Run analysis 

            :param request_type: 'PointAnalysis', 'ExclusionZoneAnalysis', or 'HeatmapAnalysis'
        '''

        user_id = auth(roles=['Analysis'])
        user = User.query.filter_by(id=user_id).first()

        args = flask.request.data
        LOGGER.debug("Running phase 1 analysis with params: %s", args)
        LOGGER.debug("history directory: %s",
                     flask.current_app.config['HISTORY_DIR'])

        valid_requests = ['PointAnalysis',
                          'ExclusionZoneAnalysis', 'HeatmapAnalysis']
        if request_type not in valid_requests:
            raise werkzeug.exceptions.BadRequest('Invalid request type')
        temp_dir = getQueueDirectory(
            flask.current_app.config['TASK_QUEUE'], request_type)
        request_file_path = os.path.join(temp_dir, 'analysisRequest.json.gz')

        response_file_path = os.path.join(
            temp_dir, 'analysisResponse.json.gz')

        LOGGER.debug("Writing request file: %s", request_file_path)
        with open(request_file_path, "w") as fle:
            fle.write(args)  # write JSON to request file
        LOGGER.debug("Request file written")

        task = build_task(request_file_path, response_file_path,
                          request_type, user_id, user, temp_dir)

        if task.state == 'FAILURE':
            raise werkzeug.exceptions.InternalServerError(
                'Task was unable to be started', dict(id=task.id, info=str(task.info)))

        include_kml = request_type in [
            'ExclusionZoneAnalysis', 'PointAnalysis']

        return flask.jsonify(
            taskId=task.id,
            statusUrl=flask.url_for(
                'ratapi-v1.AnalysisStatus', task_id=task.id),
            kmlUrl=(flask.url_for('ratapi-v1.AnalysisKmlResult',
                                  task_id=task.id) if include_kml else None)
        )


class AnalysisKmlResult(MethodView):
    ''' Get a KML result file from AFC Engine '''

    methods = ['GET']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.

        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def get(self, task_id):
        ''' GET method for KML Result '''

        task = run.AsyncResult(task_id)
        LOGGER.debug('state: %s', task.state)

        if task.successful() and task.result['status'] == 'DONE':
            if not os.path.exists(task.result['result_path']):
                return flask.make_response(flask.json.dumps(dict(message='Resource already deleted')), 410)
            if 'kml_path' not in task.result or not os.path.exists(task.result['kml_path']):
                return werkzeug.exceptions.NotFound('This task did not produce a KML')
            resp = flask.make_response()
            LOGGER.debug("Reading kml file: %s", task.result['kml_path'])
            with self._open(task.result['kml_path'], 'rb') as resp_file:
                resp.data = resp_file.read()
            resp.content_type = 'application/octet-stream'
            return resp

        else:
            raise werkzeug.exceptions.NotFound('KML not found')


class VapKmlResult(MethodView):
    ''' Get a KML result file from AFC Engine based on response filename'''

    methods = ['GET', 'DELETE']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.

        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def get(self, kml_file):
        ''' GET method for KML Result '''

        kml_path = os.path.join('/var/lib/fbrat/responses',kml_file)
        LOGGER.debug('Attempting to download KML file from  "%s"', kml_path)
        if not os.path.exists(kml_path):
            return flask.make_response(flask.json.dumps(dict(message='Resource not found')), 410)
        resp = flask.make_response()
        LOGGER.debug("Reading kml file: %s", kml_path)
        with self._open(kml_path, 'rb') as resp_file:
            resp.data = resp_file.read()
        
        if(kml_path.endswith("json.gz")):
            resp.content_encoding = "gzip"
            resp.content_type = 'application/json'
        else:
            resp.content_type = 'application/octet-stream'
        return resp

    def delete(self, kml_file):
         kml_path = os.path.join('/var/lib/fbrat/responses',kml_file)
         if  os.path.exists(kml_path):
            os.remove(kml_path)
         return flask.make_response(flask.json.dumps(dict(message='file deleted')), 200)

class AnalysisStatus(MethodView):
    ''' Check status of task '''

    methods = ['GET', 'DELETE']

    def _open(self, abs_path, mode):
        ''' Open a response file.

        :param rel_path: The specific file name to open.
        :return: The opened file.
        :rtype: file-like
        '''

        LOGGER.debug('Attempting to open response file "%s"', abs_path)
        if not os.path.exists(abs_path):
            raise werkzeug.exceptions.InternalServerError(
                description='Your analysis was unable to be processed.')
        return open(abs_path, mode)

    def get(self, task_id):
        ''' GET method for Analysis Status '''

        task = run.AsyncResult(task_id)

        LOGGER.debug('state: %s', task.state)

        if task.info and task.info.get('progress_file') is not None and \
                not os.path.exists(os.path.dirname(task.info['progress_file'])):
            return flask.make_response(flask.json.dumps(dict(percent=0, message="Try Again")), 503)

        if task.state == 'PROGRESS':
            auth(is_user=task.info['user_id'])
            # get progress and ETA
            if not os.path.exists(task.info['progress_file']):
                return flask.jsonify(
                    percent=0,
                    message='Initializing...'
                ), 202
            progress_file = task.info['progress_file']
            with open(progress_file, 'r') as prog:
                lines = prog.readlines()
                if len(lines) <= 0:
                    return flask.make_response(flask.json.dumps(dict(percent=0, message="Try Again")), 503)
                LOGGER.debug(lines)
                return flask.jsonify(
                    percent=float(lines[0]),
                    message=lines[1],
                ), 202

        if not task.ready():
            return flask.make_response(flask.json.dumps(dict(percent=0, message='Pending...')), 202)

        if task.failed():
            raise werkzeug.exceptions.InternalServerError(
                'Task execution failed')

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.result['result_path']):
                return flask.make_response(flask.json.dumps(dict(message='Resource already deleted')), 410)
            # read temporary file generated by afc-engine
            resp = flask.make_response()
            LOGGER.debug("Reading result file: %s", task.result['result_path'])
            with self._open(task.result['result_path'], 'rb') as resp_file:
                resp.data = resp_file.read()
            resp.content_type = 'application/json'
            resp.content_encoding = "gzip"
            return resp

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.result['error_path']):
                return flask.make_response(flask.json.dumps(dict(message='Resource already deleted')), 410)
            # read temporary file generated by afc-engine
            LOGGER.debug("Reading error file: %s", task.result['error_path'])
            with self._open(task.result['error_path'], 'rb') as error_file:
                raise AFCEngineException(
                    description=error_file.read(), exit_code=task.result['exit_code'])

        else:
            raise werkzeug.exceptions.NotFound('Task not found')

    def delete(self, task_id):
        ''' DELETE method for Analysis Status '''

        task = run.AsyncResult(task_id)

        if not task.ready():
            # task is still running, terminate it
            LOGGER.debug('Terminating %s', task_id)
            task.revoke(terminate=True)
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        if task.failed():
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.info['result_path']):
                return flask.make_response(flask.json.dumps(dict(message='Resource already deleted')), 410)
            LOGGER.debug('Deleting %s', task.info.get('result_path'))
            os.remove(task.info['result_path'])
            if 'kml_path' in task.info and os.path.exists(task.info['kml_path']):
                os.remove(task.info['kml_path'])
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            if not os.path.exists(task.info['error_path']):
                return flask.make_response(flask.json.dumps(dict(message='Resource already deleted')), 410)
            LOGGER.debug('Deleting %s', task.info.get('result_path'))
            os.remove(task.info['error_path'])
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        else:
            raise werkzeug.exceptions.NotFound('Task not found')


class UlsDb(MethodView):
    ''' Resource for converting uls files '''

    methods = ['POST']

    def post(self, uls_file):
        ''' POST method for ULS Db convert '''

        auth(roles=['Admin'])

        from ..db.generators import create_uls_db

        uls_path = os.path.join(
            flask.current_app.config['STATE_ROOT_PATH'], 'ULS_Database', uls_file)
        if not os.path.exists(uls_path):
            raise werkzeug.exceptions.BadRequest(
                "File does not exist: " + uls_file)

        file_base = os.path.splitext(uls_path)[0]
        LOGGER.debug('converting uls from csv(%s) to sqlite3(%s)',
                     uls_path, file_base + '.sqlite3')
        try:
            invalid_rows, errors = create_uls_db(file_base, uls_path)
            LOGGER.debug('conversion complete')

            return flask.jsonify(
                invalidRows=invalid_rows,
                errors=errors
            )

        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)

class UlsParse(MethodView):
    ''' Resource for daily parse of ULS data '''

    methods = ['GET', 'POST', 'PUT']

    def get(self):
        ''' GET method for last successful runtime of uls parse
        '''
        LOGGER.debug('getting last successful runtime of uls parse')
        user_id = auth(roles=['Admin'])

        try:
            datapath = flask.current_app.config["STATE_ROOT_PATH"] + '/daily_uls_parse/data_files/lastSuccessfulRun.txt'
            if not os.path.exists(datapath):
                raise werkzeug.exceptions.NotFound('last succesful run file not found')
            lastSuccess = ''
            with open(datapath, 'r') as data_file:
                lastSuccess = data_file.read()
            return flask.jsonify(
                lastSuccessfulRun=lastSuccess,
            )
        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)

    def post(self):
        ''' POST method for manual daily ULS Db parsing '''

        auth(roles=['Admin'])

        LOGGER.debug('Kicking off daily uls parse')
        try:
            task = parseULS.apply_async(args=[flask.current_app.config["STATE_ROOT_PATH"], True])
            
            LOGGER.debug('uls parse started')

            if task.state == 'FAILURE':
                raise werkzeug.exceptions.InternalServerError(
                    'Task was unable to be started', dict(id=task.id, info=str(task.info)))

            return flask.jsonify(
                taskId=task.id,
                
                statusUrl=flask.url_for(
                    'ratapi-v1.DailyULSStatus', task_id=task.id),
            )

        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)

    def put(self):
        ''' Put method for setting the automatic daily ULS time '''

        auth(roles=['Admin'])
        args = json.loads(flask.request.data)
        LOGGER.debug('Recieved arg %s', args)
        hours = args["hours"]
        mins =  args["mins"]
        if hours == 0:
            hours = "00"
        if mins == 0:
            mins = "00"
        timeStr = str(hours) + ":" + str(mins)
        LOGGER.debug('Updating automated ULS time to ' + timeStr + " UTC")
        datapath = flask.current_app.config["STATE_ROOT_PATH"] + '/daily_uls_parse/data_files/nextRun.txt'
        if not os.path.exists(datapath):
            raise werkzeug.exceptions.NotFound('next run file not found')
        with open(datapath, 'w') as data_file:
            data_file.write(timeStr)
        try:
            return flask.jsonify(
                newTime=timeStr
            ), 200
        except Exception as err:
            raise werkzeug.exceptions.InternalServerError(
                description=err.message)


class DailyULSStatus(MethodView):
    ''' Check status of task '''

    methods = ['GET', 'DELETE']

    def resetManualParseFile(self):
        ''' Overwrites the file for manual task id with a blank string '''
        datapath = flask.current_app.config["STATE_ROOT_PATH"] + '/daily_uls_parse/data_files/currentManualId.txt'
        with open(datapath, 'w') as data_file:
            data_file.write("")

    def get(self, task_id):
        ''' GET method for uls parse Status '''
        LOGGER.debug("Getting ULS Parse status with task id: " + task_id)
        task = parseULS.AsyncResult(task_id)
        # LOGGER.debug('state: %s', task.state)

        if task.state == 'PROGRESS':
            LOGGER.debug("Found Task in progress")
            # todo: add percent progress
            return flask.jsonify(
                    percent="WIP",
            ), 202
        if not task.ready():
            LOGGER.debug("Found Task pending")
            return flask.make_response(flask.json.dumps(dict(percent=0, message='Pending...')), 202)
        if task.state == 'REVOKED':
            LOGGER.debug("Found task already in progress")
            # LOGGER.debug("task info %s", task.info)
            raise werkzeug.exceptions.ServiceUnavailable()

        elif task.failed():
            LOGGER.debug("Found failed task")
            self.resetManualParseFile()
            raise werkzeug.exceptions.InternalServerError(
                'Task excecution failed')

        if task.successful():
            self.resetManualParseFile()
            results = task.result
            return flask.jsonify(
                    entriesUpdated=results[0],
                    entriesAdded=results[1],
                    finishTime=results[2]
            ), 200

        else:
            raise werkzeug.exceptions.NotFound('Task not found')

    def delete(self, task_id):
        ''' DELETE method for ULS Status '''

        task = parseULS.AsyncResult(task_id)

        if not task.ready():
            # task is still running, terminate it
            LOGGER.debug('Terminating %s', task_id)
            task.revoke(terminate=True)
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)
        if task.failed():
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        if task.successful() and task.result['status'] == 'DONE':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        elif task.successful() and task.result['status'] == 'ERROR':
            auth(is_user=task.result['user_id'])
            task.forget()
            return flask.make_response(flask.json.dumps(dict(message='Task deleted')), 200)

        else:
            raise werkzeug.exceptions.NotFound('Task not found')



module.add_url_rule('/guiconfig', view_func=GuiConfig.as_view('GuiConfig'))
module.add_url_rule('/afcconfig/<path:filename>',
                    view_func=AfcConfigFile.as_view('AfcConfigFile'))
module.add_url_rule('/afcconfig/trial', view_func= TrialAfcConfigFile.as_view('TrialAfcConfigFile'))
module.add_url_rule('/files/lidar_bounds',
                    view_func=LiDAR_Bounds.as_view('LiDAR_Bounds'))
module.add_url_rule('/files/ras_bounds', 
                    view_func=RAS_Bounds.as_view('RAS_Bounds'))
module.add_url_rule('/analysis/p1/<request_type>',
                    view_func=Phase1Analysis.as_view('Phase1Analysis'))
module.add_url_rule('/analysis/status/p1/<task_id>',
                    view_func=AnalysisStatus.as_view('AnalysisStatus'))
module.add_url_rule('/analysis/kml/p1/<task_id>',
                    view_func=AnalysisKmlResult.as_view('AnalysisKmlResult'))
module.add_url_rule('/convert/uls/csv/sql/<uls_file>',
                    view_func=UlsDb.as_view('UlsDb'))
module.add_url_rule('/ulsparse',
                    view_func=UlsParse.as_view('UlsParse'))
module.add_url_rule('/ulsparse/<task_id>',
                    view_func=DailyULSStatus.as_view('DailyULSStatus'))
module.add_url_rule('/replay',
                    view_func=ReloadAnalysis.as_view('ReloadAnalysis'))
module.add_url_rule('/kml/<path:kml_file>',
                    view_func=VapKmlResult.as_view('VapKmlResult'))
