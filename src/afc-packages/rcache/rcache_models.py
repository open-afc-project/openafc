""" AFC Request Cache external interface structures """
#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# pylint: disable=too-few-public-methods, invalid-name, wrong-import-order
# pylint: disable=too-many-boolean-expressions
# pylint: disable=no-member

import datetime
import enum
import json
import pydantic
from typing import Any, Dict, List, Optional

from rcache_common import dp

__all__ = ["AfcReqRespKey", "ApDbPk", "ApDbRecord", "ApDbRespState",
           "FuncSwitch", "IfDbExists", "LatLonRect", "RatapiAfcConfig",
           "RatapiRulesetIds", "RcacheClientSettings", "RcacheInvalidateReq",
           "RcacheServiceSettings", "RcacheSpatialInvalidateReq",
           "RcacheStatus", "RcacheUpdateReq", "RmqReqRespKey"]

# Format of response expiration time
RESP_EXPIRATION_FORMAT = "%Y-%m-%dT%H:%M:%SZ"


class IfDbExists(enum.Enum):
    """ What to do if cache database exists on RCache service startup """
    # Leave as is (default)
    leave = "leave"
    # Completely recreate (extra stuff, like alembic, will be removed)
    recreate = "recreate"
    # Clean cache table, leaving others intact
    clean = "clean"


class RcacheServiceSettings(pydantic.BaseSettings):
    """ Rcache service parameters, passed via environment """

    class Config:
        """ Metainformation """
        # Prefix of environment variables
        env_prefix = "RCACHE_"

    enabled: bool = \
        pydantic.Field(
            True, title="Rcache enabled (False for legacy file-based cache")

    port: int = pydantic.Field(..., title="Port this service listens on",
                               env="RCACHE_CLIENT_PORT")
    postgres_dsn: pydantic.PostgresDsn = \
        pydantic.Field(
            ...,
            title="Postgres DSN: "
            "postgresql://[user[:password]]@host[:port]/database[?...]")
    if_db_exists: IfDbExists = \
        pydantic.Field(
            IfDbExists.leave,
            title="What to do if cache database already exists: 'leave' "
            "(leave as is - default), 'recreate' (completely recreate - e.g. "
            "removing alembic, if any), 'clean' (only clean the cache table)")
    precompute_quota: int = \
        pydantic.Field(
            10, title="Number of simultaneous precomputing requests in flight")
    afc_req_url: Optional[pydantic.AnyHttpUrl] = \
        pydantic.Field(
            None,
            title="RestAPI URL to send AFC Requests for precomputation")
    rulesets_url: Optional[pydantic.AnyHttpUrl] = \
        pydantic.Field(
            None,
            title="RestAPI URL to retrieve list of active Ruleset IDs")
    config_retrieval_url: Optional[pydantic.AnyHttpUrl] = \
        pydantic.Field(
            None,
            title="RestAPI URL to retrieve AFC Config for given Ruleset ID")

    @classmethod
    @pydantic.root_validator(pre=True)
    def _remove_empty(cls, values: Dict[str, Any]) -> Dict[str, Any]:
        """ Removes empty entries """
        for key in list(values.keys()):
            if values[key] == "":
                del values[key]
        return values


class RcacheClientSettings(pydantic.BaseSettings):
    """ Parameters of Rcache clients in various services, passed via
    environment """
    class Config:
        """ Metainformation """
        # Prefix of environment variables
        env_prefix = "RCACHE_"

    enabled: bool = \
        pydantic.Field(True,
                       title="Rcache enabled (False for legacy file-based "
                       "cache. Default is enabled")
    postgres_dsn: Optional[pydantic.PostgresDsn] = \
        pydantic.Field(
            None,
            title="Postgres DSN: "
            "postgresql://[user[:password]]@host[:port]/database[?...]")
    service_url: Optional[pydantic.AnyHttpUrl] = \
        pydantic.Field(None, title="Rcache server base RestAPI URL")
    rmq_dsn: Optional[pydantic.AmqpDsn] = \
        pydantic.Field(
            None,
            title="RabbitMQ AMQP DSN: amqp://[user[:password]]@host[:port]")
    update_on_send: bool = \
        pydantic.Field(
            True,
            title="True to update cache from worker (on sending response), "
            "False to update cache on msghnd (on receiving response)")

    @classmethod
    @pydantic.root_validator(pre=True)
    def _remove_empty(cls, values: Dict[str, Any]) -> Dict[str, Any]:
        """ Removes empty entries """
        for key in list(values.keys()):
            if values[key] == "":
                del values[key]
        return values

    def validate_for(self, db: bool = False, rmq: bool = False,
                     rcache: bool = False) -> None:
        """ Generates exception if Rcache is enabled, but parameter(s) for
        some its required aspect are not set

        Arguments:
        db     -- Check parameters for Postgres DB connection
        rmq    -- Check parameters for RabbitMQ connection
        rcache -- Check parameters for Rcache service connection
        """
        if not self.enabled:
            return
        for predicate, attr in [(db, "postgres_dsn"), (rmq, "rmq_dsn"),
                                (rcache, "service_url")]:
            if (not predicate) or getattr(self, attr):
                continue
            raise ValueError(
                f"RcacheClientSettings.{attr} Rcache client configuration "
                f"parameter neither set explicitly nor via "
                f"{self.Config.env_prefix}{attr.upper()} environment "
                f"variable")


