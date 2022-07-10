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
import sys
import shutil
import pkg_resources
import flask
import json
import glob
import re
import datetime
import zlib
from flask.views import MethodView
import werkzeug.exceptions
import StringIO
from ..defs import RNTM_OPT_NODBG_NOGUI, RNTM_OPT_DBG, RNTM_OPT_GUI, RNTM_OPT_AFCENGINE_HTTP_IO
from ..tasks.afc_worker import run
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from ..models.aaa import User, AccessPoint
from .auth import auth
from .ratapi import build_task
from .. import data_if
from .. import task

#: Logger for this module
LOGGER = logging.getLogger(__name__)

RULESET = 'US_47_CFR_PART_15_SUBPART_E'

#: All views under this API blueprint
module = flask.Blueprint('ap-afc', 'ap-afc')

# tuple listing files to remain in cache dir
CACHED_FILES = ("analysisResponse.json.gz", "mapData.json.gz", "results.kmz")


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


def fail_done(task_stat, dataif):
    error_data = None
    if task_stat['runtime_opts'] & RNTM_OPT_GUI:
        with dataif.open("pro", "progress.txt") as hfile:
            hfile.delete()
    with dataif.open("pro", "analysisRequest.json") as hfile:
        hfile.delete()
    try:
        with dataif.open("pro", "engine-error.txt") as hfile:
            error_data = dataif.read()
            dataif.delete()
    except:
        return flask.make_response(
            flask.json.dumps(dict(message='Resource already deleted')),
            410)
    LOGGER.debug("Error data: %s", error_data)
    _translate_afc_error(error_data)
    dataif.rmtmpdir(CACHED_FILES)
    # raise for internal AFC E2yyngine error
    raise AP_Exception(-1, error_data)


def in_progress(task_stat, dataif):
    # get progress and ETA
    if task_stat['runtime_opts'] & RNTM_OPT_GUI:
        progress = None
        try:
            with dataif.open("pro", "progress.txt") as hfile:
                progress = dataif.read()
        except:
            return flask.jsonify(
                percent=0,
                message='Initializing...'
            ), 202
        buff = StringIO.StringIO(progress)
        perc = buff.readline()
        mess = buff.readline()
        if perc and mess:
            return flask.jsonify(
                percent=float(perc),
                message=mess,
            ), 202

    return flask.make_response(
        flask.json.dumps(dict(percent=0, message="Try Again")),
        503)


def success_done(task_stat, dataif):
    resp_data = None
    with dataif.open("pro", "analysisRequest.json") as hfile:
        hfile.delete()
    with dataif.open("pro", "analysisResponse.json.gz") as hfile:
        resp_data = hfile.read()
    if task_stat['runtime_opts'] & RNTM_OPT_DBG:
        with dataif.open("dbg", "analysisResponse.json.gz") as hfile:
            hfile.write(resp_data)
    LOGGER.debug('resp_data size={}'.format(sys.getsizeof(resp_data)))
    resp_data = zlib.decompress(resp_data, 16 + zlib.MAX_WBITS)

    if task_stat['runtime_opts'] & RNTM_OPT_GUI:
        with dataif.open("pro", "progress.txt") as hfile:
            hfile.delete()
        kmz_file = dataif.rname("pro", "results.kmz")
        geo_file = dataif.rname("pro", "mapData.json.gz")

        LOGGER.debug('kmz and geo files: %s %s', kmz_file, geo_file)
        # add the map data file (if it is generated) into a vendor extension
        # will have to revisit when API can handle multiple requests
        resp_json = json.loads(resp_data)
        resp_json["vendorExtensions"] = [{
            "extensionID": "openAfc.mapinfo",
            "parameters": {
                "kmzFile": kmz_file,
                "geoJsonFile": geo_file
            }
        }]
        resp_data = json.dumps(resp_json)

    resp = flask.make_response()
    resp.data = resp_data
    dataif.rmtmpdir(CACHED_FILES)
    return resp


