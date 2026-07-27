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
           "Rest_LinearPolygon_1_4", "Rest_Location_1_4",
           "Rest_RadialPolygon_1_4",
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
    enforce_mtls: bool = \
        pydantic.Field(
            default=True,
            title="Reject external requests that arrive without an mTLS "
            "client DN (i.e. when AFC_ENFORCE_MTLS=true is set in nginx). "
            "Enforces mTLS client certificate presence on the external endpoint. "
            "Secure by default; set AFC_ENFORCE_MTLS=false to opt out in "
            "lab/test deployments.",
            validation_alias="AFC_ENFORCE_MTLS")

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
        List[Rest_AvailableSpectrumInquiryRequest] = \
        pydantic.Field(min_length=1, max_length=16)


class Rest_ReqMsg_1_4(pydantic.BaseModel, extra="allow"):
    """ AFC 1.4 request message with minimally acceptable requests """
    version: str
    availableSpectrumInquiryRequests: \
        List[Rest_AvailableSpectrumInquiryRequest] = \
        pydantic.Field(min_length=1, max_length=16)
    vendorExtensions: Optional[List[Rest_VendorExtension]] = \
        pydantic.Field(default=None, max_length=32)


class Rest_CertificationId_1_4(pydantic.BaseModel,
                               extra="forbid"):
    """ Certification definition used in AFC 1.4 requests """
    # Excludes '|': rcache's ApDbPk.from_req joins rulesetId/id across the
    # whole certificationId list with "|" to build the cache primary key
    # (rcache_models.py); an embedded '|' would let a crafted list collide
    # with a different device's decomposition of the same joined string
    # (CWE-180). serialNumber below already restricts to a safe charset for
    # the same class of reason.
    rulesetId: str = pydantic.Field(min_length=1, max_length=64,
                                    pattern=r'^[^|]+$')
    id: str = pydantic.Field(min_length=1, max_length=64,
                             pattern=r'^[^|]+$')


class Rest_DeviceDescriptor_1_4(pydantic.BaseModel,
                                extra="forbid"):
    """ Device definition used in AFC 1.4 requests """
    serialNumber: str = pydantic.Field(min_length=1, pattern=r'^[A-Za-z0-9._-]+$')
    certificationId: List[Rest_CertificationId_1_4] = \
        pydantic.Field(min_length=1, max_length=16)


class Rest_LinearPolygon_1_4(pydantic.BaseModel, extra="allow"):
    """ LinearPolygon location shape — vertex count capped to bound engine allocation """
    outerBoundary: List[Any] = pydantic.Field(min_length=3, max_length=1000)


class Rest_RadialPolygon_1_4(pydantic.BaseModel, extra="allow"):
    """ RadialPolygon location shape — vertex count capped to bound engine allocation """
    outerBoundary: List[Any] = pydantic.Field(min_length=1, max_length=1000)


class _Rest_EllipseCenter(pydantic.BaseModel, extra="allow"):
    """ Ellipse center point with typed lat/lon fields for proper float coercion """
    latitude: float
    longitude: float


class _Rest_Ellipse_1_4(pydantic.BaseModel, extra="allow"):
    """ Ellipse location shape — center coordinates declared as float """
    center: Optional[_Rest_EllipseCenter] = None


class Rest_Location_1_4(pydantic.BaseModel, extra="allow"):
    """ Device location — declares polygon shapes to enforce vertex-count limits """
    linearPolygon: Optional[Rest_LinearPolygon_1_4] = None
    radialPolygon: Optional[Rest_RadialPolygon_1_4] = None
    ellipse: Optional[_Rest_Ellipse_1_4] = None


class _Rest_FrequencyRange_1_4(pydantic.BaseModel, extra="allow"):
    """One inquired frequency range entry."""
    lowFrequency: Optional[float] = None
    highFrequency: Optional[float] = None


class _Rest_InquiredChannel_1_4(pydantic.BaseModel, extra="allow"):
    """One inquired channel entry — caps the inner channelCfi list."""
    globalOperatingClass: Optional[int] = None
    # 6 GHz has at most ~240 20-MHz channels; 512 is generous but bounded.
    channelCfi: Optional[List[int]] = \
        pydantic.Field(default=None, max_length=512)


class Rest_AvailableSpectrumInquiryRequest_1_4(pydantic.BaseModel,
                                               extra="allow"):
    """ Minimally acceptable 1.4 request structure.

    Array-length bounds mirror the limits enforced by AfcManager.cpp so
    oversized requests are rejected at the API boundary before dispatch.
    """
    requestId: str = pydantic.Field(min_length=1)
    deviceDescriptor: Rest_DeviceDescriptor_1_4
    location: Optional[Rest_Location_1_4] = None
    vendorExtensions: Optional[List[Rest_VendorExtension]] = \
        pydantic.Field(default=None, max_length=32)
    # Max 256 — matches maxFreqRanges / maxInquiredChannels in AfcManager.cpp
    inquiredFrequencyRange: Optional[List[_Rest_FrequencyRange_1_4]] = \
        pydantic.Field(default=None, max_length=256)
    inquiredChannels: Optional[List[_Rest_InquiredChannel_1_4]] = \
        pydantic.Field(default=None, max_length=256)


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