class LatLonRect(pydantic.BaseModel):
    """ Latitude/longitude rectangle, used in spatial cache invalidation """
    min_lat: float = \
        pydantic.Field(..., title="Minimum latitude in north-positive degrees")
    max_lat: float = \
        pydantic.Field(..., title="Maximum latitude in north-positive degrees")
    min_lon: float = \
        pydantic.Field(..., title="Minimum longitude in east-positive degrees")
    max_lon: float = \
        pydantic.Field(..., title="Maximum longitude in east-positive degrees")

    def short_str(self) -> str:
        """ Condensed string representation """
        parts: List[str] = []
        for min_val, max_val, pos_semi, neg_semi in \
                [(self.min_lat, self.max_lat, "N", "S"),
                 (self.min_lon, self.max_lon, "E", "W")]:
            if (min_val * max_val) >= 0:
                # Same hemisphere
                parts.append(
                    f"[{min(abs(min_val), abs(max_val))}-"
                    f"{max(abs(min_val), abs(max_val))}]"
                    f"{pos_semi if (min_val + max_val) >= 0 else neg_semi}")
            else:
                # Different hemispheres
                parts.append(
                    f"[{abs(min_val)}{neg_semi}-"
                    f"{abs(max_val)}{pos_semi}]")
        return ", ".join(parts)


class RcacheInvalidateReq(pydantic.BaseModel):
    """ RCache REST API cache invalidation request """
    ruleset_ids: Optional[List[str]] = \
        pydantic.Field(None,
                       title="Optional list of ruleset IDs to invalidate. By "
                       "default invalidates everything")


class RcacheSpatialInvalidateReq(pydantic.BaseModel):
    """ RCache REST API spatial invalidation request """
    tiles: List[LatLonRect] = \
        pydantic.Field(..., title="List of rectangles, containing changed FSs")


class AfcReqRespKey(pydantic.BaseModel):
    """ Information about single computed result, used in request cache update
    request """
    afc_req: str = pydantic.Field(..., title="AFC Request as string")
    afc_resp: str = pydantic.Field(..., title="AFC Response as string")
    req_cfg_digest: str = \
        pydantic.Field(
            ..., title="Request/Config hash (cache lookup key) as string")

    class Config:
        """ Metadata """
        # May be constructed from RmqReqRespKey
        orm_mode = True


class RcacheUpdateReq(pydantic.BaseModel):
    """ RCache REST API cache update request """
    req_resp_keys: List[AfcReqRespKey] = \
        pydantic.Field(..., title="Computation results to add to cache")


class RcacheStatus(pydantic.BaseModel):
    """ Rcache service status information """
    up_time: datetime.timedelta = pydantic.Field(..., title="Service up time")
    db_connected: bool = \
        pydantic.Field(..., title="Database successfully connected")
    all_tasks_running: bool = \
        pydantic.Field(..., title="All tasks running (none crashed)")
    invalidation_enabled: bool = \
        pydantic.Field(..., title="Invalidation enabled")
    update_enabled: bool = pydantic.Field(..., title="Update enabled")
    precomputation_enabled: bool = \
        pydantic.Field(..., title="Precomputation enabled")
    precomputation_quota: int = \
        pydantic.Field(...,
                       title="Maximum number of simultaneous precomputations")
    num_valid_entries: int = \
        pydantic.Field(..., title="Number of valid entries in cache")
    num_invalid_entries: int = \
        pydantic.Field(
            ...,
            title="Number of invalidated (awaiting precomputation) entries in "
            "cache")
    update_queue_len: int = \
        pydantic.Field(..., title="Number of pending records in update queue")
    update_count: int = \
        pydantic.Field(..., title="Number of updates written to database")
    avg_update_write_rate: float = \
        pydantic.Field(..., title="Average number of update writes per second")
    avg_update_queue_len: float = \
        pydantic.Field(..., title="Average number of pending update records")
    num_precomputed: int = \
        pydantic.Field(..., title="Number of initiated precomputations")
    active_precomputations: int = \
        pydantic.Field(..., title="Number of active precomputations")
    avg_precomputation_rate: float = \
        pydantic.Field(
            ...,
            title="Average number precomputations initiated per per second")
    avg_schedule_lag: float = \
        pydantic.Field(
            ...,
            title="Average scheduling delay in seconds (measure of service "
            "process load)")


