""" AFC Server Pydantic data models """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-few-public-methods, wrong-import-order, invalid-name

import enum
import pydantic
from pydantic import ConfigDict
from pydantic_settings import BaseSettings
from typing import Any, ClassVar, Dict, List, NamedTuple, Optional

__all__ = ["AfcServerSettings", "OpenAfcUsedDataVendorExtParams",
           "Rest_AvailableSpectrumInquiryRequest",
           "Rest_AvailableSpectrumInquiryRequest_1_4",
           "Rest_AvailableSpectrumInquiryResponseMinGen",
           "Rest_AvailableSpectrumInquiryResponseMinParse",
           "Rest_CertificationId_1_4", "Rest_DeviceDescriptor_1_4",
           "Rest_ReqMsg", "Rest_ReqMsg_1_4", "Rest_RespMsg_1_4",
           "Rest_Response", "Rest_ResponseCode", "Rest_VendorExtension",
           "Rest_SupplementalInfo", "Rest_SupportedVersions"]


class AfcServerSettings(BaseSettings):
    """ AFC server service parameters, passed via environment variables """

    model_config = ConfigDict(env_prefix="AFC_SERVER_")

    port: int = pydantic.Field(default=...,
                               title="Port AFC server listens on")
    rcache_dsn: pydantic.PostgresDsn = \
        pydantic.Field(
            default=...,
            title="Rcache Postgres DSN: "
            "postgresql://[user[:password]]@host[:port]/database[?...]",
            validation_alias="RCACHE_POSTGRES_DSN")
    rcache_password_file: Optional[str] = \
        pydantic.Field(default=None, title="File with password for Rcache DSN",
                       validation_alias="RCACHE_POSTGRES_PASSWORD_FILE")
    ratdb_dsn: pydantic.PostgresDsn = \
        pydantic.Field(
            default=...,
            title="RatDb Postgres DSN: "
            "postgresql://[user[:password]]@host[:port]/database[?...]")
    ratdb_password_file: Optional[str] = \
        pydantic.Field(default=None, title="File with password for RatDb DSN")
    rmq_dsn: pydantic.AmqpDsn = \
        pydantic.Field(
            default=...,
            title="RabbitMQ AMQP DSN for receiving replies from worker: "
            "amqp://[user]@host[:port])",
            validation_alias="RCACHE_RMQ_DSN")
    rmq_password_file: Optional[str] = \
        pydantic.Field(default=None,
                       title="File with password for RabbitMQ AMQP DSN",
                       validation_alias="RCACHE_RMQ_PASSWORD_FILE")
    static_data_root: Optional[str] = \
        pydantic.Field(default=None,
                       title="Worker's mount path of static files",
                       validation_alias="NFS_MOUNT_PATH")
    request_timeout: float = \
        pydantic.Field(
            default=...,
            title="Maximum AFC Request processing duration in seconds")
    request_timeout_edebug: float = \
        pydantic.Field(
            default=...,
            title="Maximum EDEBUG AFC Request processing duration in seconds")
    config_refresh: float = \
        pydantic.Field(default=...,
                       title="AFC Config refresh interval in seconds")
    log_level: Optional[str] = pydantic.Field(default=None,
                                              title="Log level name")
    engine_request_type: str = pydantic.Field(default="AP-AFC",
                                              title="AFC Engine Request Type")
    afc_state_vendor_extensions: Optional[List[str]] = \
        pydantic.Field(
            default=None,
            title="Comma-separated list of response vendor extensions from "
            "rcache to attach to requests sent to AFC Engine",
            validation_alias="AFC_STATE_VENDOR_EXTENSIONS")
    bypass_cert: bool = \
        pydantic.Field(
            default=False,
            title="Bypass certification lookup (always respond "
            "affirmatively). For performance estimation purposes")
    bypass_rcache: bool = \
        pydantic.Field(
            default=False,
            title="Bypass actual Rcache lookup (always return same record). "
            "For performance estimation purposes")

    @pydantic.model_validator(mode="before")
    @classmethod
    def _coerce_list_fields(cls, values: Dict[str, Any]) -> Dict[str, Any]:
        """Coerce comma-separated env var strings to lists for list-typed
        fields"""
        for key in ("afc_state_vendor_extensions",
                    "AFC_STATE_VENDOR_EXTENSIONS"):
            val = values.get(key)
            if isinstance(val, str):
                values[key] = [s.strip() for s in val.split(",") if s.strip()]
        return values


# Supported request versions. Last is default response version
Rest_SupportedVersions = ["1.4"]


class Rest_VendorExtension(pydantic.BaseModel, extra="forbid"):
    """ Vendor Extension used in AFC requests and responses """
    extensionId: str = pydantic.Field(min_length=1)
    parameters: Any


