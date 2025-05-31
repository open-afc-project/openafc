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
import uuid
import copy
import time
import platform
from flask.views import MethodView
import werkzeug.exceptions
import threading
import inspect
from typing import NamedTuple, Optional
import six
from appcfg import RatafcMsghndCfgIface, AFC_RATAPI_LOG_LEVEL
from hchecks import RmqHealthcheck
from defs import RNTM_OPT_NODBG_NOGUI, RNTM_OPT_DBG, RNTM_OPT_GUI, \
    RNTM_OPT_AFCENGINE_HTTP_IO, RNTM_OPT_NOCACHE, RNTM_OPT_SLOW_DBG, \
    RNTM_OPT_CERT_ID
from afc_worker import run
from ..util import AFCEngineException, require_default_uls, getQueueDirectory
from afcmodels.aaa import User, AFCConfig, CertId, Ruleset, \
    Organization, AccessPointDeny
from afcmodels.hardcoded_relations import RulesetVsRegion, \
    SpecialCertifications, VendorExtensionFilter, CERT_ID_LOCATION_UNKNOWN, \
    CERT_ID_LOCATION_OUTDOOR, CERT_ID_LOCATION_INDOOR
from .auth import auth
from .ratapi import build_task
from fst import DataIf
import afctask
import als
from afcmodels.base import db
from flask_login import current_user
from .auth import auth
import traceback
from urllib.parse import urlparse
from typing import Any, Dict, NamedTuple, Optional
from rcache_models import RcacheClientSettings
from rcache_client import RcacheClient
from rcache_req_cfg_hash import RequestConfigHash
from rcache_db import RcacheLookupResult
import prometheus_client
import prometheus_utils
import afc_traffic_metrics

LOGGER = logging.getLogger(__name__)
LOGGER.setLevel(AFC_RATAPI_LOG_LEVEL)

# Metrics for autoscaling - only works in gunicorn (not on apache)
if prometheus_utils.multiprocess_prometheus_configured():
    prometheus_metric_flask_afc_waiting_reqs = \
        prometheus_client.Gauge('msghnd_flask_afc_waiting_reqs',
                                'Number of requests waiting for afc engine',
                                ['host'], multiprocess_mode='sum')
    prometheus_metric_flask_afc_waiting_reqs = \
        prometheus_metric_flask_afc_waiting_reqs.labels(host=platform.node())
else:
    prometheus_metric_flask_afc_waiting_reqs = None

# We want to dynamically trim this this list e.g.
# via environment variable, for the current deployment
RULESETS = RulesetVsRegion.ruleset_list()

#: All views under this API blueprint
module = flask.Blueprint('ap-afc', 'ap-afc')

ALLOWED_VERSIONS = ['1.4']


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
# 	Other Information:	This responseCode value is returned when, for example, AFC receives a lot of requests or a large request and
#        AFC cannot complete the process soon.
#     '''

#     def __init__(self, wait_time_sec=3600):
#         super(TempNotProcessedException, self).__init__(101, 'The request could not be processed at this time. Try again later.',
#         { 'waitTime': wait_time_sec })

class VersionNotSupportedException(AP_Exception):
    '''
    100 Name: VERSION_NOT_SUPPORTED
    Interpretation: The requested version number is invalid
    The communication can be attempted again using a different version number. In the case of an AP attempting
    to communicate with an AFC, communications with the same version but a different AFC could be attempted
    '''

    def __init__(self, invalid_version=[]):
        super(
            VersionNotSupportedException, self).__init__(
            100, 'The requested version number is invalid', {
                'invalidVersion': invalid_version})


class DeviceUnallowedException(AP_Exception):
    '''
    101 Name: DEVICE_UNALLOWED
    Interpretation: The provided credentials are invalid
    This specific device as identified by the combination of its FCC ID and unique manufacturer's
    serial number is not allowed to operate under AFC control due to regulatory action or other action.
    '''

    def __init__(self, err_str):
        super(
            DeviceUnallowedException,
            self).__init__(
            101,
            'This specific device is not allowed to operate under AFC control.' +
            err_str)