class AfcReqCertificationId(pydantic.BaseModel):
    """ Interesting part of AFC Request's CertificationId structure """
    rulesetId: str = \
        pydantic.Field(
            ..., title="Regulatory ruleset for which certificate was given")
    id: str = \
        pydantic.Field(..., title="Certification ID for given ruleset")


class AfcReqDeviceDescriptor(pydantic.BaseModel):
    """ Interesting part of AFC Request's DeviceDescriptor structure """
    serialNumber: str = \
        pydantic.Field(..., title="Device serial number")
    certificationId: List[AfcReqCertificationId] = \
        pydantic.Field(..., min_items=1, title="Device certifications")


class AFcReqPoint(pydantic.BaseModel):
    """ Interesting part of AFC Request's Point structure """
    longitude: float = \
        pydantic.Field(..., title="Longitude in east-positive degrees")
    latitude: float = \
        pydantic.Field(..., title="Latitude in north-positive degrees")


class AfcReqEllipse(pydantic.BaseModel):
    """ Interesting part of AFC Request's Ellipse structure """
    center: AFcReqPoint = pydantic.Field(..., title="Ellipse center")


class AfcReqLinearPolygon(pydantic.BaseModel):
    """ Interesting part of AFC Request's LinearPolygon structure """
    outerBoundary: List[AFcReqPoint] = \
        pydantic.Field(..., title="List of vertices", min_items=1)


class AfcReqRadialPolygon(pydantic.BaseModel):
    """ Interesting part of AFC Request's RadialPolygon structure """
    center: AFcReqPoint = pydantic.Field(..., title="Polygon center")


class AfcReqLocation(pydantic.BaseModel):
    """ Interesting part of AFC Request's Location structure """
    ellipse: Optional[AfcReqEllipse] = \
        pydantic.Field(None, title="Optional ellipse descriptor")
    linearPolygon: Optional[AfcReqLinearPolygon] = \
        pydantic.Field(None, title="Optional linear polygon descriptor")
    radialPolygon: Optional[AfcReqRadialPolygon] = \
        pydantic.Field(None, title="Optional radial polygon descriptor")

    @classmethod
    @pydantic.root_validator()
    def one_definition(cls, v: Dict[str, Any]) -> Dict[str, Any]:
        """ Verifies that exactly one type of AP location is specified """
        if ((0 if v.get("ellipse") is None else 1) +
                (0 if v.get("linearPolygon") is None else 1) +
                (0 if v.get("radialPolygon") is None else 1)) != 1:
            raise ValueError(
                "Not exactly one AFC Request location definition found")
        return v

    def center(self) -> AFcReqPoint:
        """ Returns location center """
        if self.ellipse is not None:
            return self.ellipse.center
        if self.radialPolygon is not None:
            return self.radialPolygon.center
        assert self.linearPolygon is not None
        lat = sum(b.latitude for b in self.linearPolygon.outerBoundary) / \
            len(self.linearPolygon.outerBoundary)
        lon: float = 0.
        lon0 = self.linearPolygon.outerBoundary[0].longitude
        for b in self.linearPolygon.outerBoundary:
            blon = b.longitude
            while blon > (lon0 + 360):
                blon -= 360
            while blon <= (lon0 - 360):
                blon += 360
            lon += blon
        lon /= len(self.linearPolygon.outerBoundary)
        return AFcReqPoint(latitude=lat, longitude=lon)


class AfcReqAvailableSpectrumInquiryRequest(pydantic.BaseModel):
    """ Interesting part of AFC Request's AvailableSpectrumInquiryRequest
    structure """
    deviceDescriptor: AfcReqDeviceDescriptor = \
        pydantic.Field(..., title="Device descriptor")
    location: AfcReqLocation = \
        pydantic.Field(..., title="Device location")


