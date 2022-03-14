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
import re, datetime
from flask.views import MethodView
import werkzeug.exceptions
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
        super(VersionNotSupportedException, self).__init__(100, 'The requested version number is invalid', { 'invalidVersion': invalid_version })

class DeviceUnallowedException(AP_Exception):
    '''
    101 Name: DEVICE_UNALLOWED
    Interpretation: The provided credentials are invalid
    This specific device as identified by the combination of its FCC ID and unique manufacturer's 
    serial number is not allowed to operate under AFC control due to regulatory action or other action.
    '''

    def __init__(self):
        super(DeviceUnallowedException, self).__init__(101, 'This specific device is not allowed to operate under AFC control.')

class MissingParamException(AP_Exception):
    '''
    102	Name:	MISSING_PARAM
	Interpretation:	One or more fields required to be included in the request are missing.
	Required Action:	Not specified.
	Other Information:	The supplementalInfo field may carry a list of missing parameter names.
    '''

    def __init__(self, missing_params=[]):
        super(MissingParamException, self).__init__(102, 'One or more fields required to be included in the request are missing.', { 'missingParams': missing_params })


class InvalidValueException(AP_Exception):
    '''
    103	Name:	INVALID_VALUE
	Interpretation:	One or more fields have an invalid value.
	Required Action:	Not specified.
	Other Information:	The supplementalInfo field may carry a list of the names of the fields set to invalid value.
    '''

    def __init__(self, invalid_params=[]):
        super(InvalidValueException, self).__init__(103, 'One or more fields have an invalid value.', { 'invalidParams': invalid_params })


class UnexpectedParamException(AP_Exception):
    '''
    106	Name:	UNEXPECTED_PARAM
	Interpretation:	Unknown parameter found, or conditional parameter found, but condition is not met.
	Required Action:	Not specified.
	Other Information:	The supplementalInfo field may carry a list of unexpected parameter names.
    '''

    def __init__(self, unexpected_params=[]):
        super(UnexpectedParamException, self).__init__(106, 'Unknown parameter found, or conditional parameter found, but condition is not met.', { 'unexpectedParams': unexpected_params })


class UnsupportedSpectrumException(AP_Exception):
    '''
    300	Name:	UNSUPPORTED_SPECTRUM
	Interpretation:	The frequency range indicated in the Available Spectrum Inquiry Request is at least partially outside of the frequency band under the management of the AFC (e.g. 5.925-6.425 GHz and 6.525-6.875 GHz bands in US).
	Required Action:	Not specified.
	Other Information:	None
    '''

    def __init__(self):
        super(UnsupportedSpectrumException, self).__init__(300, 'The frequency range indicated in the Available Spectrum Inquiry Request is at least partially outside of the frequency band under the management of the AFC.')


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

class RatAfc(MethodView):
    ''' RAT AFC resources
    '''

    def _auth_ap(self, serial_number, certification_id, rulesets):
        ''' Authenticate an access point. If must match the serial_number and certification_id in the database to be valid
        '''
        LOGGER.debug('Starting auth_ap,serial: %s; certId: %s; ruleset %s.',serial_number,certification_id,rulesets )


        if serial_number is None:
            raise MissingParamException(['serialNumber'])

        ap = AccessPoint.query.filter_by(serial_number=serial_number).first()

        if rulesets is None or len(rulesets) != 1 or rulesets[0] != RULESET:
            raise InvalidValueException(["rulesets"])
        if ap is None:
            raise DeviceUnallowedException() #InvalidCredentialsException()
        if certification_id is None:
            raise MissingParamException(missing_params=['certificationId'])
        if ap.certification_id != certification_id:
            raise DeviceUnallowedException() #InvalidCredentialsException()

        LOGGER.info('Found AP, certifiying User with Id: %i', ap.user_id)
        return ap.user_id


    def post(self):
        ''' POST method for RAT AFC
        '''

        # check request version
        ver = flask.request.json["version"]
        if not (ver == "1.1"):
            raise VersionNotSupportedException([ver])

        # start request
        args = flask.request.json # Get the entire JSON to allow for multiple requests and match v1.1 expected format
        firstRequest = flask.request.json["availableSpectrumInquiryRequests"][0] # as of now, there can be only one request
        LOGGER.debug("Running AFC analysis with params: %s", args)
        request_type = 'AP-AFC'

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
                firstCertId = device_desc['certificationId'][0]['nra'] + ' ' + device_desc['certificationId'][0]['id']
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
                LOGGER.debug('Config file not in vendor extension, using user specified config')
                config_file_path = os.path.join(
                flask.current_app.config['STATE_ROOT_PATH'],
                'afc_config',
                # use scoped user config and fallback to default if not available
                str(user_id) if os.path.exists(os.path.join(
                    flask.current_app.config['STATE_ROOT_PATH'], 'afc_config', str(user_id))) else '',
                'afc_config.json')
                shutil.copy(config_file_path, temp_dir)
            else:
                LOGGER.debug('AP specified config vendor extension. Writing contents to afc_config.json')
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
            


            task = build_task(request_file_path, response_file_path, request_type, 0, user, temp_dir, config_file_path)        

            # wait for request to finish processing
            task.wait()

            if task.successful() and task.result['status'] == 'DONE':
                if not os.path.exists(task.result['result_path']):
                    raise ServerError('Resource already deleted')
                # read temporary file generated by afc-engine
                LOGGER.debug("Reading result file: %s", task.result['result_path'])
                resp_data = None
                with gzip.open(task.result['result_path'], 'rb') as resp_file:
                    resp_data = resp_file.read()
                os.remove(task.result['result_path'])
                #os.remove(task.result['result_path'].replace('.json', '.kmz'))
                resp = flask.make_response()
                LOGGER.debug('resp data: %s', resp_data)
                resp.data = resp_data
                #resp.content_type = 'application/json'
                #resp.content_encoding = 'gzip'
                LOGGER.debug('returning response')
                return resp

            elif task.successful() and task.result['status'] == 'ERROR':
                if not os.path.exists(task.result['error_path']):
                    raise ServerError('Resource already deleted')
                # read temporary file generated by afc-engine
                LOGGER.debug("Reading error file: %s", task.result['error_path'])
                error_data = None
                with open(task.result['error_path'], 'rb') as error_file:
                    error_data = error_file.read()
                LOGGER.debug("Error data: %s", error_data)
                os.remove(task.result['error_path'])

                # try to pick up on specific errors
                _translate_afc_error(error_data)

                # raise for internal AFC Engine error
                raise AP_Exception(-1, error_data) 
            else:
                raise AP_Exception(-1, 'The completed task state is invalid. Try again later, and if this issue persists contact support.')

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



module.add_url_rule('/1.1/availableSpectrumInquiry',
                    view_func=RatAfc.as_view('RatAfc'))

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