class Rest_AvailableSpectrumInquiryRequest(pydantic.BaseModel,
                                           extra="allow"):
    """ Minimally acceptable request structure """
    requestId: str


class Rest_ReqMsg(pydantic.BaseModel, extra="allow"):
    """ Minimally acceptable message structure """
    version: str
    availableSpectrumInquiryRequests: \
        List[Rest_AvailableSpectrumInquiryRequest]


class Rest_ReqMsg_1_4(pydantic.BaseModel, extra="allow"):
    """ AFC 1.4 request message with minimally acceptable requests """
    version: str
    availableSpectrumInquiryRequests: \
        List[Rest_AvailableSpectrumInquiryRequest]
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None


class Rest_CertificationId_1_4(pydantic.BaseModel,
                               extra="forbid"):
    """ Certification definition used in AFC 1.4 requests """
    rulesetId: str = pydantic.Field(min_length=1)
    id: str = pydantic.Field(min_length=1)


class Rest_DeviceDescriptor_1_4(pydantic.BaseModel,
                                extra="forbid"):
    """ Device definition used in AFC 1.4 requests """
    serialNumber: str = pydantic.Field(min_length=1)
    certificationId: List[Rest_CertificationId_1_4] = \
        pydantic.Field(min_length=1)


class Rest_AvailableSpectrumInquiryRequest_1_4(pydantic.BaseModel,
                                               extra="allow"):
    """ Minimally acceptable 1.4 request structure """
    requestId: str = pydantic.Field(min_length=1)
    deviceDescriptor: Rest_DeviceDescriptor_1_4
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None


ResponseCodeInfo = \
    NamedTuple("ResponseCodeInfo",
               [
                # Response code for Rest_Response
                ("code", int),
                # ratafc.py - compatible shortDescription prefix
                ("prefix", Optional[str])])


class Rest_ResponseCode(enum.Enum):
    """ Response codes """
    GENERAL_FAILURE = ResponseCodeInfo(code=-1, prefix="")
    SUCCESS = ResponseCodeInfo(code=0, prefix=None)
    VERSION_NOT_SUPPORTED = \
        ResponseCodeInfo(
            code=100,
            prefix=f"The requested version number is invalid. Supported "
            f"versions are: {', '.join(Rest_SupportedVersions)}")
    DEVICE_DISALLOWED = \
        ResponseCodeInfo(
            code=101,
            prefix="This specific device is not allowed to operate under AFC "
            "control. ")
    MISSING_PARAM = \
        ResponseCodeInfo(
            code=102,
            prefix="One or more fields required to be included in the request "
            "are missing.")
    INVALID_VALUE = \
        ResponseCodeInfo(
            code=103,
            prefix="One or more fields have an invalid value.")
    UNEXPECTED_PARAM = \
        ResponseCodeInfo(
            code=106,
            prefix="Unknown parameter found, or conditional parameter found, "
            "but condition is not met.")
    UNSUPPORTED_SPECTRUM = \
        ResponseCodeInfo(
            code=300,
            prefix="The frequency range indicated in the Available Spectrum "
            "Inquiry Request is at least partially outside of the frequency "
            "band under the management of the AFC.")
    UNSUPPORTED_BASIS = ResponseCodeInfo(code=301, prefix="")


class Rest_SupplementalInfo(pydantic.BaseModel, extra="forbid"):
    """ Error supplemental info used in AFC responses """
    missingParams: Optional[List[str]] = None
    invalidParams: Optional[List[str]] = None
    unexpectedParams: Optional[List[str]] = None


class Rest_Response(pydantic.BaseModel, extra="forbid"):
    """ AFC Response (computation result) """
    responseCode: int
    shortDescription: Optional[str] = None
    supplementalInfo: Optional[Rest_SupplementalInfo] = None


class Rest_AvailableSpectrumInquiryResponseMinGen(pydantic.BaseModel,
                                                  extra="allow"):
    """ Minimal generated AFC Response structure """
    requestId: str
    rulesetId: str
    response: Rest_Response
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None


class Rest_AvailableSpectrumInquiryResponseMinParse(
        pydantic.BaseModel, extra="allow"):
    """ Minimal parsed AFC Response structure """
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None


class Rest_RespMsg_1_4(pydantic.BaseModel, extra="allow"):
    """ Minimum parsed AFC 1.4 response message"""
    availableSpectrumInquiryResponses: \
        List[Rest_AvailableSpectrumInquiryResponseMinParse]


class OpenAfcUsedDataVendorExtParams(pydantic.BaseModel,
                                     extra="ignore"):
    """ Vendor extension with information on what data was used by Engine """
    EXT_ID: ClassVar[str] = "openAfc.used_data"
    uls_id: Optional[str] = None
    geo_id: Optional[str] = None