class AfcReqAvailableSpectrumInquiryRequestMessage(pydantic.BaseModel):
    """ Interesting part of AFC Request's
    AvailableSpectrumInquiryRequestMessage structure """
    availableSpectrumInquiryRequests: \
        List[AfcReqAvailableSpectrumInquiryRequest] = \
        pydantic.Field(..., min_items=1, max_items=1,
                       title="Single element list of requests")


class AfcRespResponse(pydantic.BaseModel):
    """ Interesting part of AFC Responses' status data """
    responseCode: int = pydantic.Field(..., title="Response code")


class AfcRespAvailableSpectrumInquiryResponse(pydantic.BaseModel):
    """ Interesting part of AFC Response's AvailableSpectrumInquiryResponse
    structure """
    rulesetId: str = \
        pydantic.Field(..., title="ID of ruleset used for computation")
    availabilityExpireTime: Optional[str] = \
        pydantic.Field(
            None,
            title="UTC expiration time in YYY-MM-DDThh:mm:ssZ format. Absent "
            "if response unsuccessful")
    response: AfcRespResponse = pydantic.Field(None, title="Response status")

    @classmethod
    @pydantic.validator('availabilityExpireTime')
    def check_expiration_time_format(cls, v: Optional[str]) -> Optional[str]:
        """ Checks validity of 'availabilityExpireTime' """
        if v is not None:
            datetime.datetime.strptime(v, RESP_EXPIRATION_FORMAT)
        return v


class AfcRespAvailableSpectrumInquiryResponseMessage(pydantic.BaseModel):
    """ Interesting part of AFC Response's
    AvailableSpectrumInquiryResponseMessage structure """
    availableSpectrumInquiryResponses: \
        List[AfcRespAvailableSpectrumInquiryResponse] = \
        pydantic.Field(..., min_items=1, max_items=1,
                       title="Single-element list of responses")


class RmqReqRespKey(pydantic.BaseModel):
    """ Request/Response/Digest structure, used in RabbitMQ communication """
    afc_req: Optional[str] = \
        pydantic.Field(
            ...,
            title="AFC Request as string. None if cache update being made on "
            "sender (Worker) side")
    afc_resp: Optional[str] = \
        pydantic.Field(..., title="AFC Response as string. None on failure")
    req_cfg_digest: str = \
        pydantic.Field(
            ..., title="Request/Config hash (cache lookup key) as string")


# Cache database row state (reflects status of response)
ApDbRespState = enum.Enum("ApDbRespState", ["Valid", "Invalid", "Precomp"])


class ApDbPk(pydantic.BaseModel):
    """ Fields that comprise database primary key in AP table """
    serial_number: str = pydantic.Field(..., title="Device serial number")
    rulesets: str = pydantic.Field(..., title="Concatenated ruleset IDs")
    cert_ids: str = pydantic.Field(..., title="Concatenated certification IDs")

    @classmethod
    def from_req(
            cls,
            req_pydantic:
            Optional[AfcReqAvailableSpectrumInquiryRequestMessage] = None,
            req_str: Optional[str] = None) -> "ApDbPk":
        """ Create self from either string or pydantic request message """
        if req_pydantic is None:
            assert req_str is not None
            req_pydantic = \
                AfcReqAvailableSpectrumInquiryRequestMessage.parse_raw(req_str)
        first_request = req_pydantic.availableSpectrumInquiryRequests[0]
        return \
            ApDbPk(
                serial_number=first_request.deviceDescriptor.serialNumber,
                rulesets="|".join(cert.rulesetId for cert in
                                  first_request.deviceDescriptor.
                                  certificationId),
                cert_ids="|".join(cert.id for cert in
                                  first_request.deviceDescriptor.
                                  certificationId))


