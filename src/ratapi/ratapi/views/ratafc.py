#
# This Python file uses the following encoding: utf-8
#
# Portions copyright © 2021 Broadcom.
# All rights reserved. The term “Broadcom” refers solely
# to the Broadcom Inc. corporate affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which
# is included with this software program.
#
''' API for AFC specification
'''

import gzip
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
from ..defs import RNTM_OPT_NODBG_NOGUI, RNTM_OPT_DBG_NOGUI, RNTM_OPT_NODBG_GUI, RNTM_OPT_DBG_GUI
from ..tasks.afc_worker import run
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from ..models.aaa import User, AccessPoint
from .auth import auth
from .ratapi import build_task

#: Logger for this module
LOGGER = logging.getLogger(__name__)

RULESET = 'US_47_CFR_PART_15_SUBPART_E'

#: All views under this API blueprint
module = flask.Blueprint('ap-afc', 'ap-afc')


class AP_Exception(Exception):
    ''' Exception type used for RAT AFC respones
    '''

    def __init__(self, code, description, supplemental_info=None):
        super(AP_Exception, self).__init__(description)
        self.response_code = code
        self.description = description
        self.supplemental_info = supplemental_info


# class TempNotProcessedException(AP_Exception):
#     '''
#     101	Name:	TEMPORARY_NOT_PROCESSED
# 	Interpretation:	AFC cannot complete processing the request and respond to it.
# 	Required Action:	AFC Device shall resend the same request later.
# 	Other Information:	This responseCode value is returned when, for example, AFC receives a lot of requests or a large request and AFC cannot complete the process soon.
#     '''

#     def __init__(self, wait_time_sec=3600):
#         super(TempNotProcessedException, self).__init__(101, 'The request could not be processed at this time. Try again later.', { 'waitTime': wait_time_sec })

class VersionNotSupportedException(AP_Exception):
    '''
    100 Name: VERSION_NOT_SUPPORTED
    Interpretation: The requested version number is invalid 
    The communication can be attempted again using a different version number. In the case of an AP attempting 
    to communicate with an AFC, communications with the same version but a different AFC could be attempted
    '''

    def __init__(self, invalid_version=[]):
        super(VersionNotSupportedException, self).__init__(
            100, 'The requested version number is invalid', {'invalidVersion': invalid_version})


class DeviceUnallowedException(AP_Exception):
    '''
    101 Name: DEVICE_UNALLOWED
    Interpretation: The provided credentials are invalid
    This specific device as identified by the combination of its FCC ID and unique manufacturer's 
    serial number is not allowed to operate under AFC control due to regulatory action or other action.
    '''

    def __init__(self):
        super(DeviceUnallowedException, self).__init__(
            101, 'This specific device is not allowed to operate under AFC control.')


class MissingParamException(AP_Exception):
    '''
    102	Name:	MISSING_PARAM
        Interpretation:	One or more fields required to be included in the request are missing.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of missing parameter names.
    '''

    def __init__(self, missing_params=[]):
        super(MissingParamException, self).__init__(
            102, 'One or more fields required to be included in the request are missing.', {'missingParams': missing_params})


class InvalidValueException(AP_Exception):
    '''
    103	Name:	INVALID_VALUE
        Interpretation:	One or more fields have an invalid value.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of the names of the fields set to invalid value.
    '''

    def __init__(self, invalid_params=[]):
        super(InvalidValueException, self).__init__(
            103, 'One or more fields have an invalid value.', {'invalidParams': invalid_params})


class UnexpectedParamException(AP_Exception):
    '''
    106	Name:	UNEXPECTED_PARAM
        Interpretation:	Unknown parameter found, or conditional parameter found, but condition is not met.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of unexpected parameter names.
    '''

    def __init__(self, unexpected_params=[]):
        super(UnexpectedParamException, self).__init__(
            106, 'Unknown parameter found, or conditional parameter found, but condition is not met.', {'unexpectedParams': unexpected_params})


