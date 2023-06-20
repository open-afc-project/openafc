#
# This Python file uses the following encoding: utf-8
#
# Portions copyright (C) 2021 Broadcom.
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
import hashlib
import uuid
from flask.views import MethodView
import werkzeug.exceptions
import six
from defs import RNTM_OPT_NODBG_NOGUI, RNTM_OPT_DBG, RNTM_OPT_GUI, \
RNTM_OPT_AFCENGINE_HTTP_IO, RNTM_OPT_NOCACHE, RNTM_OPT_SLOW_DBG, \
RNTM_OPT_CERT_ID
from afc_worker import run
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from afcmodels.aaa import User, AFCConfig, CertId, Ruleset, \
                          Organization, AccessPointDeny
from .auth import auth
from .ratapi import build_task, nraToRegionStr, rulesetIdToRegionStr
from fst import DataIf
import afctask
from .. import als
from afcmodels.base import db
from flask_login import current_user
from .auth import auth
import traceback
from urllib.parse import urlparse

#: Logger for this module
LOGGER = logging.getLogger(__name__)

# We want to dynamically trim this this list e.g.
# via environment variable, for the current deployment
RULESETS = ['US_47_CFR_PART_15_SUBPART_E', 'CA_RES_DBS-06', 'TEST_FCC','DEMO_FCC', "BRAZIL_RULESETID"]

#: All views under this API blueprint
module = flask.Blueprint('ap-afc', 'ap-afc')

ALLOWED_VERSIONS = ['1.4']
RULESET_VERSIONS = ['1.4']

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

    def __init__(self, err_str):
        super(DeviceUnallowedException, self).__init__(
            101, 'This specific device is not allowed to operate under AFC control.' + err_str)


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
        raise VersionNotSupportedException()


def _get_vendor_extension(json_dict, ext_id):
    """ Retrieves given extension from JSON dictionary

    Arguments:
    json_dict -- JSON dictionary, presumably containing or eligible to contain
                 'vendorExtensions' top level field
    ext_id    -- Extemnsion ID to look for
    Returns 'parameters' field of extension with given ID. None if extension
    not found
    """
    try:
        for extension in json_dict.get("vendorExtensions", []):
            if extension["extensionId"] == ext_id:
                return extension["parameters"]
    except (TypeError, ValueError, LookupError):
        pass
    return None


def fail_done(t):
    LOGGER.debug('fail_done()')
    task_stat = t.getStat()
    dataif = t.getDataif()
    error_data = None
    with dataif.open(os.path.join("/responses", t.getId(), "engine-error.txt")) as hfile:
        try:
            error_data = str(hfile.read())
        except:
            LOGGER.debug("engine-error.txt not found")
        else:
            LOGGER.debug("Error data: %s", error_data)
            _translate_afc_error(error_data)
    with dataif.open(os.path.join("/responses", task_stat['hash'])) as hfile:
        hfile.delete()
    with dataif.open(os.path.join("/responses", t.getId())) as hfile:
        hfile.delete()
    # raise for internal AFC E2yyngine error
    raise AP_Exception(-1, error_data)


def in_progress(t):
    # get progress and ETA
    LOGGER.debug('in_progress()')
    task_stat = t.getStat()
    if task_stat['runtime_opts'] & RNTM_OPT_GUI:
        return flask.jsonify(
            percent=0,
            message='In progress...'
        ), 202

    return flask.make_response(
        flask.json.dumps(dict(percent=0, message="Try Again")),
        503)