class ApDbRecord(pydantic.BaseModel):
    """ Database AP record in Pydantic representation

    Structure must be kept in sync with one, defined in rcache_db.RcacheDb()
    """
    serial_number: str = pydantic.Field(..., title="Device serial number")
    rulesets: str = pydantic.Field(..., title="Concatenated ruleset IDs")
    cert_ids: str = pydantic.Field(..., title="Concatenated certification IDs")
    state: str = pydantic.Field(..., title="Response state")
    config_ruleset: str = \
        pydantic.Field(..., title="Ruleset used for computation")
    lat_deg: float = \
        pydantic.Field(..., title="North positive latitude in degrees")
    lon_deg: float = \
        pydantic.Field(..., title="East positive longitude in degrees")
    last_update: datetime.datetime = \
        pydantic.Field(..., title="Time of last update")
    req_cfg_digest: str = \
        pydantic.Field(..., title="Request/Config digest (cache lookup key")
    validity_period_sec: Optional[float] = \
        pydantic.Field(..., title="Response validity period in seconds")
    request: str = pydantic.Field(..., title="Request message as string")
    response: str = pydantic.Field(..., title="Response message as string")

    @classmethod
    def from_req_resp_key(cls, rrk: AfcReqRespKey) -> Optional["ApDbRecord"]:
        """ Construct from Request/Response/digest data
        Returns None if data not deserved to be in database
        """
        resp = \
            AfcRespAvailableSpectrumInquiryResponseMessage.parse_raw(
                rrk.afc_resp).availableSpectrumInquiryResponses[0]
        if resp.response.responseCode != 0:
            return None
        req_pydantic = \
            AfcReqAvailableSpectrumInquiryRequestMessage.parse_raw(
                rrk.afc_req)
        pk = ApDbPk.from_req(req_pydantic=req_pydantic)
        center = \
            req_pydantic.availableSpectrumInquiryRequests[0].location.center()
        return \
            ApDbRecord(
                **pk.dict(),
                state=ApDbRespState.Valid.name,
                config_ruleset=resp.rulesetId,
                lat_deg=center.latitude,
                lon_deg=center.longitude,
                last_update=datetime.datetime.now(),
                req_cfg_digest=rrk.req_cfg_digest,
                validity_period_sec=None
                if resp.availabilityExpireTime is None
                else (datetime.datetime.strptime(
                      resp.availabilityExpireTime, RESP_EXPIRATION_FORMAT) -
                      datetime.datetime.utcnow()).total_seconds(),
                request=rrk.afc_req,
                response=rrk.afc_resp)

    @classmethod
    def check_db_table(cls, table: Any) -> Optional[str]:
        """ Checks that structure is the same as given SqlAlchemy tab
        Returns None if all OK, error message otherwise
        """
        class_name = cls.schema()['title']
        properties: Dict[str, Dict[str, str]] = cls.schema()["properties"]
        if len(properties) != len(table.c):
            return f"Database table and {class_name} have different numbers " \
                f"of fields"
        for idx, (field_name, attrs) in enumerate(properties.items()):
            if field_name != table.c[idx].name:
                return f"{idx}'s field in database and in {class_name} have " \
                    f"different names: {table.c[idx].name} and {field_name} " \
                    f"respectively"
            field_type = attrs.get("type")
            column_type_name = str(table.c[idx].type)
            if ((column_type_name == "INTEGER") and
                (field_type != "integer")) or \
                    ((column_type_name == "FLOAT") and
                     (field_type != "number")) or \
                    ((column_type_name == "VARCHAR") and
                     (field_type not in ("string", None))) or \
                    ((column_type_name == "DATETIME") and
                     (attrs.get("format") != "date-time")):
                return f"Field '{field_name}' at index {idx} in the " \
                    f"database and in the {class_name} have different " \
                    f"types: {str(table.c[idx].type)} and {field_type} " \
                    f"respectively"
        if set(c.name for c in table.c if c.primary_key) != \
                set(ApDbPk.schema()["properties"].keys()):
            return "Different primary key composition in database and ApDbPk"
        return None

    def get_patched_response(self) -> str:
        """ Returns response patched with known nonconstant fields """
        ret_dict = json.loads(self.response)
        resp = ret_dict["availableSpectrumInquiryResponses"][0]
        if self.validity_period_sec is not None:
            resp["availabilityExpireTime"] = \
                datetime.datetime.strftime(
                    datetime.datetime.utcnow() +
                    datetime.timedelta(seconds=self.validity_period_sec),
                    RESP_EXPIRATION_FORMAT)
        return json.dumps(ret_dict)


class RatapiRulesetIds(pydantic.BaseModel):
    """ RatApi request to retrieve list of active ruleset IDs """
    rulesetId: List[str] = \
        pydantic.Field(
            ..., title="List of active ruleset IDs (AFC Config identifiers)")


class RatapiAfcConfig(pydantic.BaseModel):
    """ Interesting parts of AFC Config, returned by RapAPI REST request """
    maxLinkDistance: float = \
        pydantic.Field(...,
                       title="Maximum distance between AP and affected FS RX")


# Rcache functionality enable/disable switches
FuncSwitch = enum.Enum("FuncSwitch", ["Update", "Invalidate", "Precompute"])