class UnsupportedSpectrumException(AP_Exception):
    '''
    300	Name:	UNSUPPORTED_SPECTRUM
        Interpretation:	The frequency range indicated in the Available Spectrum Inquiry Request is at least partially outside of the frequency band under the management of the AFC (e.g. 5.925-6.425 GHz and 6.525-6.875 GHz bands in US).
        Required Action:	Not specified.
        Other Information:	None
    '''

    def __init__(self):
        super(UnsupportedSpectrumException, self).__init__(
            300, 'The frequency range indicated in the Available Spectrum Inquiry Request is at least partially outside of the frequency band under the management of the AFC.')


def _translate_afc_error(error_msg):
    unsupported_spectrum = error_msg.find('UNSUPPORTED_SPECTRUM')
    if unsupported_spectrum != -1:
        raise UnsupportedSpectrumException()

    invalid_value = error_msg.find('INVALID_VALUE')
    if invalid_value != -1:
        raise InvalidValueException()

    missing_param = error_msg.find('MISSING_PARAM')
    if missing_param != -1:
        raise MissingParamException()

    unexpected_param = error_msg.find('UNEXPECTED_PARAM')
    if unexpected_param != -1:
        raise UnexpectedParamException()

    invalid_version = error_msg.find('VERSION_NOT_SUPPORTED')
    if invalid_version != -1:
        raise VersionNotSupported()


def fail_done(task):
    if not os.path.exists(task.result['error_path']):
        return flask.make_response(
            flask.json.dumps(dict(message='Resource already deleted')),
            410)
    # read temporary file generated by afc-engine
    LOGGER.debug("Reading error file: %s", task.result['error_path'])
    error_data = None
    with open(task.result['error_path'], 'rb') as error_file:
        error_data = error_file.read()
        LOGGER.debug("Error data: %s", error_data)
        os.remove(task.result['error_path'])
        # try to pick up on specific errors
        _translate_afc_error(error_data)
        # raise for internal AFC E2yyngine error
        raise AP_Exception(-1, error_data)


def in_progress(task):
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
            return flask.make_response(
                flask.json.dumps(dict(percent=0, message="Try Again")),
                503)
        return flask.jsonify(
            percent=float(lines[0]),
            message=lines[1],
        ), 202


def success_done(task):
    if not os.path.exists(task.result['result_path']):
        raise ServerError('Resource already deleted')
    # read temporary file generated by afc-engine
    LOGGER.debug("success_done() Reading result file: %s",
                 task.result['result_path'])
    kmz_file = task.result.get('kml_path')
    geo_file = task.result.get('geoJson_path')
    resp_data = None
    with gzip.open(task.result['result_path'], 'rb') as resp_file:
        resp_data = resp_file.read()

    LOGGER.debug('resp data before: %s', resp_data)
    LOGGER.debug('kmz and geo files: %s %s', kmz_file, geo_file)
    #add the map data file (if it is generated) into a vendor extension
    # will have to revist when API can handle multiple requests
    if geo_file is not None and os.path.exists(geo_file) and kmz_file is not None and os.path.exists(kmz_file):
        resp_json = json.loads(resp_data)
        resp_json["vendorExtensions"] = [{
            "extensionID": "openAfc.mapinfo",
            "parameters": {
                "kmzFile": os.path.basename(kmz_file),
                "geoJsonFile": os.path.basename(geo_file)
            }
        }]
        resp_data = json.dumps(resp_json)

    os.remove(task.result['result_path'])
    LOGGER.debug('resp data after: %s', resp_data)
    resp = flask.make_response()
    resp.data = resp_data
    return resp

response_map = {
    'DONE' : success_done,
    'ERROR' : fail_done,
    'PROGRESS': in_progress
}