def success_done(t):
    LOGGER.debug('success_done()')
    task_stat = t.getStat()
    dataif = t.getDataif()
    hash_val = task_stat['hash']

    resp_data = None
    with dataif.open(os.path.join("/responses", hash_val, "analysisResponse.json.gz")) as hfile:
        resp_data = hfile.read()
    if task_stat['runtime_opts'] & RNTM_OPT_DBG:
        with dataif.open(os.path.join(task_stat['history_dir'],
                                      "analysisResponse.json.gz")) as hfile:
            hfile.write(resp_data)
    LOGGER.debug('resp_data size={}'.format(sys.getsizeof(resp_data)))
    resp_data = zlib.decompress(resp_data, 16 + zlib.MAX_WBITS)

    if task_stat['runtime_opts'] & RNTM_OPT_GUI:
        # Add the map data file (if it is generated) into a vendor extension
        kmz_data = None
        try:
            with dataif.open(os.path.join("/responses", t.getId(), "results.kmz")) as hfile:
                kmz_data = hfile.read()
        except:
            pass
        map_data = None
        try:
            with dataif.open(os.path.join("/responses", t.getId(), "mapData.json.gz")) as hfile:
                map_data = hfile.read()
        except:
            pass
        if kmz_data or map_data:
            # TODO: temporary support python2 where kmz_data is of type str
            if isinstance(kmz_data, str):
                kmz_data = kmz_data.encode('base64')
            if isinstance(kmz_data, bytes):
                kmz_data = kmz_data.decode('iso-8859-1')
            resp_json = json.loads(resp_data)
            existingExtensions = resp_json["availableSpectrumInquiryResponses"][0].get("vendorExtensions");
            if(existingExtensions == None):
                resp_json["availableSpectrumInquiryResponses"][0]["vendorExtensions"] = [{
                    "extensionId": "openAfc.mapinfo",
                    "parameters": {
                        "kmzFile": kmz_data if kmz_data else None,
                        "geoJsonFile": zlib.decompress(map_data, 16 + zlib.MAX_WBITS).decode('iso-8859-1') if map_data else None
                    }
                }]
            else:
                 resp_json["availableSpectrumInquiryResponses"][0]["vendorExtensions"].append({
                    "extensionId": "openAfc.mapinfo",
                    "parameters": {
                        "kmzFile": kmz_data if kmz_data else None,
                        "geoJsonFile": zlib.decompress(map_data, 16 + zlib.MAX_WBITS).decode('iso-8859-1') if map_data else None
                    }
                })
            resp_data = json.dumps(resp_json)

    resp = flask.make_response(resp_data)
    resp.content_type = 'application/json'
    LOGGER.debug("returning data: %s", resp.data)
    with dataif.open(os.path.join("/responses", t.getId())) as hfile:
        hfile.delete()
    return resp


response_map = {
    afctask.Task.STAT_SUCCESS: success_done,
    afctask.Task.STAT_FAILURE: fail_done,
    afctask.Task.STAT_PROGRESS: in_progress
}


class AlsConfigInfo:
    """ Information pertinent to ALS Config record known before request
    
    Attributes:
    req_idx     -- Index of request in request message
    config_text -- Content of config file for request
    customer    -- Custmer name for request
    """
    def __init__(self, req_idx, config_text, customer):
        self.config_text = config_text
        self.customer = customer
        self.req_idx = req_idx