class MissingParamException(AP_Exception):
    '''
    102	Name:	MISSING_PARAM
        Interpretation:	One or more fields required to be included in the request are missing.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of missing parameter names.
    '''

    def __init__(self, missing_params=[]):
        super(
            MissingParamException, self).__init__(
            102, 'One or more fields required to be included in the request are missing.', {
                'missingParams': missing_params})


class InvalidValueException(AP_Exception):
    '''
    103	Name:	INVALID_VALUE
        Interpretation:	One or more fields have an invalid value.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of the names of the fields set to invalid value.
    '''

    def __init__(self, invalid_params=[]):
        super(
            InvalidValueException, self).__init__(
            103, 'One or more fields have an invalid value.', {
                'invalidParams': invalid_params})


class UnexpectedParamException(AP_Exception):
    '''
    106	Name:	UNEXPECTED_PARAM
        Interpretation:	Unknown parameter found, or conditional parameter found, but condition is not met.
        Required Action:	Not specified.
        Other Information:	The supplementalInfo field may carry a list of unexpected parameter names.
    '''

    def __init__(self, unexpected_params=[]):
        super(
            UnexpectedParamException, self).__init__(
            106, 'Unknown parameter found, or conditional parameter found, but condition is not met.', {
                'unexpectedParams': unexpected_params})


class UnsupportedSpectrumException(AP_Exception):
    '''
    300	Name:	UNSUPPORTED_SPECTRUM
        Interpretation:	The frequency range indicated in the Available Spectrum Inquiry Request is at least partially outside of the frequency band
                         under the management of the AFC (e.g. 5.925-6.425 GHz and 6.525-6.875 GHz bands in US).
        Required Action:	Not specified.
        Other Information:	None
    '''

    def __init__(self):
        super(
            UnsupportedSpectrumException,
            self).__init__(
            300,
            'The frequency range indicated in the Available Spectrum Inquiry Request is at least partially '
            'outside of the frequency band under the management of the AFC.')


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


def drop_unwanted_extensions(json_dict, is_input, is_gui, is_internal):
    """ Removes unallowed vendor extensions from given message

    Arguments:
    json_dict   -- AFC Request/Response message in dictionary form
    is_input    -- True for request, False for response
    is_gui      -- True if from GUI
    is_internal -- True if internal (from within AFC Cluster)
    """
    for container, is_message in [(json_dict, True)] + \
            [(req_resp, False) for req_resp in
             json_dict.get("availableSpectrumInquiryRequests" if is_input
                           else "availableSpectrumInquiryResponses", [])]:
        extensions = container.get("vendorExtensions")
        if extensions is None:
            continue
        idx = 0
        while idx < len(extensions):
            if VendorExtensionFilter.allowed_extension(
                    extension=extensions[idx]["extensionId"],
                    is_message=is_message, is_input=is_input, is_gui=is_gui,
                    is_internal=is_internal):
                idx += 1
            else:
                extensions.pop(idx)
        if not extensions:
            del container["vendorExtensions"]


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
            error_data = hfile.read().decode("utf-8", errors="replace")
        except BaseException:
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
    if not isinstance(task_stat['runtime_opts'], type(None)) and \
            task_stat['runtime_opts'] & RNTM_OPT_GUI:
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
    LOGGER.debug('resp_data size=%d', sys.getsizeof(resp_data))
    resp_json = json.loads(zlib.decompress(resp_data, 16 + zlib.MAX_WBITS))

    # if task_stat['runtime_opts'] & RNTM_OPT_GUI:
    # Add the map data file (if it is generated) into a vendor extension
    kmz_data = None
    try:
        with dataif.open(os.path.join("/responses", t.getId(), "results.kmz")) as hfile:
            kmz_data = hfile.read()
    except BaseException:
        pass
    map_data = None
    try:
        with dataif.open(os.path.join("/responses", t.getId(), "mapData.json.gz")) as hfile:
            map_data = hfile.read()
    except BaseException:
        pass
    if kmz_data or map_data:
        # TODO: temporary support python2 where kmz_data is of type str
        if isinstance(kmz_data, str):
            kmz_data = kmz_data.encode('base64')
        if isinstance(kmz_data, bytes):
            kmz_data = kmz_data.decode('iso-8859-1')
        resp_json["availableSpectrumInquiryResponses"][0].setdefault(
            "vendorExtensions",
            []).append(
            {
                "extensionId": "openAfc.mapinfo",
                "parameters": {
                    "kmzFile": kmz_data if kmz_data else None,
                    "geoJsonFile": zlib.decompress(
                        map_data,
                        16 +
                        zlib.MAX_WBITS).decode('iso-8859-1') if map_data else None}})
    drop_unwanted_extensions(
        json_dict=resp_json, is_input=False,
        is_gui=bool(task_stat['runtime_opts'] & RNTM_OPT_GUI),
        is_internal=bool(task_stat['is_internal_request']))
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

