""" AFC Server Pydantic data models """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-few-public-methods, wrong-import-order, invalid-name

import enum
import math
import pydantic
from pydantic import ConfigDict
from pydantic_settings import BaseSettings
from typing import Any, ClassVar, Dict, List, Literal, NamedTuple, Optional

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

    @pydantic.field_validator("parameters")
    @classmethod
    def _reject_non_finite_numbers(cls, value: Any) -> Any:
        """ Reject NaN/Infinity anywhere inside parameters.

        Python's json parser accepts the non-RFC-8259 tokens
        NaN/Infinity/-Infinity and json.dumps re-emits them, but
        PostgreSQL's json input rejects them: a non-finite float that
        reaches the ALS rx_envelope JSON column makes the siphon demote
        the whole message bundle to a decode_error row (structured
        audit loss). Reject at the API boundary, BEFORE a grant is
        computed (mirrors _Rest_GeoPoint_1_4 / duplicate-requestId).
        """
        stack = [value]
        while stack:
            item = stack.pop()
            if isinstance(item, float) and not math.isfinite(item):
                raise ValueError(
                    "non-finite numbers (NaN/Infinity) not allowed in "
                    "vendor extension parameters")
            if isinstance(item, dict):
                stack.extend(item.values())
            elif isinstance(item, (list, tuple)):
                stack.extend(item)
        return value


class Rest_AvailableSpectrumInquiryRequest(pydantic.BaseModel,
                                           extra="allow"):
    """ Minimally acceptable request structure """
    # NUL (\x00) excluded: als_siphon binds requestId into the sa.Text
    # request_id column and PostgreSQL rejects NUL in text values
    # (SQLSTATE 22021) — a JSON-legal \u0000 would demote the whole ALS
    # bundle to decode_error, suppressing the structured audit row for a
    # granted request (mirrors the certificationId/serialNumber NUL
    # exclusion below).
    requestId: str = pydantic.Field(min_length=1, pattern=r'^[^\x00]+$')


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

    @pydantic.model_validator(mode="before")
    @classmethod
    def _reject_non_finite_numbers_msg(cls, values: Any) -> Any:
        """ Reject NaN/Infinity anywhere in the raw request message.

        extra="allow" admits unknown keys that are retained in the ALS
        rx_envelope JSON column, so the whole raw dict must be
        RFC 8259-clean, not only the declared fields (the
        Rest_VendorExtension.parameters validator alone could be
        bypassed via an undeclared top-level key).
        """
        stack: List[Any] = [values]
        while stack:
            item = stack.pop()
            if isinstance(item, float) and not math.isfinite(item):
                raise ValueError(
                    "non-finite numbers (NaN/Infinity) not allowed in "
                    "AFC request messages")
            if isinstance(item, dict):
                stack.extend(item.values())
            elif isinstance(item, (list, tuple)):
                stack.extend(item)
        return values

    @pydantic.field_validator("availableSpectrumInquiryRequests")
    @classmethod
    def _reject_duplicate_request_ids(
            cls, requests: List[Rest_AvailableSpectrumInquiryRequest]) \
            -> List[Rest_AvailableSpectrumInquiryRequest]:
        """ Reject duplicate requestId values within one message.

        ALS audit assembly (als_siphon.py take_apart()) keys
        request/response pairs on requestId; duplicates collapse N
        independently-granted pairs into one persisted row and demote
        the rest to decode_error orphans (audit understatement).
        """
        seen = set()
        for req in requests:
            if req.requestId in seen:
                raise ValueError(
                    "duplicate requestId values in "
                    "availableSpectrumInquiryRequests")
            seen.add(req.requestId)
        return requests


class Rest_CertificationId_1_4(pydantic.BaseModel,
                               extra="forbid"):
    """ Certification definition used in AFC 1.4 requests """
    # Excludes '|': rcache's ApDbPk.from_req joins rulesetId/id across the
    # whole certificationId list with "|" to build the cache primary key
    # (rcache_models.py); an embedded '|' would let a crafted list collide
    # with a different device's decomposition of the same joined string
    # (CWE-180). serialNumber below already restricts to a safe charset for
    # the same class of reason.
    # NUL (\x00) additionally excluded: JSON-legal \u0000 passes ^[^|]+$
    # but PostgreSQL rejects NUL in text values (SQLSTATE 22021), letting a
    # crafted certificationId turn the cert lookup into a DB error on demand
    # (CWE-617 DoS path).
    # Whitespace (\s) additionally excluded: CertId allow rows and
    # AccessPointDeny rows are compared byte-exact against the request
    # value, and deny rows are stored stripped — a request id carrying
    # stray whitespace could only match a non-canonical allow row that
    # deny-listing can never revoke (CWE-180, S0158-11).
    rulesetId: str = pydantic.Field(min_length=1, max_length=64,
                                    pattern=r'^[^|\x00\s]+$')
    # Whitespace additionally excluded from id (mirrors the msghnd/ratapi
    # ingress guard in ratapi/views/ratafc.py): deny-list rows are stored
    # stripped, so a whitespace-bearing variant of a denied certificationId
    # must fail validation instead of reaching the cert/deny lookups in a
    # non-canonical form.
    id: str = pydantic.Field(min_length=1, max_length=64,
                             pattern=r'^[^|\x00\s]+$')


