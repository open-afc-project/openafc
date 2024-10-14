""" AFC Message processor """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=wrong-import-order, too-many-arguments
# pylint: disable=too-few-public-methods, too-many-branches, too-many-locals
# pylint: disable=broad-exception-caught, too-many-statements
# pylint: disable=consider-using-from-import

import asyncio
import datetime
import json
import os
import pydantic
import sys
import time
import traceback
from typing import Any, Dict, List, NamedTuple, Optional, Union
import uuid

import afcmodels.hardcoded_relations as hardcoded_relations
import afc_server_compute
import afc_server_db
from afc_server_models import OpenAfcUsedDataVendorExtParams, \
    Rest_AvailableSpectrumInquiryRequest_1_4, \
    Rest_AvailableSpectrumInquiryResponseMinGen, Rest_ReqMsg, \
    Rest_ReqMsg_1_4, Rest_RespMsg_1_4, Rest_Response, Rest_ResponseCode, \
    Rest_SupplementalInfo, Rest_SupportedVersions
import als
import defs
from log_utils import dp, get_module_logger
import rcache_req_cfg_hash

# Logger for this module
LOGGER = get_module_logger()

__all__ = ["AfcServerMessageProcessor"]


class AfcServerMessageProcessor:
    """ Processor of AFC Request messages

    Private attributes:
    _db                         -- DB Accessor
    _compute                    -- AFC Engine computer
    _request_timeout_sec        -- Timeout for normal request computation in
                                   seconds
    _edebug_request_timeout_sec -- Timeout for EDEBUG request computation in
                                   seconds
    _config_dispenser           -- AfcConfigDispenser object
    """

    class AfcConfigDispenser:
        """ Holds and dispenses per-ruleset AFC Configs, reacquiring them
        from DB sometimes

        Private attributes:
        _db                 -- DB Accessor
        _config_refresh_sec -- AFC Config refresh interval in seconds
        _cfg_infos         -- Per-ruleset dictionary of AFC Configs with
                              retention deadlines
        """
        # Config and its retention deadline
        CfgInfo = \
            NamedTuple("CfgInfo",
                       [
                        # Optional AFC Config as dictionary
                        ("config", Optional[Dict[str, Any]]),
                        # Retention deadline in seconds since the Epoch
                        ("retention", float)])

        def __init__(self, db: afc_server_db.AfcServerDb,
                     config_refresh_sec: float) -> None:
            """ Constructor

            Arguments:
            db                 -- DB Accessor
            config_refresh_sec -- AFC Config refresh interval in seconds
            """
            self._db = db
            self._config_refresh_sec = config_refresh_sec
            self._cfg_infos: \
                Dict[str,
                     "AfcServerMessageProcessor.AfcConfigDispenser.CfgInfo"] \
                = {}

        async def get_config(self, ruleset_name, deadline: float) \
                -> Optional[Optional[Dict[str, Any]]]:
            """ Get config fir given Ruleset

            Arguments:
            ruleset_name -- Ruleset name (aka Ruleset ID)
            deadline     -- Request message completion deadline in seconds
                            since the Epoch
            Returns AFC Config as dictionary, None if not found. May raise
            TimeeoutError exception
            """
            cfg_info = self._cfg_infos.get(ruleset_name)
            if (cfg_info is not None) and (cfg_info.retention > time.time()):
                return cfg_info.config
            config = await self._db.get_afc_config(ruleset_name=ruleset_name,
                                                   deadline=deadline)
            self._cfg_infos[ruleset_name] = \
                AfcServerMessageProcessor.AfcConfigDispenser.CfgInfo(
                    config=config,
                    retention=time.time() + self._config_refresh_sec)
            return config

    def __init__(self, db: afc_server_db.AfcServerDb,
                 compute: afc_server_compute.AfcServerCompute,
                 request_timeout_sec: float,
                 edebug_request_timeout_sec: float,
                 config_refresh_sec: float) -> None:
        """ Constructor

        Arguments:
        db                         -- DB Accessor
        compute                    -- AFC Engine computer
        request_timeout_sec        -- Timeout for normal request computation in
                                      seconds
        edebug_request_timeout_sec -- Timeout for EDEBUG request computation in
                                      seconds
        config_refresh_sec         -- AFC Config refresh interval in seconds
        """
        als.als_initialize(client_id="afc_server")
        self._db = db
        self._compute = compute
        self._request_timeout_sec = request_timeout_sec
        self._edebug_request_timeout_sec = edebug_request_timeout_sec
        self._config_dispenser = \
            AfcServerMessageProcessor.AfcConfigDispenser(
                db=self._db, config_refresh_sec=config_refresh_sec)

    async def close(self) -> None:
        """ Gracefully close """
        await self._db.close()
        await self._compute.close()

    async def process_msg(self, req_msg: Rest_ReqMsg, debug: bool,
                          edebug: bool, nocache: bool, gui: bool,
                          internal: bool) \
            -> Dict[str, Any]:
        """ Process AFC Request message

        Arguments:
        req_msg  -- AFC Request message
        debug    -- True if 'debug' flag was set in URL
        edebug   -- True if 'edebug' flag was set in URL
        nocache  -- True if 'nocache' flag was set in URL
        gui      -- True if 'gui' flag was set in URL
        internal -- True if request message came from within the cluster
        Returns AFC Response message in dictionary form
        """
        req_msg_dict = req_msg.dict(exclude_none=True)
        als_req_id = als.als_afc_req_id()
        als.als_afc_request(req_id=als_req_id, req=req_msg_dict)
        deadline = time.time() + \
            (self._edebug_request_timeout_sec if edebug
             else self._request_timeout_sec)
        # Message format version acceptable?
        if req_msg.version not in Rest_SupportedVersions:
            return \
                self._make_response_msg(
                    response_info=self._make_response_info(
                        response_code=Rest_ResponseCode.VERSION_NOT_SUPPORTED),
                    req_msg_dict=req_msg_dict)
        ret = \
            (await self._process_msg_internal(
                req_msg_dict=req_msg_dict, debug=debug, edebug=edebug,
                nocache=nocache, gui=gui, internal=internal,
                als_req_id=als_req_id, deadline=deadline))
        self._drop_unwanted_extensions(
            msg_dict=ret, is_input=False, is_gui=gui, is_internal=internal)
        als.als_afc_response(req_id=als_req_id, resp=ret)
        return ret

    async def _process_msg_internal(
            self, req_msg_dict: Dict[str, Any], debug: bool, edebug: bool,
            nocache: bool, gui: bool, internal: bool, als_req_id: int,
            deadline: float) -> Dict[str, Any]:
        """ Internal implementation of Request message processing

        Arguments:
        req_msg_dict  -- AFC Request message in dictionary form
        debug         -- True if 'debug' flag was set in URL
        edebug        -- True if 'edebug' flag was set in URL
        nocache       -- True if 'nocache' flag was set in URL
        gui           -- True if 'gui' flag was set in URL
        internal      -- True if request message came from within the cluster
        als_req_id    -- Request id to use in subsequent ALS writes
        deadline      -- Message processing deadline in seconds since the Epoch
        Returns AFC Response message in dictionary form
        """
        # Too late?
        if time.time() >= deadline:
            return \
                self._make_response_msg(
                    response_info=self._make_response_info(ex=TimeoutError()),
                    req_msg_dict=req_msg_dict)
        # Top-level format acceptable? Individual requests checked separately
        try:
            Rest_ReqMsg_1_4.validate(req_msg_dict)
        except pydantic.ValidationError as ex:
            return self._make_response_msg(
                response_info=self._make_response_info(
                    ex=ex, validated_dict=req_msg_dict),
                req_msg_dict=req_msg_dict)
        # Remove unacceptable Vendor Extensions
        self._drop_unwanted_extensions(
            msg_dict=req_msg_dict, is_input=True, is_gui=gui,
            is_internal=internal)
        req_tasks: List[asyncio.Task] = []
        # Process individual requests
        async with asyncio.TaskGroup() as req_tg:
            for req_idx, req_dict in \
                    enumerate(
                        req_msg_dict["availableSpectrumInquiryRequests"]):
                req_tasks.append(
                    req_tg.create_task(
                        self._process_req(
                            req_dict=req_dict, debug=debug, edebug=edebug,
                            nocache=nocache, gui=gui, als_req_id=als_req_id,
                            req_idx=req_idx, deadline=deadline)))
        # Make result form individual responses
        return \
            self._make_response_msg(
                responses=[task.result() for task in req_tasks],
                req_msg_dict=req_msg_dict)

    async def _process_req(self, req_dict: Dict[str, Any], debug: bool,
                           edebug: bool, nocache: bool, gui: bool,
                           als_req_id: int, req_idx: int, deadline: float) \
            -> Dict[str, Any]:
        """ Process individual AFC request

        Arguments:
        req_dict      -- Request to process in dictionary form
        debug         -- True if 'debug' flag was set in URL
        edebug        -- True if 'edebug' flag was set in URL
        nocache       -- True if 'nocache' flag was set in URL
        gui           -- True if 'gui' flag was set in URL
        als_req_id    -- Request id to use in subsequent ALS writes
        req_idx       -- Request index inside AFC Request message
        deadline      -- Message processing deadline in seconds since the Epoch
        Returns AFC Response in dictionary form
        """
        task_id = str(uuid.uuid4())
        # Ruleset ID to use in error responses
        err_ruleset_name = "Unknown"
        # Dictionary that, possibly, caused Pydantic validation error
        validated_dict: Optional[Dict[str, Any]] = None
        try:
            try:
                err_ruleset_name = \
                    req_dict["deviceDescriptor"]["certificationId"][0][
                        "rulesetId"]
            except KeyError:
                pass
            # Validate request format
            validated_dict = req_dict
            req = Rest_AvailableSpectrumInquiryRequest_1_4.validate(req_dict)
            validated_dict = None

            # Find allowed certifications
            cert_info = \
                await self._db.get_cert_info(
                    afc_server_db.AfcCertReq(req.deviceDescriptor),
                    deadline=deadline)
            allowed_certifications = cert_info.allowed_cert_resps()
            if not allowed_certifications:
                # Bail if none allowed
                return \
                    self._make_failed_response(
                        request_id=req.requestId, ruleset_id=err_ruleset_name,
                        response_info=self._make_response_info(
                            response_code=Rest_ResponseCode.DEVICE_DISALLOWED,
                            description=cert_info.deny_reason()))
            err_ruleset_name = allowed_certifications[0].ruleset_name

            # Find AFC Config for some allowed certification
            afc_config_dict: Optional[Dict[str, Any]]
            cert_resp: Optional[afc_server_db.AfcCertResp.CertResp] = None
            for cr in allowed_certifications:
                afc_config_dict = \
                    await self._config_dispenser.get_config(
                        ruleset_name=cr.ruleset_name, deadline=deadline)
                if afc_config_dict is not None:
                    cert_resp = cr
                    err_ruleset_name = cr.ruleset_name
                    break
            else:
                # Bail if no AFC Configs found
                return \
                    self._make_failed_response(
                        request_id=req.requestId, ruleset_id=err_ruleset_name,
                        response_info=self._make_response_info(
                            response_code=Rest_ResponseCode.DEVICE_DISALLOWED,
                            description="No AFC Config found for presented "
                            "Ruleset IDs"))
            assert afc_config_dict is not None
            assert cert_resp is not None

            # Replace region string in AFC Config if necessary
            try:
                overwrite_region = \
                    hardcoded_relations.RulesetVsRegion.overwrite_region(
                        afc_config_dict["regionStr"], exc=KeyError)
                if overwrite_region:
                    afc_config_dict = dict(afc_config_dict)
                    afc_config_dict["regionStr"] = overwrite_region
            except KeyError:
                pass

            # Compute request/config hash (key in Rcache)
            rcc = \
                rcache_req_cfg_hash.RequestConfigHash(
                    req_dict=req_dict, afc_config_dict=afc_config_dict)

            # Do the rcache lookup
            ret: Optional[Dict[str, Any]] = None
            resp_str: Optional[str]
            if not (nocache or debug or edebug or gui):
                resp_str = \
                    await self._db.lookup_rcache(
                        req_cfg_digest=rcc.req_cfg_hash, deadline=deadline)
                if resp_str is not None:
                    try:
                        ret = \
                            Rest_RespMsg_1_4.parse_raw(resp_str).\
                            availableSpectrumInquiryResponses[0].\
                            dict(exclude_none=True)
                    except pydantic.ValidationError:
                        ret = None

            # If no results from cache - invoke AFC Engine
            if ret is None:
                runtime_opts = 0
                for predicate, flag in \
                        [(cert_resp.location_flags &
                          hardcoded_relations.CERT_ID_LOCATION_INDOOR,
                          defs.RNTM_OPT_CERT_ID),
                         (debug, defs.RNTM_OPT_DBG),
                         (edebug, defs.RNTM_OPT_SLOW_DBG),
                         (gui, defs.RNTM_OPT_GUI)]:
                    if predicate:
                        runtime_opts |= flag
                resp_str = \
                    await self._compute.process_request(
                        request_str=json.dumps(
                            {"version": Rest_SupportedVersions[-1],
                             "availableSpectrumInquiryRequests": [req_dict]}),
                        config_str=rcc.cfg_str,
                        req_cfg_digest=rcc.req_cfg_hash,
                        runtime_opts=runtime_opts, task_id=task_id,
                        history_dir=os.path.join(
                            "/history", req.deviceDescriptor.serialNumber,
                            datetime.datetime.now().isoformat()),
                        deadline=deadline)
                if not resp_str:
                    # Bail on failure
                    return \
                        self._make_failed_response(
                            request_id=req.requestId,
                            ruleset_id=err_ruleset_name,
                            response_info=self._make_response_info(
                                response_code=Rest_ResponseCode.
                                GENERAL_FAILURE,
                                description=f"AFC General failure. "
                                f"Task ID {task_id}"))
                ret = \
                    Rest_RespMsg_1_4.parse_raw(resp_str).\
                    availableSpectrumInquiryResponses[0].\
                    dict(exclude_none=True)

            # Extract processing information from response...
            uls_id = "Unknown"
            geo_id = "Unknown"
            for ve_dict in ret.get("vendorExtensions", []):
                if ve_dict["extensionId"] == \
                        OpenAfcUsedDataVendorExtParams.EXT_ID:
                    parameters_dict = ve_dict.get("parameters", {})
                    uls_id = parameters_dict.get("uls_id", uls_id)
                    geo_id = parameters_dict.get("geo_id", geo_id)
                    break
            # ... and log it
            als.als_afc_config(
                req_id=als_req_id, config_text=rcc.cfg_str,
                customer=afc_config_dict["regionStr"],
                geo_data_version=geo_id, uls_id=uls_id,
                req_indices=[req_idx])

            # Return the result
            ret["requestId"] = req.requestId
            return ret
        except Exception as ex:
            return \
                self._make_failed_response(
                    request_id=req_dict["requestId"],
                    ruleset_id=err_ruleset_name,
                    response_info=self._make_response_info(
                        ex=ex, validated_dict=validated_dict, task_id=task_id))

    def _make_response_msg(
            self, req_msg_dict: Dict[str, Any],
            response_info: Optional[Rest_Response] = None,
            responses:  Optional[List[Dict[str, Any]]] = None) \
            -> Dict[str, Any]:
        """ Make AFC Response message from individual responses

        Arguments:
        req_msg_dict  -- Original request message in dictionary form
        response_info -- One Rest_Response in dictionary form to use for all
                         requests (optional)
        responses     -- List of individual responses in dictionary form
                         (optional)
        Returns AFC Response message in dictionary form
        """
        resp_version = req_msg_dict["version"]
        if response_info:
            assert responses is None
            if resp_version not in Rest_SupportedVersions:
                resp_version = Rest_SupportedVersions[-1]
            responses = \
                [self._make_failed_response(
                    request_id=req_dict["requestId"], ruleset_id="Unknown",
                    response_info=response_info)
                 for req_dict in
                 req_msg_dict["availableSpectrumInquiryRequests"]]
        assert responses is not None
        return {"version": resp_version,
                "availableSpectrumInquiryResponses": responses}

    def _make_failed_response(self, request_id: str, ruleset_id: str,
                              response_info: Rest_Response) -> Dict[str, Any]:
        """ Returns singular failed AFC response object in dictionary form """
        return \
            Rest_AvailableSpectrumInquiryResponseMinGen(
                requestId=request_id, rulesetId=ruleset_id,
                response=response_info).dict(exclude_none=True)

    def _make_response_info(
            self, ex: Optional[BaseException] = None,
            validated_dict: Optional[Dict[str, Any]] = None,
            task_id: Optional[str] = None,
            response_code: Optional[Rest_ResponseCode] = None,
            description: Optional[str] = None) -> Rest_Response:
        """ Makes Response object containing some bad news

        Arguments:
        ex             -- Exception occurred during request processing or None
        validated_dict -- Dictionary, containing object whose validatin caused
                          pydantic exception
        task_id        -- Request task id or None
        responses_code -- Response code or None
        description    -- Error message or None
        Returns Response object
        """

        def construct_response(
                code: Rest_ResponseCode,
                dsc: str = "",
                supplemental_info: Optional[Rest_SupplementalInfo] = None) \
                -> Rest_Response:
            """ Constructs Rest_Response from given parts, applying
            ratafc.py-compatible prefixes to short descriptions """
            short_description: Optional[str] = \
                None if code.value.prefix is None \
                else (code.value.prefix + dsc)
            assert short_description or (short_description is None)
            return \
                Rest_Response(
                    responseCode=code.value.code,
                    shortDescription=short_description,
                    supplementalInfo=supplemental_info)

        if response_code is not None:
            return construct_response(code=response_code,
                                      dsc=description or "")
        assert ex is not None
        if isinstance(ex, pydantic.ValidationError):
            # Missing parameters?
            missing_params = \
                sum([[str(err["loc"][-1])] for err in ex.errors()
                     if "missing" in err["type"]],
                    [])
            if missing_params:
                return \
                    construct_response(
                        code=Rest_ResponseCode.MISSING_PARAM,
                        supplemental_info=Rest_SupplementalInfo(
                            missingParams=missing_params))
            # Unexpected parameters?
            unexpected_params = \
                sum([[str(err["loc"][-1])] for err in ex.errors()
                     if "extra" in err["type"]],
                    [])
            if unexpected_params:
                return \
                    construct_response(
                        code=Rest_ResponseCode.UNEXPECTED_PARAM,
                        supplemental_info=Rest_SupplementalInfo(
                            unexpectedParams=unexpected_params))
            # Assuming invalid parameters
            invalid_params: List[str] = []
            for err in ex.errors():
                invalid_params.append(str(err["loc"][-1]))
                if validated_dict is None:
                    invalid_params.append("?")
                else:
                    try:
                        v: Any = validated_dict
                        for attr in err["loc"]:
                            v = v[attr]
                        invalid_params.append(str(v))
                    except KeyError:
                        invalid_params.append("?")
            if invalid_params:
                return \
                    construct_response(
                        code=Rest_ResponseCode.INVALID_VALUE,
                        supplemental_info=Rest_SupplementalInfo(
                            invalidParams=invalid_params))
        if isinstance(ex, (TimeoutError, asyncio.TimeoutError)):
            return \
                construct_response(
                    code=Rest_ResponseCode.GENERAL_FAILURE,
                    dsc="Request processing timed out")
        lineno: Union[int, str] = "Unknown"
        frame = sys.exc_info()[2]
        while frame is not None:
            f_code = frame.tb_frame.f_code
            if os.path.basename(f_code.co_filename) == \
                    os.path.basename(__file__):
                lineno = frame.tb_lineno
            frame = frame.tb_next
        LOGGER.error(f"{traceback.format_exc()}\n"
                     f"Catching exception: "
                     f"{getattr(ex, 'message', repr(ex))}, "
                     f"raised at line {lineno} of "
                     f"{os.path.basename(__file__)}")
        return \
            construct_response(
                code=Rest_ResponseCode.GENERAL_FAILURE,
                dsc=f"AFC General failure"
                f"{('. Task ID ' + task_id) if task_id else ''}")

    def _drop_unwanted_extensions(
            self, msg_dict: Dict[str, Any], is_input: bool, is_gui: bool,
            is_internal: bool) -> None:
        """ Inplace removal of unallowed vendor extensions from given message

        Arguments:
        msg_dict    -- AFC Request/Response message in dictionary form
        is_input    -- True for request, False for response
        is_gui      -- True if from GUI
        is_internal -- True if internal (from within AFC Cluster)
        """
        for container, is_message in [(msg_dict, True)] + \
                [(req_resp, False) for req_resp in
                 msg_dict.get("availableSpectrumInquiryRequests" if is_input
                              else "availableSpectrumInquiryResponses", [])]:
            extensions = container.get("vendorExtensions")
            if extensions is None:
                continue
            idx = 0
            while idx < len(extensions):
                if hardcoded_relations.VendorExtensionFilter.allowed_extension(
                        extension=extensions[idx]["extensionId"],
                        is_message=is_message, is_input=is_input,
                        is_gui=is_gui, is_internal=is_internal):
                    idx += 1
                else:
                    extensions.pop(idx)
            if not extensions:
                del container["vendorExtensions"]