if os.environ.get('RCACHE_ENABLED', '').lower() in ('0', 'no', 'false', '-'):
    rcache = None
else:
    rcache_settings = RcacheClientSettings()
    # In this validation rcache is True to handle 'not update_on_send' case
    rcache_settings.validate_for(db=True, rmq=True, rcache=True)
    rcache = RcacheClient(rcache_settings, rmq_receiver=True) \
        if rcache_settings.enabled else None


class ReqInfo(NamedTuple):
    """ Information about single AFC Request, needed for its processing """
    # 0-based request index within message
    req_idx: int

    # Hash computed over essential part of request and of AFC config
    req_cfg_hash: str

    # Request ID from message
    request_id: str

    # AFC Request as dictionary - used for compitation
    request: Dict[str, Any]

    # AFC Config file content as string
    config_str: str

    # AFC Config file path. None if tasks not used
    config_path: Optional[str]

    # Region identifier
    region: str

    # Root directory for AFC Engine debug files
    history_dir: Optional[str]

    # AFC Engine Request type
    request_type: str

    # AFC Engine computation options
    runtime_opts: int

    # Task ID. Objstore directory name for AFC Engine artifacts
    task_id: str


class RatAfc(MethodView):
    ''' RAT AFC resources
    '''

    def _auth_cert_id(self, cert_id, ruleset, session):
        ''' Authenticate certification id. Return new indoor value
            for bell application
        '''
        LOGGER.debug("(%d) %s::%s()", threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3])
        indoor_certified = True
        certId = session.query(CertId).filter_by(certification_id=cert_id).first()
        if not certId:
            raise DeviceUnallowedException("")
        if not certId.ruleset.name == ruleset:
            raise InvalidValueException(["ruleset", ruleset])
        # add check for certId.valid

        now = datetime.datetime.now()
        delta = now - certId.refreshed_at
        if delta.days > 1:
            LOGGER.debug("(%d) %s::%s() stale CertId %s",
                         threading.get_native_id(),
                         self.__class__, inspect.stack()[0][3],
                         certId.certification_id)

        if certId.location & CERT_ID_LOCATION_INDOOR:
            indoor_certified = True
        else:
            indoor_certified = False

        return indoor_certified

    def _auth_ap(self, serial_number, prefix, cert_id, rulesets, version):
        ''' Authenticate an access point. If must match the serial_number and
            certification_id in the database to be valid
        '''
        LOGGER.debug("(%d) %s::%s()Starting auth_ap,serial: %s; prefix: %s; "
                     "certId: %s ruleset %s; version %s",
                     threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3],
                     serial_number, prefix, cert_id, rulesets, version)
        with db.session() as session:
            deny_ap = session.query(AccessPointDeny).filter_by(
                serial_number=serial_number). filter_by(
                certification_id=cert_id).first()
            if deny_ap:
                raise DeviceUnallowedException("")  # InvalidCredentialsException()

            deny_ap = session.query(AccessPointDeny).filter_by(certification_id=cert_id).\
                filter_by(serial_number=None).first()
            if deny_ap:
                # denied all devices matching certification id
                raise DeviceUnallowedException("")  # InvalidCredentialsException()

            ruleset = prefix

            scp = \
                SpecialCertifications.get_properties(
                    cert_id=cert_id, serial_number=serial_number)
            if scp is not None:
                return (scp.location_flags & CERT_ID_LOCATION_INDOOR) != 0

            # Assume that once we got here, we already trim the cert_obj list
            # down to only one entry for the country we're operating in
            return self._auth_cert_id(cert_id, ruleset, session)

    def get(self):
        ''' GET method for Analysis Status '''
        task_id = flask.request.args['task_id']
        LOGGER.debug("(%d) %s::%s() task_id=%s", threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3], task_id)

        dataif = DataIf()
        t = afctask.Task(
            task_id, dataif,
            flask.current_app.config['AFC_MSGHND_RATAFC_TOUT'])
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
                LOGGER.debug("(%d) %s::%s() not ready state: %s",
                             threading.get_native_id(),
                             self.__class__, inspect.stack()[0][3],
                             task_stat['status'])
                return flask.make_response(
                    flask.json.dumps(dict(percent=0, message='Pending...')),
                    202)

    def post(self):
        ''' POST method for RAT AFC
        '''
        flask.g.is_afc = True
        LOGGER.debug("(%d) %s::%s()", threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3])
        als_req_id = als.als_afc_req_id()
        results = {"availableSpectrumInquiryResponses": []}
        request_ids = set()
        dataif = DataIf()

        try:
            is_internal_request = urlparse(flask.request.url).path.\
                endswith("/availableSpectrumInquiryInternal")
            is_gui = flask.request.args.get('gui') == 'True'
            drop_unwanted_extensions(
                json_dict=flask.request.json, is_input=True, is_gui=is_gui,
                is_internal=is_internal_request)
            # start request
            args = flask.request.json
            # check request version
            ver = flask.request.json["version"]
            if not (ver in ALLOWED_VERSIONS):
                raise VersionNotSupportedException([ver])
            results["version"] = ver

            # split multiple requests into an array of individual requests
            requests = map(
                lambda r: {
                    "availableSpectrumInquiryRequests": [r],
                    "version": ver},
                args["availableSpectrumInquiryRequests"])
            request_ids = \
                set([r["requestId"]
                     for r in args["availableSpectrumInquiryRequests"]])

            LOGGER.debug("(%d) %s::%s() Running AFC analysis with params: %s",
                         threading.get_native_id(),
                         self.__class__, inspect.stack()[0][3], args)
            request_type = 'AP-AFC'

            use_tasks = (rcache is None) or \
                (flask.request.args.get('conn_type') == 'async') or is_gui

            message_runtime_opts = 0
            for arg, mask in [('gui', RNTM_OPT_GUI), ('debug', RNTM_OPT_DBG),
                              ('edebug', RNTM_OPT_SLOW_DBG),
                              ('nocache', RNTM_OPT_NOCACHE)]:
                if flask.request.args.get(arg):
                    message_runtime_opts |= mask

            als.als_afc_request(req_id=als_req_id, req=args,
                                mtls_dn=flask.request.headers.get('mTLS-DN'),
                                runtime_opt=message_runtime_opts,
                                ap_ip=flask.request.headers.get('X-Real-IP'))
            uls_id = "Unknown"
            geo_id = "Unknown"
            indoor_certified = True
            req_infos = {}

            # Creating req_infos - list of ReqInfo objects
            for req_idx, request in enumerate(requests):
                try:
                    # authenticate
                    LOGGER.debug("(%d) %s::%s() Request: %s",
                                 threading.get_native_id(),
                                 self.__class__, inspect.stack()[0][3],
                                 request)
                    individual_request = \
                        request["availableSpectrumInquiryRequests"][0]
                    prefix = None
                    device_desc = individual_request.get('deviceDescriptor')

                    devices = device_desc.get('certificationId')
                    if not devices:
                        raise MissingParamException(
                            missing_params=['certificationId'])

                    # Pick one ruleset that is being used for the
                    # deployment of this AFC (RULESETS)
                    certId = None
                    region = None
                    for r in devices:
                        prefix = r.get('rulesetId')
                        if not prefix:
                            # empty/non-exist ruleset
                            break
                        if prefix in RULESETS:
                            region = \
                                RulesetVsRegion.ruleset_to_region(
                                    prefix, exc=werkzeug.exceptions.NotFound)
                            certId = r.get('id')
                            break
                        prefix = prefix.strip()

                    if prefix is None:
                        raise MissingParamException(
                            missing_params=['rulesetId'])
                    elif not prefix:
                        raise InvalidValueException(["rulesetId", prefix])

                    if region:
                        if certId is None:
                            # certificationId id field does not exist
                            raise MissingParamException(
                                missing_params=['certificationId', 'id'])
                        elif not certId:
                            raise InvalidValueException(
                                ["id", certId])
                    else:
                        # ruleset is not in list
                        raise DeviceUnallowedException("")

                    serial = device_desc.get('serialNumber')
                    if serial is None:
                        raise MissingParamException(
                            missing_params=['serialNumber'])
                    elif not serial:
                        raise InvalidValueException(["serialNumber", serial])

                    runtime_opts = message_runtime_opts

                    indoor_certified = \
                        self._auth_ap(serial, prefix, certId,
                                      device_desc.get('rulesetIds'), ver)
                    if indoor_certified:
                        runtime_opts |= RNTM_OPT_CERT_ID
                    if use_tasks:
                        runtime_opts |= RNTM_OPT_AFCENGINE_HTTP_IO

                    LOGGER.debug("(%d) %s::%s() runtime %d",
                                 threading.get_native_id(),
                                 self.__class__, inspect.stack()[0][3],
                                 runtime_opts)

                    # Retrieving config
                    with db.session() as session:
                        config = session.query(AFCConfig).filter(
                            AFCConfig.config['regionStr'].astext == region).first()
                    if not config:
                        raise DeviceUnallowedException(
                            "No AFC configuration for ruleset " + str(region))
                    afc_config = config.config
                    try:
                        overwrite_region = \
                            RulesetVsRegion.overwrite_region(
                                region, exc=KeyError)
                        if overwrite_region:
                            afc_config = dict(afc_config)
                            afc_config['regionStr'] = overwrite_region
                    except KeyError:
                        pass

                    rcc = RequestConfigHash(req_dict=individual_request,
                                            afc_config_dict=afc_config,
                                            compute_config_hash=True)
                    config_path = \
                        os.path.join("/afc_config", prefix, rcc.cfg_hash,
                                     "afc_config.json") if use_tasks else None
                    history_dir = None
                    if runtime_opts & (RNTM_OPT_DBG | RNTM_OPT_SLOW_DBG):
                        history_dir =\
                            os.path.join(
                                "/history", str(serial),
                                str(datetime.datetime.now().isoformat()))
                    req_infos[rcc.req_cfg_hash] = \
                        ReqInfo(
                            req_idx=req_idx, req_cfg_hash=rcc.req_cfg_hash,
                            request_id=individual_request["requestId"],
                            request=request, config_str=rcc.cfg_str,
                            config_path=config_path, region=region,
                            history_dir=history_dir, request_type=request_type,
                            runtime_opts=runtime_opts,
                            task_id=str(uuid.uuid4()))
                except AP_Exception as e:
                    response_dict = {"responseCode": e.response_code,
                                     "shortDescription": e.description}
                    if e.supplemental_info:
                        response_dict["supplementalInfo"] = e.supplemental_info
                    results["availableSpectrumInquiryResponses"].append(
                        {"requestId": individual_request["requestId"],
                         "rulesetId": prefix or "Unknown",
                         "response": response_dict})
                    afc_traffic_metrics.request_processed(
                        duration_sec=time.time() - flask.g.afc_start_time,
                        response="Exception")

            # Creating 'responses' - dictionary of responses as JSON strings,
            # indexed by request/config hashes
            # First looking up in the cache
            invalidated_responses = {}
            responses = {}
            for req_cfg_hash, lookup_result in \
                    self._cache_lookup(dataif=dataif, req_infos=req_infos).\
                    items():
                (responses if lookup_result.found else invalidated_responses)[
                    req_cfg_hash] = lookup_result.response
            cached_req_hashes = set(responses.keys())
            cached_duration = time.time() - flask.g.afc_start_time

            # Computing the responses not found in cache
            # First handling async case
            if len(responses) != len(req_infos):
                if flask.request.args.get('conn_type') == 'async':
                    # Special case of async request
                    if len(req_infos) > 1:
                        raise \
                            AP_Exception(-1,
                                         "Unsupported multipart async request")
                    task = \
                        self._start_processing(
                            dataif=dataif,
                            req_info=list(req_infos.values())[0],
                            is_internal_request=is_internal_request,
                            use_tasks=True)
                    # No ALS logging for config/response (request is logged
                    # though). Can be fixed if anybody cares
                    return flask.jsonify(taskId=task.getId(),
                                         taskState=task.get()["status"])
                with prometheus_metric_flask_afc_waiting_reqs.track_inprogress() \
                        if prometheus_metric_flask_afc_waiting_reqs \
                        else contextlib.nullcontext():
                    responses.update(
                        self._compute_responses(
                            dataif=dataif, use_tasks=use_tasks,
                            req_infos={k: v for k, v in req_infos.items()
                                       if k not in responses},
                            invalidated_responses=invalidated_responses,
                            is_internal_request=is_internal_request))
            computed_duration = time.time() - flask.g.afc_start_time

            # Preparing responses for requests
            for req_info in req_infos.values():
                response = responses.get(req_info.req_cfg_hash)
                if not response:
                    # No response for request
                    continue
                dataAsJson = json.loads(response)
                actualResult = dataAsJson.get(
                    "availableSpectrumInquiryResponses")
                if actualResult is not None:
                    engine_result_ext = \
                        _get_vendor_extension(actualResult[0],
                                              "openAfc.used_data") or {}
                    uls_id = engine_result_ext.get("uls_id", uls_id)
                    geo_id = engine_result_ext.get("geo_id", geo_id)
                    actualResult[0]["requestId"] = req_info.request_id
                    results["availableSpectrumInquiryResponses"].append(
                        actualResult[0])
                als.als_afc_config(
                    req_id=als_req_id, config_text=req_info.config_str,
                    customer=req_info.region, geo_data_version=geo_id,
                    uls_id=uls_id, req_indices=[req_info.req_idx])
                afc_traffic_metrics.request_processed(
                    duration_sec=cached_duration
                    if req_info.req_cfg_hash in cached_req_hashes
                    else computed_duration,
                    response=actualResult[0].get("response", {}).
                    get("responseCode", "Error") if actualResult
                    else "NotComputed")
        except Exception as e:
            LOGGER.error(traceback.format_exc())
            lineno = "Unknown"
            frame = sys.exc_info()[2]
            while frame is not None:
                f_code = frame.tb_frame.f_code
                if os.path.basename(f_code.co_filename) == \
                        os.path.basename(__file__):
                    lineno = frame.tb_lineno
                frame = frame.tb_next
            LOGGER.error('catching exception: %s, raised at line %s',
                         getattr(e, 'message', repr(e)), str(lineno))

        # Disaster recovery - filling-in unfulfilled stuff
        if "version" not in results:
            results["version"] = ALLOWED_VERSIONS[-1]
        for missing_id in \
                request_ids - \
                set(r["requestId"] for r in
                    results["availableSpectrumInquiryResponses"]):
            response_info = {"responseCode": -1}
            req_info_l = [ri for ri in req_infos.values()
                          if ri.request_id == missing_id]
            if req_info_l:
                response_info["shortDescription"] = \
                    f"AFC general failure. Task ID {req_info_l[0].task_id}"
            results["availableSpectrumInquiryResponses"].append(
                {"requestId": missing_id,
                 "rulesetId": "Unknown",
                 "response": response_info})
            afc_traffic_metrics.request_processed(
                duration_sec=time.time() - flask.g.afc_start_time,
                response="Exception")

        # Removing internal vendor extensions
        drop_unwanted_extensions(
            json_dict=results, is_input=False, is_gui=is_gui,
            is_internal=is_internal_request)

        # Logging response to ALS
        als.als_afc_response(req_id=als_req_id, resp=results)

        # Logginfg failed requests
        for result in results["availableSpectrumInquiryResponses"]:
            if result["response"]["responseCode"] != 0:
                LOGGER.error(
                    f"AFC Failure on request {result['requestId']}: "
                    f"{result['response'].get('shortDescription', '')} "
                    f"{result['response'].get('supplementalInfo', '')}")

        LOGGER.debug("Final results: %s", str(results))
        resp = flask.make_response(flask.json.dumps(results), 200)
        resp.content_type = 'application/json'
        flask.g.afc_no_exception = True
        return resp

    def _cache_lookup(self, dataif, req_infos):
        """ Looks up cached results

        Arguments:
        dataif    -- Objstore handle (used only if RCache not used)
        req_infos -- Dictionary of ReqInfo objects, indexed by request/config
                     hashes
        Returns dictionary of RcacheLookupResult, indexed by request/config
        hashes
        """
        cached_keys = \
            [req_cfg_hash for req_cfg_hash in req_infos.keys()
             if not (req_infos[req_cfg_hash].runtime_opts &
                     (RNTM_OPT_GUI | RNTM_OPT_NOCACHE))]
        if not cached_keys:
            return {}
        if rcache is None:
            ret = {}
            for req_cfg_hash in cached_keys:
                try:
                    with dataif.open(os.path.join(
                            "/responses", req_cfg_hash,
                            "analysisResponse.json.gz")) as hfile:
                        resp_data = hfile.read()
                except BaseException:
                    continue
                ret[req_cfg_hash] = \
                    RcacheLookupResult(
                        found=True,
                        response=zlib.decompress(
                            resp_data, 16 + zlib.MAX_WBITS))
            return ret
        return rcache.lookup_responses(cached_keys)

    def _start_processing(self, dataif, req_info, is_internal_request,
                          rcache_queue=None, use_tasks=False,
                          original_request=None):
        """ Initiates processing on remote worker
        If 'use_tasks' - returns created Task
        """
        request_str = json.dumps(req_info.request, sort_keys=True)
        original_request_str = \
            json.dumps(original_request, sort_keys=True) if original_request \
            else request_str
        if use_tasks:
            with dataif.open(req_info.config_path) as hfile:
                if not hfile.head():
                    hfile.write(req_info.config_str.encode("utf-8"))
            with dataif.open(os.path.join("/responses", req_info.req_cfg_hash,
                                          "analysisRequest.json")) as hfile:
                hfile.write(request_str)

        if req_info.runtime_opts & RNTM_OPT_DBG:
            for fname, content in [("analysisRequest.json", request_str),
                                   ("afc_config.json", req_info.config_str)]:
                with dataif.open(os.path.join(req_info.history_dir, fname)) \
                        as hfile:
                    hfile.write(content.encode("utf-8"))
        build_task(dataif=dataif, request_type=req_info.request_type,
                   task_id=req_info.task_id, hash_val=req_info.req_cfg_hash,
                   config_path=req_info.config_path if use_tasks else None,
                   history_dir=req_info.history_dir,
                   runtime_opts=req_info.runtime_opts,
                   rcache_queue=rcache_queue,
                   request_str=None if use_tasks else request_str,
                   original_request_str=None if use_tasks
                   else original_request_str,
                   config_str=None if use_tasks else req_info.config_str,
                   timeout_sec=flask.current_app.config[
                        'AFC_MSGHND_RATAFC_TOUT'])
        if use_tasks:
            return afctask.Task(task_id=req_info.task_id, dataif=dataif,
                                hash_val=req_info.req_cfg_hash,
                                history_dir=req_info.history_dir,
                                is_internal_request=is_internal_request)
        return None

    def _compute_responses(self, dataif, use_tasks, req_infos,
                           invalidated_responses, is_internal_request):
        """ Prepares worker tasks and waits their completion

        Arguments:
        dataif                -- Objstore handle (used only if RCache not used)
        use_tasks             -- True to use task-based communication with
                                 worker, False to use RabbitMQ-based
                                 communication
        req_infos             -- Dictionary of ReqInfo objects, indexed by
                                 request/config hashes
        invalidated_responses -- Dictionary of invalidated responses for some
                                 requests, indexed by request/config hashes
        is_internal_request   -- True for internal request massage, False for
                                 external request message
        Returns dictionary of found responses (as strings), indexed by
        request/config hashes
        """
        with (contextlib.nullcontext()
              if use_tasks else rcache.rmq_create_rx_connection()) as rmq_conn:
            # Copy AFC Engine state vendor extensions from invalidated
            # responses to requests
            original_requests = {}
            if invalidated_responses:
                for req_cfg_hash, req_info in req_infos.items():
                    invalidated_response_s = \
                        invalidated_responses.get(req_cfg_hash)
                    if not invalidated_response_s:
                        continue    # No such invalidated response
                    invalidated_response = json.loads(
                        invalidated_response_s)
                    vendor_extensions = invalidated_response.get(
                        "availableSpectrumInquiryResponses")[0].\
                        get("vendorExtensions")
                    if not vendor_extensions:
                        continue    # No vendor extensions
                    for ve in vendor_extensions:
                        if ve.get("extensionId") in \
                                rcache.afc_state_vendor_extensions():
                            if req_cfg_hash not in original_requests:
                                original_requests[req_cfg_hash] = \
                                    copy.deepcopy(req_info.request)
                            req_info.request[
                                "availableSpectrumInquiryRequests"][0].\
                                setdefault("vendorExtensions", []).append(ve)
            tasks = {}
            for req_cfg_hash, req_info in req_infos.items():
                tasks[req_cfg_hash] = \
                    self._start_processing(
                        dataif=dataif, req_info=req_info, use_tasks=use_tasks,
                        is_internal_request=is_internal_request,
                        rcache_queue=None if use_tasks
                        else rmq_conn.rx_queue_name(),
                        original_request=original_requests.get(req_cfg_hash))
            if use_tasks:
                ret = {}
                for req_cfg_hash, task in tasks.items():
                    task_stat = \
                        task.wait(
                            timeout=flask.current_app.config[
                                'AFC_MSGHND_RATAFC_TOUT'])
                    ret[req_cfg_hash] = \
                        response_map[task_stat['status']](task).data
                return ret
            ret = \
                rcache.rmq_receive_responses(
                    rx_connection=rmq_conn,
                    req_cfg_digests=req_infos.keys(),
                    timeout_sec=flask.current_app.config[
                        'AFC_MSGHND_RATAFC_TOUT'])
            for req_cfg_hash, response in ret.items():
                req_info = req_infos[req_cfg_hash]
                if (not response) or (not req_info.history_dir):
                    continue
                with dataif.open(os.path.join(req_info.history_dir,
                                              "analysisResponse.json.gz")) \
                        as hfile:
                    hfile.write(zlib.compress(response.encode("utf-8")))
            return ret