response_map = {
    'SUCCESS' : success_done,
    'FAILURE' : fail_done,
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
        print("RatAfc.get() task_id={}".format(task_id))

        dataif = data_if.DataIf_v1(task_id, None, None, flask.current_app.config['STATE_ROOT_PATH'])
        t = task.task(dataif)
        task_stat = t.get()

        if t.ready(task_stat): # The task is done
            if t.successful(task_stat): # The task is done w/o exception 
                return response_map[task_stat['status']](task_stat, t.dataif)
            else: # The task raised an exception
                raise werkzeug.exceptions.InternalServerError(
                    'Task execution failed')
        else: # The task in progress or pending
            if task_stat['status'] == 'PROGRESS': # The task in progress
                # 'PROGRESS' is task.state value, not task.result['status']
                return response_map['PROGRESS'](task_stat, t.dataif)
            else: # The task yet not started
                LOGGER.debug('RatAfc::get() not ready state: %s', task_stat['status'])
                return flask.make_response(
                    flask.json.dumps(dict(percent=0, message='Pending...')),
                    202)

    def post(self):
        ''' POST method for RAT AFC 
        '''
        LOGGER.debug('RatAfc::post()')
        LOGGER.debug(flask.request.url)

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

        try:
            # authenticate
            device_desc = firstRequest.get('deviceDescriptor')

            try:
                firstCertId = device_desc['certificationId'][0]['nra'] + \
                    ' ' + device_desc['certificationId'][0]['id']
            except:
                firstCertId = None

            user_id = self._auth_ap(device_desc.get('serialNumber'), firstCertId, device_desc.get('rulesetIds'))

            user = User.query.filter_by(id=user_id).first()

            runtime_opts = RNTM_OPT_NODBG_NOGUI
            debug_opt = flask.request.args.get('debug')
            if debug_opt == 'True':
                runtime_opts |= RNTM_OPT_DBG
            gui_opt = flask.request.args.get('gui')
            if gui_opt == 'True':
                runtime_opts |= RNTM_OPT_GUI
            LOGGER.debug('RatAfc::post() runtime %d', runtime_opts)
            if ('FILESTORAGE_HOST' in os.environ and
                    'FILESTORAGE_PORT' in os.environ):
                runtime_opts |= RNTM_OPT_AFCENGINE_HTTP_IO

            history_dir = None
            if runtime_opts & RNTM_OPT_DBG:
                history_dir = user.email + "/" + str(datetime.datetime.now().isoformat())
            dataif = data_if.DataIf_v1(None, user_id, history_dir, flask.current_app.config['STATE_ROOT_PATH'])
            request_json_bytes = json.dumps(args).encode('utf-8')
            config_bytes = None
            with dataif.open("cfg", "afc_config.json") as hfile:
                config_bytes = hfile.read()
            dataif.genId(config_bytes, request_json_bytes)
            with dataif.open("pro", "analysisRequest.json") as hfile:
                hfile.write(request_json_bytes)

            if runtime_opts & RNTM_OPT_DBG:
                with dataif.open("dbg", "analysisRequest.json") as hfile:
                    hfile.write(request_json_bytes)
                with dataif.open("dbg", "afc_config.json") as hfile:
                    hfile.write(config_bytes)

            build_task(dataif.get_id(), request_type, user_id, history_dir, runtime_opts)

            conn_type = flask.request.args.get('conn_type')
            LOGGER.debug("RatAfc:post() conn_type={}".format(conn_type))
            t = task.task(dataif)
            task_stat = t.get()
            if conn_type == 'async':
#                return flask.jsonify(taskId = dataif.get_id(),
#                                     taskState = task.state)
                return flask.jsonify(taskId = dataif.get_id(),
                                     taskState = task_stat["status"])

            # wait for request to finish processing
            try:
                task_stat = t.wait()
            except:
                raise AP_Exception(-1,
                       'The task state is invalid. '
                       'Try again later, and if this issue '
                       'persists contact support.')

            return response_map[task_stat['status']](task_stat, dataif)

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