class RatAfc(MethodView):
    ''' RAT AFC resources
    '''

    def _auth_cert_id(self, cert_id, ruleset):
        ''' Authenticate certification id. Return new indoor value
            for bell application
        '''
        indoor_certified = True

        certId = CertId.query.filter_by(certification_id=cert_id).first()
        if not certId:
            raise DeviceUnallowedException("")
        if not certId.ruleset.name == ruleset:
            raise InvalidValueException(["ruleset", ruleset])
        # add check for certId.valid

        now = datetime.datetime.now()
        delta = now - certId.refreshed_at
        if delta.days > 1:
            LOGGER.debug('RatAfc::_auth_cert_id(): stale CertId %s', certId.certification_id )

        if not certId.location & certId.OUTDOOR:
            raise DeviceUnallowedException("Outdoor operation not allowed")
        elif certId.location & certId.INDOOR:
            indoor_certified = True
        else:
            indoor_certified = False

        return indoor_certified

    def _auth_ap(self, serial_number, prefix, cert_id, rulesets, version):
        ''' Authenticate an access point. If must match the serial_number and certification_id in the database to be valid
        '''
        LOGGER.debug('RatAfc::_auth_ap()')
        LOGGER.debug('Starting auth_ap,serial: %s; prefix: %s; certId: %s; ruleset %s; version %s',
                     serial_number, prefix, cert_id, rulesets, version)

        deny_ap = AccessPointDeny.query.filter_by(serial_number=serial_number).\
                  filter_by(certification_id=cert_id).first()
        if deny_ap:
            raise DeviceUnallowedException("")  # InvalidCredentialsException()

        deny_ap = AccessPointDeny.query.filter_by(certification_id=cert_id).\
                  filter_by(serial_number=None).first()
        if deny_ap:
            # denied all devices matching certification id
            raise DeviceUnallowedException("")  # InvalidCredentialsException()

        ruleset = prefix

        # Test AP will by pass certification ID check as Indoor Certified
        if cert_id == "TestCertificationId" \
           and serial_number == "TestSerialNumber":
            return True

        # Assume that once we got here, we already trim the cert_obj list down
        # to only one entry for the country we're operating in
        return self._auth_cert_id(cert_id, ruleset)


    def __filter(self, url, json):
        """ Check correspondence of URL to request file.
        Return true if the request should be filtered."""
        urlp = urlparse(url)
        if urlp.path.endswith("/availableSpectrumInquiryInternal"):
            # It's internal test
            return False
        if "availableSpectrumInquiryRequests" not in json:
            # No requests in the json
            return False
        requests = json["availableSpectrumInquiryRequests"]
        for request in requests:
            if "vendorExtensions" not in request:
                break
            extensions = request["vendorExtensions"]
            for extension in extensions:
                if "extensionId" not in extension:
                    break;
                if (extension["extensionId"] == "OpenAfcOverrideAfcConfig"):
                    return True
        return False

    def get(self):
        ''' GET method for Analysis Status '''
        LOGGER.debug('RatAfc::get()')

        task_id = flask.request.args['task_id']
        LOGGER.debug("RatAfc.get() task_id={}".format(task_id))

        dataif = DataIf()
        t = afctask.Task(task_id, dataif)
        task_stat = t.get()

        if t.ready(task_stat):  # The task is done
            if t.successful(task_stat):  # The task is done w/o exception
                return response_map[task_stat['status']](t)
            else:  # The task raised an exception
                raise werkzeug.exceptions.InternalServerError(
                    'Task execution failed')
        else:  # The task in progress or pending
            if task_stat['status'] == afctask.Task.STAT_PROGRESS:  # The task in progress
                # 'PROGRESS' is task.state value, not task.result['status']
                return response_map[afctask.Task.STAT_PROGRESS](t)
            else:  # The task yet not started
                LOGGER.debug('RatAfc::get() not ready state: %s',
                             task_stat['status'])
                return flask.make_response(
                    flask.json.dumps(dict(percent=0, message='Pending...')),
                    202)

    def post(self):
        ''' POST method for RAT AFC
        '''
        LOGGER.debug('RatAfc::post() from {}'.format(flask.request.url))

        if self.__filter(flask.request.url, flask.request.json):
            LOGGER.debug("request filtered out")
            raise RuntimeError("Wrong analysisRequest.json from {}".format(flask.request.url))

        # check request version
        ver = flask.request.json["version"]
        if not (ver in ALLOWED_VERSIONS):
            raise VersionNotSupportedException([ver])

        # start request
        args = flask.request.json
        # split multiple requests into an array of individual requests
        requests = map(lambda r: {"availableSpectrumInquiryRequests": [r], "version": ver}, args["availableSpectrumInquiryRequests"])
        LOGGER.debug("Running AFC analysis with params: %s", args)
        request_type = 'AP-AFC'

        results = {"availableSpectrumInquiryResponses": [], "version": ver}

        als_req_id = als.als_afc_req_id()
        als.als_afc_request(req_id=als_req_id, req=args)
        tasks = []
        als_config_infos = []
        uls_id = "Unknown"
        geo_id = "Unknown"
        indoor_certified = True

        try:
            for req_idx, request in enumerate(requests):
                # authenticate
                LOGGER.debug("Request: %s", request)
                device_desc = request["availableSpectrumInquiryRequests"][0].get(
                    'deviceDescriptor')

                devices = device_desc.get('certificationId')
                if not devices:
                    raise MissingParamException(missing_params=['certificationId'])

                # Pick one ruleset that is being used for the
                # deployment of this AFC (RULESETS)
                prefix = None
                certId = None
                region = None
                for r in devices:
                    prefix = r.get('rulesetId')
                    if not prefix:
                        # empty/non-exist ruleset
                        break
                    if prefix in RULESETS:
                        region = rulesetIdToRegionStr(prefix)
                        certId = r.get('id')
                        break
                    prefix = prefix.strip()

                if prefix is None: 
                    raise MissingParamException(missing_params=['rulesetId'])
                elif not prefix:
                   raise InvalidValueException(["rulesetId", prefix])

                if region:
                    if certId is None:
                        # certificationId id field does not exist
                        raise MissingParamException(missing_params=['certificationId', 'id'])
                    elif not certId:
                        raise InvalidValueException(["certificationId", certId])
                else:
                   # ruleset is not in list
                   raise DeviceUnallowedException("")

                serial = device_desc.get('serialNumber')
                if serial is None:
                    raise MissingParamException(missing_params=['serialNumber'])
                elif not serial:
                    raise InvalidValueException(["serialNumber", serial])

                location = request["availableSpectrumInquiryRequests"][0].get(
                    'location')

                indoor_certified = self._auth_ap(serial,
                                   prefix,
                                   certId,
                                   device_desc.get('rulesetIds'),
                                   ver)

                if indoor_certified:
                    runtime_opts = RNTM_OPT_CERT_ID | RNTM_OPT_NODBG_NOGUI
                else:
                    runtime_opts = RNTM_OPT_NODBG_NOGUI

                debug_opt = flask.request.args.get('debug')
                if debug_opt == 'True':
                    runtime_opts |= RNTM_OPT_DBG
                edebug_opt = flask.request.args.get('edebug')
                if edebug_opt == 'True':
                    runtime_opts |= RNTM_OPT_SLOW_DBG
                gui_opt = flask.request.args.get('gui')
                if gui_opt == 'True':
                    runtime_opts |= RNTM_OPT_GUI
                opt = flask.request.args.get('nocache')
                if opt == 'True':
                    runtime_opts |= RNTM_OPT_NOCACHE
                runtime_opts |= RNTM_OPT_AFCENGINE_HTTP_IO
                LOGGER.debug('RatAfc::post() runtime %d', runtime_opts)

                task_id = str(uuid.uuid4())
                dataif = DataIf()

                config = AFCConfig.query.filter(AFCConfig.config['regionStr'].astext \
                         == region).first()

                if not config:
                    raise DeviceUnallowedException("No AFC configuration for ruleset")

                config_bytes = json.dumps(config.config, sort_keys=True)

                # calculate hash of config
                hashlibobj = hashlib.md5()
                hashlibobj.update(config_bytes.encode('utf-8'))
                hash1 = hashlibobj.hexdigest()
                config_path = os.path.join("/afc_config", prefix, hash1, "afc_config.json")

                # check config_path exist
                with dataif.open(config_path) as hfile:
                    if not hfile.head():
                        # Write afconfig to objst cache.
                        # convert TESTxxx region to xxx in cache to make afc engine sane
                        if region[:5] == "TEST_" or region[:5] == 'DEMO_':
                            patch_config = config.config
                            patch_config['regionStr'] =  region[5:]
                            config_bytes = json.dumps(patch_config, sort_keys=True)
                        hfile.write(config_bytes)

                # calculate hash of config + request
                request_json_bytes = json.dumps(request, sort_keys=True).encode('utf-8')
                hashlibobj.update(request_json_bytes)
                hash_val = hashlibobj.hexdigest()

                # check cache
                if not runtime_opts & (RNTM_OPT_GUI | RNTM_OPT_NOCACHE):
                    try:
                        with dataif.open(os.path.join(
                            "/responses", hash_val, "analysisResponse.json.gz")) as hfile:
                            resp_data = hfile.read()
                    except:
                        pass
                    else:
                        resp_data = zlib.decompress(resp_data, 16 + zlib.MAX_WBITS)
                        dataAsJson = json.loads(resp_data)
                        LOGGER.debug("dataAsJson: %s", dataAsJson)
                        actualResult = dataAsJson.get(
                            "availableSpectrumInquiryResponses")
                        results["availableSpectrumInquiryResponses"].append(
                            actualResult[0])
                        continue

                with dataif.open(os.path.join("/responses", hash_val, "analysisRequest.json")) as hfile:
                    hfile.write(request_json_bytes)
                history_dir = None
                if runtime_opts & (RNTM_OPT_DBG | RNTM_OPT_SLOW_DBG):
                    history_dir = os.path.join("/history", str(serial),
                                               str(datetime.datetime.now().isoformat()))
                if runtime_opts & RNTM_OPT_DBG:
                    with dataif.open(os.path.join(history_dir, "analysisRequest.json")) as hfile:
                        hfile.write(request_json_bytes)
                    with dataif.open(os.path.join(history_dir, "afc_config.json")) as hfile:
                        hfile.write(config_bytes.encode('utf-8'))
                build_task(dataif,
                        request_type,
                        task_id, hash_val, config_path, history_dir,
                        runtime_opts)

                conn_type = flask.request.args.get('conn_type')
                LOGGER.debug("RatAfc:post() conn_type={}".format(conn_type))
                t = afctask.Task(task_id, dataif, hash_val, history_dir)
                if conn_type == 'async':
                    if len(requests) > 1:
                        raise AP_Exception(-1, "Unsupported multipart async request")
                    task_stat = t.get()
                    # async request comes from GUI only, so it can't be multi nor cached
                    return flask.jsonify(taskId = task_id,
                                     taskState = task_stat["status"])
                tasks.append(t)
                als_config_infos.append(
                    AlsConfigInfo(req_idx=req_idx, config_text=config_bytes,
                                  customer=region))

            for task_idx, t in enumerate(tasks):
                # wait for requests to finish processing
                uls_id = "Unknown"
                geo_id = "Unknown"
                try:
                    task_stat = t.wait()
                    LOGGER.debug("Task complete: %s", task_stat)
                    if t.successful(task_stat):
                        taskResponse = response_map[task_stat['status']](t)
                        # we might be able to clean this up by having the result functions not return a full response object
                        # need to check everywhere they are called
                        dataAsJson = json.loads(taskResponse.data)
                        LOGGER.debug("dataAsJson: %s", dataAsJson)
                        engine_result_ext = \
                            _get_vendor_extension(dataAsJson,
                                                  "openAfc.used_data")
                        uls_id = (engine_result_ext or {}).get("uls_id",
                                                               uls_id)
                        geo_id = (engine_result_ext or {}).get("geo_id",
                                                               geo_id)
                        actualResult = dataAsJson.get(
                            "availableSpectrumInquiryResponses")
                        if actualResult is not None:
                            # LOGGER.debug("actualResult: %s", actualResult)
                            results["availableSpectrumInquiryResponses"].append(
                                actualResult[0])
                        else:
                            LOGGER.debug("actualResult was None")
                            results["availableSpectrumInquiryResponses"].append(
                                dataAsJson)
                    else:
                        LOGGER.debug("Task was not successful")
                        taskResponse = response_map[task_stat['status']](t)
                        dataAsJson = json.loads(taskResponse.data)
                        LOGGER.debug(
                            "Unsuccessful dataAsJson: %s", dataAsJson)
                        results["availableSpectrumInquiryResponses"].append(
                            dataAsJson)

                except Exception as e:
                    LOGGER.error('catching and rethrowing exception: %s',
                                 getattr(e, 'message', repr(e)))
                    LOGGER.debug(''.join(traceback.format_exception(None, e, e.__traceback__)))
                    raise AP_Exception(-1,
                                       'The task state is invalid. '
                                       'Try again later, and if this issue '
                                       'persists contact support.')
                finally:
                    config_info = als_config_infos[task_idx]
                    als.als_afc_config(
                        req_id=als_req_id, config_text=config_info.config_text,
                        customer=config_info.customer, geo_data_version=geo_id,
                        uls_id=uls_id, req_indices=[config_info.req_idx])
                    

        except AP_Exception as e:
            LOGGER.error('catching exception: %s',
                         getattr(e, 'message', repr(e)))
            results["availableSpectrumInquiryResponses"].append(
                {
                    'requestId': request["availableSpectrumInquiryRequests"][0]["requestId"],
                    'response': {
                        'responseCode': e.response_code,
                        'shortDescription': e.description,
                        'supplementalInfo': json.dumps(e.supplemental_info) if e.supplemental_info is not None else None
                    }
                })
        als.als_afc_response(req_id=als_req_id, resp=results)
        LOGGER.error("Final results: %s", str(results))
        resp = flask.make_response(flask.json.dumps(results), 200)
        resp.content_type = 'application/json'
        return resp


class RatAfcSec(RatAfc):
    def get(self):
        user_id = auth(roles=['Analysis', 'Trial', 'Admin'])
        return super().get()

    def post(self):
        user_id = auth(roles=['Analysis', 'Trial', 'Admin'])
        return super().post()

class RatAfcInternal(MethodView):
    """ Allow to regiter second URL on RatAfc """
    pass

# registration of default runtime options

module.add_url_rule('/availableSpectrumInquirySec',
                    view_func=RatAfcSec.as_view('RatAfcSec'))

module.add_url_rule('/availableSpectrumInquiry',
                    view_func=RatAfc.as_view('RatAfc'))

module.add_url_rule('/availableSpectrumInquiryInternal',
                    view_func=RatAfc.as_view('RatAfcInternal'))

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