class Rest_DeviceDescriptor_1_4(pydantic.BaseModel,
                                extra="forbid"):
    """ Device definition used in AFC 1.4 requests """
    # max_length=64 keeps ingress congruent with the RatDB
    # access_point_deny.serial_number String(64) column (aaa.py): a longer
    # self-reported serial could never be deny-listed per-serial, defeating
    # revocation. Mirrors the explicit 64-char cap on certificationId above.
    serialNumber: str = pydantic.Field(min_length=1, max_length=64,
                                       pattern=r'^[A-Za-z0-9._-]+$')
    certificationId: List[Rest_CertificationId_1_4] = \
        pydantic.Field(min_length=1, max_length=16)


class _Rest_GeoPoint_1_4(pydantic.BaseModel, extra="allow"):
    """ Geodetic point — finite, in-range lat/lon.

    Typed to mirror what the ALS siphon's LocationTableUpdater consumes
    (float(latitude)/float(longitude) into a PostGIS POINT): a point the
    siphon cannot parse or PostGIS cannot store must be rejected here at
    the API boundary, BEFORE a grant is computed, so one malformed
    sub-request cannot void the structured audit rows of its co-batched
    siblings (als_siphon.py LocationTableUpdater._make_rows raises
    JsonFormatError and the fallback writer degrades per BUNDLE).
    """
    latitude: float = pydantic.Field(ge=-90, le=90, allow_inf_nan=False)
    longitude: float = pydantic.Field(ge=-180, le=180, allow_inf_nan=False)


class _Rest_RadialVector_1_4(pydantic.BaseModel, extra="allow"):
    """ RadialPolygon boundary vector — length typed for the ALS consumer """
    length: float = pydantic.Field(ge=0, allow_inf_nan=False)
    angle: Optional[float] = None


class Rest_LinearPolygon_1_4(pydantic.BaseModel, extra="allow"):
    """ LinearPolygon location shape — vertex count capped to bound engine allocation """
    outerBoundary: List[_Rest_GeoPoint_1_4] = \
        pydantic.Field(min_length=3, max_length=1000)


class Rest_RadialPolygon_1_4(pydantic.BaseModel, extra="allow"):
    """ RadialPolygon location shape — vertex count capped to bound engine allocation """
    center: _Rest_GeoPoint_1_4
    outerBoundary: List[_Rest_RadialVector_1_4] = \
        pydantic.Field(min_length=1, max_length=1000)


class _Rest_Ellipse_1_4(pydantic.BaseModel, extra="allow"):
    """ Ellipse location shape — center and majorAxis typed for the ALS consumer """
    center: Optional[_Rest_GeoPoint_1_4] = None
    majorAxis: Optional[float] = pydantic.Field(default=None, ge=0, allow_inf_nan=False)


class _Rest_Elevation_1_4(pydantic.BaseModel, extra="allow"):
    """ Elevation — fields typed to mirror LocationTableUpdater consumption """
    height: Optional[float] = pydantic.Field(default=None, ge=-500, le=100000, allow_inf_nan=False)
    heightType: Optional[Literal["AGL", "AMSL"]] = None
    verticalUncertainty: Optional[float] = pydantic.Field(default=None, ge=0, allow_inf_nan=False)


class Rest_Location_1_4(pydantic.BaseModel, extra="allow"):
    """ Device location — declares polygon shapes to enforce vertex-count
    limits; elevation/indoorDeployment typed to what the ALS siphon's
    LocationTableUpdater dereferences so requests whose audit record the
    siphon cannot persist are rejected before compute """
    linearPolygon: Optional[Rest_LinearPolygon_1_4] = None
    radialPolygon: Optional[Rest_RadialPolygon_1_4] = None
    ellipse: Optional[_Rest_Ellipse_1_4] = None
    elevation: Optional[_Rest_Elevation_1_4] = None
    # StrictInt, non-nullable: the siphon's ji() accepts only real ints —
    # lax pydantic int would coerce "1", and Optional would admit an
    # explicit null that .get("indoorDeployment", 0) passes through to
    # ji() as None; both poison the audit bundle. Absent key -> default 0
    # (validation-only model; the raw req_dict is what goes to the engine).
    indoorDeployment: pydantic.StrictInt = 0


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
    # NUL excluded for ALS ingress-persistence congruence — see
    # Rest_AvailableSpectrumInquiryRequest.requestId above.
    requestId: str = pydantic.Field(min_length=1, pattern=r'^[^\x00]+$')
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