class RatAfc(MethodView):
    ''' RAT AFC resources
    '''

    def _auth_ap(self, serial_number, certification_id, rulesets):
        ''' Authenticate an access point. If must match the serial_number and certification_id in the database to be valid
        '''
        LOGGER.debug('RatAfc::_auth_ap()')
        LOGGER.debug('Starting auth_ap,serial: %s; certId: %s; ruleset %s.',
                     serial_number,certification_id,rulesets )

        if serial_number is None:
            raise MissingParamException(['serialNumber'])

        ap = AccessPoint.query.filter_by(serial_number=serial_number).first()

        if rulesets is None or len(rulesets) != 1 or rulesets[0] != RULESET:
            raise InvalidValueException(["rulesets"])
        if ap is None:
            raise DeviceUnallowedException()  # InvalidCredentialsException()
        if certification_id is None:
            raise MissingParamException(missing_params=['certificationId'])
        if ap.certification_id != certification_id:
            raise DeviceUnallowedException()  # InvalidCredentialsException()

        LOGGER.info('Found AP, certifiying User with Id: %i', ap.user_id)
        return ap.user_id

    def get(self):
        ''' GET method for Analysis Status '''
        LOGGER.debug('RatAfc::get()')

        task_id = flask.request.args['task_id']
        task = run.AsyncResult(task_id)

        LOGGER.debug('RatAfc::get() state: %s', task.state)

        if task.info and task.info.get('progress_file') is not None and \
            not os.path.exists(os.path.dirname(task.info['progress_file'])):
            return flask.make_response(
                flask.json.dumps(dict(percent=0, message="Try Again")),
                503)

        if task.state == 'PROGRESS':
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
                    return flask.make_response(
                        flask.json.dumps(dict(percent=0, message="Try Again")),
                        503)
                LOGGER.debug(lines)
                return flask.jsonify(
                    percent=float(lines[0]),
                    message=lines[1],
                ), 202

        elif task.successful():
            LOGGER.debug('RatAfc::get() success state: %s status %s',
                         task.state, task.result['status'])
            return response_map[task.result['status']](task)

        elif not task.ready():
            LOGGER.debug('RatAfc::get() not ready state: %s', task.state)
            return flask.make_response(
                flask.json.dumps(dict(percent=0, message='Pending...')),
                202)

        elif task.failed():
            raise werkzeug.exceptions.InternalServerError(
                'Task execution failed')

    def post(self):
        ''' POST method for RAT AFC
        '''
        LOGGER.debug('RatAfc::post()')

        # check request version
        ver = flask.request.json["version"]
        if not (ver == "1.1"):
            raise VersionNotSupportedException([ver])

        # start request
        # Get the entire JSON to allow for multiple requests and
        # match v1.1 expected format
        args = flask.request.json
        # as of now, there can be only one request
        firstRequest = flask.request.json["availableSpectrumInquiryRequests"][0]
        LOGGER.debug("Running AFC analysis with params: %s", args)
        request_type = 'AP-AFC'

        runtime_opts = RNTM_OPT_DBG_GUI
        if hasattr(self, 'runtime_opts'):
            runtime_opts = self.runtime_opts

        try:
            config = None
            # disable the passing of configuration via vendorExtensions
            # extensions = firstRequest['vendorExtensions'] if 'vendorExtensions' in args else []
            # for ext in extensions:
            #     if ext['extensionID'] == 'RAT v1.1 AFC Config':
            #         config = ext['parameters']
            #         del ext['parameters']

            # authenticate
            device_desc = firstRequest.get('deviceDescriptor')

            try:
                firstCertId = device_desc['certificationId'][0]['nra'] + \
                    ' ' + device_desc['certificationId'][0]['id']
            except:
                firstCertId = None

            user_id = self._auth_ap(device_desc.get('serialNumber'), firstCertId, device_desc.get('rulesetIds'))

            user = User.query.filter_by(id=user_id).first()

            # process request
            # write guiJson and afc config to separate files in temp directory
            # if no config is in request then look for config accociated with the user
            temp_dir = getQueueDirectory(
                flask.current_app.config['TASK_QUEUE'], request_type)
            request_file_path = os.path.join(temp_dir, 'analysisRequest.json')

            response_file_path = os.path.join(
                temp_dir, 'analysisResponse.json.gz')

            config_file_path = None
            if config is None:
                LOGGER.debug(
                    'Config file not in vendor extension, using user specified config')
                config_file_path = os.path.join(
                    flask.current_app.config['STATE_ROOT_PATH'],
                    'afc_config',
                    # use scoped user config and fallback to default if not available
                    str(user_id) if os.path.exists(os.path.join(
                        flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user_id))) else '',
                    'afc_config.json')
                shutil.copy(config_file_path, temp_dir)
            else:
                LOGGER.debug(
                    'AP specified config vendor extension. Writing contents to afc_config.json')
                config_file_path = os.path.join(
                    temp_dir, 'afc_config.json')
                LOGGER.debug("Writing config file: %s", config_file_path)
                config_json_bytes = json.dumps(config).encode('utf-8')
                with open(config_file_path, 'wb') as fout:
                    fout.write(config_json_bytes)
                LOGGER.debug("config file written")
                LOGGER.debug('config: %s', config_json_bytes)

            request_json_bytes = json.dumps(args).encode('utf-8')
            LOGGER.debug("Writing request file: %s", request_file_path)
            with open(request_file_path, 'wb') as fout:
                fout.write(request_json_bytes)
            LOGGER.debug("Request file written")

            task = build_task(request_file_path, response_file_path,
                              request_type, 0, user, temp_dir, config_file_path,
                              runtime_opts)

            conn_type = flask.request.args.get('conn_type')
            LOGGER.debug('RatAfc::post() %s, %s, %d',
                         task.state, conn_type, task.id)
            if conn_type == 'async':
                return flask.jsonify(taskId = task.id,
                                     taskState = task.state)

            # wait for request to finish processing
            task.wait()

            LOGGER.debug('task result: %s', task.result)

            if task.successful():
                return response_map[task.result['status']](task)
            else:
                raise AP_Exception(-1,
                                   'The completed task state is invalid. '
                                   'Try again later, and if this issue '
                                   'persists contact support.')

        except AP_Exception as e:
            LOGGER.error('catching exception: %s', e.message)
            result = {
                'version': '1.1',
                'availableSpectrumInquiryResponses': [{
                    'requestId': firstRequest.get('requestId'),
                    'response': {
                        'responseCode': e.response_code,
                        'shortDescription': e.description,
                        'supplementalInfo': e.supplemental_info
                    }
                }]
            }
            LOGGER.error(str(result))
            resp = flask.make_response(flask.json.dumps(result), 200)
            resp.content_type = 'application/json'
            return resp