class RatAfcSec(RatAfc):
    def get(self):
        LOGGER.debug("(%d) %s::%s()", threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3])
        user_id = auth(roles=['Analysis', 'Trial', 'Admin'])
        return super().get()

    def post(self):
        LOGGER.debug("(%d) %s::%s()", threading.get_native_id(),
                     self.__class__, inspect.stack()[0][3])
        user_id = auth(roles=['Analysis', 'Trial', 'Admin'])
        return super().post()


class RatAfcInternal(MethodView):
    """ Add rule for availableSpectrumInquiryInternal """
    pass


class HealthCheck(MethodView):

    def get(self):
        '''GET method for HealthCheck'''
        msg = 'The ' + flask.current_app.config['AFC_APP_TYPE'] + ' is healthy'

        return flask.make_response(msg, 200)


class ReadinessCheck(MethodView):

    def get(self):
        '''GET method for Readiness Check'''
        msg = 'The ' + flask.current_app.config['AFC_APP_TYPE'] + ' is '

        hconn = RmqHealthcheck(flask.current_app.config['BROKER_URL'])
        if hconn.healthcheck():
            msg += 'not ready'
            return flask.make_response(msg, 503)

        msg += 'ready'
        return flask.make_response(msg, 200)


# Message-level AFC metrics
@module.before_request
def traffic_metrics_bedore():
    flask.g.afc_start_time = time.time()
    flask.g.is_afc = False
    flask.g.afc_no_exception = False


@module.after_request
def traffic_metrics_after(response):
    if flask.g.is_afc:
        afc_traffic_metrics.message_processed(
            duration_sec=time.time() - flask.g.afc_start_time,
            status=response.status_code if flask.g.afc_no_exception
            else "Exception")
    return response


# registration of default runtime options

module.add_url_rule('/availableSpectrumInquirySec',
                    view_func=RatAfcSec.as_view('RatAfcSec'))

module.add_url_rule('/availableSpectrumInquiry',
                    view_func=RatAfc.as_view('RatAfc'))

module.add_url_rule('/availableSpectrumInquiryInternal',
                    view_func=RatAfc.as_view('RatAfcInternal'))

module.add_url_rule('/healthy', view_func=HealthCheck.as_view('HealthCheck'))

module.add_url_rule(
    '/ready', view_func=ReadinessCheck.as_view('ReadinessCheck'))

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