class RatAfc_DbgNoGui(RatAfc):
   def __init__(self):
       LOGGER.debug('RatAfc_DbgNoGui::__init__()')
       self.runtime_opts = RNTM_OPT_DBG_NOGUI
       super(RatAfc_DbgNoGui, self).__init__()


class RatAfc_NoDbgGui(RatAfc):
   def __init__(self):
       LOGGER.debug('RatAfc_NoDbgGui::__init__()')
       self.runtime_opts = RNTM_OPT_NODBG_GUI
       super(RatAfc_NoDbgGui, self).__init__()


class RatAfc_NoDbgNoGui(RatAfc):
   def __init__(self):
       LOGGER.debug('RatAfc_NoDbgNoGui::__init__()')
       self.runtime_opts = RNTM_OPT_NODBG_NOGUI
       super(RatAfc_NoDbgNoGui, self).__init__()


class RatAfc_DbgGui(RatAfc):
   def __init__(self):
       LOGGER.debug('RatAfc_DbgGui::__init__()')
       self.runtime_opts = RNTM_OPT_DBG_GUI
       super(RatAfc_DbgGui, self).__init__()


# registration with runtime option NoDebug and NoGui
module.add_url_rule('/1.1/nodbg_nogui/availableSpectrumInquiry',
                    view_func=RatAfc_NoDbgNoGui.as_view('RatAfc_NoDbgNoGui'))
# registration with runtime option Debug and NoGui
module.add_url_rule('/1.1/dbg_nogui/availableSpectrumInquiry',
                    view_func=RatAfc_DbgNoGui.as_view('RatAfc_DbgNoGui'))
# registration with runtime option NoDebug and Gui
module.add_url_rule('/1.1/nodbg_gui/availableSpectrumInquiry',
                    view_func=RatAfc_NoDbgGui.as_view('RatAfc_NoDbgGui'))
# registration with runtime option Debug and Gui
module.add_url_rule('/1.1/dbg_gui/availableSpectrumInquiry',
                    view_func=RatAfc_DbgGui.as_view('RatAfc_DbgGui'))
# registration of default runtime options
module.add_url_rule('/1.1/availableSpectrumInquiry',
                    view_func=RatAfc.as_view('RatAfc'))

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
