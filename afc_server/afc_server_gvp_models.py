""" Request and response models for the Geofencing System to GVP Device
Interface (SDI). """

import datetime
from typing import List, Optional, Union

import pydantic

from afc_server_models import (Rest_Response, Rest_SupplementalInfo,
                               Rest_VendorExtension)

# Response codes used when translating a validation failure (SDI Table 23).
GVP_GENERAL_FAILURE_CODE = -1
GVP_MISSING_PARAM_CODE = 102
GVP_INVALID_VALUE_CODE = 103
GVP_UNEXPECTED_PARAM_CODE = 106

# Protocol Version of SDI section 4.1, distinct from the document version.
GVP_PROTOCOL_VERSION = "1.0"


class GvpMissingParam(ValueError):
    """ Raised by a validator when a conditionally required field is
    absent. """


class GvpUnexpectedParam(ValueError):
    """ Raised by a validator when a field is present but its condition is
    not met. """


# SUCCESS response code (SDI Table 23).
GVP_SUCCESS_RESPONSE_CODE = 0

# availabilityExpireTime format required by SDI Table 18.
GVP_EXPIRE_TIME_FORMAT = "%Y-%m-%dT%H:%M:%SZ"

# The four mutually exclusive geometry fields of SDI Table 9.
GVP_GEOMETRY_FIELDS = ("ellipse", "circle", "linearPolygon", "radialPolygon")

# indoorDeployment values of SDI Table 9.
GVP_INDOOR_DEPLOYMENT_VALUES = (0, 1, 2)

# Polygon vertex bounds, shared by SDI Tables 12 and 13.
GVP_MIN_POLYGON_VERTICES = 3
GVP_MAX_POLYGON_VERTICES = 300

# SDI types numeric fields as "number" and constrains some to integers.
# A plain int annotation would coerce, silently truncating 5925.5.
GvpNumber = Union[pydantic.StrictInt, pydantic.StrictFloat]

# SDI Table 19 gives response geometry as positional arrays, latitude first.
GvpLatLonPair = pydantic.conlist(GvpNumber, min_items=2, max_items=2)
GvpCircleTriple = pydantic.conlist(GvpNumber, min_items=3, max_items=3)


def _check_lat_lon(lat, lon):
    """ Range check a latitude and longitude from a positional array. """
    if not -90.0 <= lat <= 90.0:
        raise ValueError("latitude %r is outside [-90, 90]" % (lat,))
    if not -180.0 <= lon <= 180.0:
        raise ValueError("longitude %r is outside [-180, 180]" % (lon,))


def _require_whole(value, what):
    """ Enforce a spec statement that a value shall be an integer. """
    if float(value) != int(value):
        raise ValueError("%s must be a whole number" % what)
    return value


def _check_polygon_vertices(vertices):
    """ Vertex bounds shared by SDI Tables 12 and 13: 3 to 300 unique. """
    if len(vertices) < GVP_MIN_POLYGON_VERTICES:
        raise ValueError(
            "at least %d vertices are required" % GVP_MIN_POLYGON_VERTICES)
    if len(vertices) > GVP_MAX_POLYGON_VERTICES:
        raise ValueError(
            "at most %d vertices may be used" % GVP_MAX_POLYGON_VERTICES)
    distinct = {tuple(v.dict().values()) for v in vertices}
    if len(distinct) != len(vertices):
        raise ValueError("vertices must be unique")
    return vertices


class Rest_Gvp_Point(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Point object (SDI Table 14). """
    longitude: float = pydantic.Field(
        ge=-180.0, le=180.0, description="Degrees east, WGS 84")
    latitude: float = pydantic.Field(
        ge=-90.0, le=90.0, description="Degrees north, WGS 84")


class Rest_Gvp_Vector(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Vector object (SDI Table 15). """
    length: float = pydantic.Field(
        ge=0.0, description="Distance from the Point, in meters")
    angle: float = pydantic.Field(
        ge=0.0, le=360.0,
        description="Bearing in degrees, clockwise from true north")


class Rest_Gvp_FrequencyRange(pydantic.BaseModel,
                              extra=pydantic.Extra.forbid):
    """ FrequencyRange object (SDI Table 16). """
    lowFrequency: GvpNumber = pydantic.Field(
        description="Lowest frequency of the range, in MHz")
    highFrequency: GvpNumber = pydantic.Field(
        description="Highest frequency of the range, in MHz")

    @pydantic.validator("lowFrequency", "highFrequency")
    def positive_whole_number(cls, v):
        """ Table 16 requires an integer value. """
        if v <= 0:
            raise ValueError("must be a positive frequency in MHz")
        return _require_whole(v, "frequency")

    @pydantic.root_validator(skip_on_failure=True)
    def low_below_high(cls, values):
        """ Reject empty and inverted ranges. """
        low = values.get("lowFrequency")
        high = values.get("highFrequency")
        if low is not None and high is not None and low >= high:
            raise ValueError("lowFrequency must be less than highFrequency")
        return values


class Rest_Gvp_Ellipse(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Ellipse object (SDI Table 10). """
    center: Rest_Gvp_Point
    majorAxis: GvpNumber = pydantic.Field(
        description="Major semi axis, in meters")
    minorAxis: GvpNumber = pydantic.Field(
        description="Minor semi axis, in meters")
    orientation: float = pydantic.Field(
        ge=0.0, le=180.0,
        description="Orientation of majorAxis in degrees, clockwise from "
                    "true north")

    @pydantic.validator("majorAxis", "minorAxis")
    def positive_whole_axis(cls, v):
        """ Table 10 requires a positive integer. """
        if v <= 0:
            raise ValueError("must be a positive length in meters")
        return _require_whole(v, "axis length")


class Rest_Gvp_Circle(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Circle object (SDI Table 11). """
    longitude: float = pydantic.Field(
        ge=-180.0, le=180.0, description="Degrees east, WGS 84")
    latitude: float = pydantic.Field(
        ge=-90.0, le=90.0, description="Degrees north, WGS 84")
    radius: GvpNumber = pydantic.Field(description="Radius in meters")

    @pydantic.validator("radius")
    def non_negative_whole_radius(cls, v):
        """ Table 11 requires an integer value. """
        if v < 0:
            raise ValueError("must not be negative")
        return _require_whole(v, "radius")


class Rest_Gvp_LinearPolygon(pydantic.BaseModel,
                             extra=pydantic.Extra.forbid):
    """ LinearPolygon object (SDI Table 12). """
    outerBoundary: List[Rest_Gvp_Point] = pydantic.Field(
        description="Polygon vertices, 3 to 300 unique points")

    @pydantic.validator("outerBoundary")
    def check_vertices(cls, v):
        return _check_polygon_vertices(v)


class Rest_Gvp_RadialPolygon(pydantic.BaseModel,
                             extra=pydantic.Extra.forbid):
    """ RadialPolygon object (SDI Table 13). """
    center: Rest_Gvp_Point
    outerBoundary: List[Rest_Gvp_Vector] = pydantic.Field(
        description="Polygon vertices, 3 to 300 unique vectors")

    @pydantic.validator("outerBoundary")
    def check_vertices(cls, v):
        return _check_polygon_vertices(v)


class Rest_Gvp_CertificationId(pydantic.BaseModel,
                               extra=pydantic.Extra.forbid):
    """ CertificationId object (SDI Table 8). """
    rulesetId: str = pydantic.Field(
        min_length=1, description="Identifier of the regulatory rules")
    id: str = pydantic.Field(
        min_length=1,
        description="Certification ID of the GVP Access Point")


class Rest_Gvp_DeviceDescriptor(pydantic.BaseModel,
                                extra=pydantic.Extra.forbid):
    """ DeviceDescriptor object (SDI Table 7). """
    certificationId: List[Rest_Gvp_CertificationId] = pydantic.Field(
        min_items=1,
        description="Certification IDs and corresponding rulesets")
    serialNumber: Optional[str] = pydantic.Field(
        None, min_length=1, description="Device serial number")
    deviceModel: Optional[str] = pydantic.Field(
        None, min_length=1, description="Device model")

    @pydantic.root_validator(skip_on_failure=True)
    def serial_or_model(cls, values):
        """ Apply the Table 7 conditional requirement. """
        if values.get("serialNumber") is None and \
                values.get("deviceModel") is None:
            raise GvpMissingParam(
                "one of serialNumber or deviceModel is required")
        return values


class Rest_Gvp_Location(pydantic.BaseModel, extra=pydantic.Extra.allow):
    """ Location object (SDI Table 9), the area of intended operation. """
    ellipse: Optional[Rest_Gvp_Ellipse] = None
    circle: Optional[Rest_Gvp_Circle] = None
    linearPolygon: Optional[Rest_Gvp_LinearPolygon] = None
    radialPolygon: Optional[Rest_Gvp_RadialPolygon] = None
    indoorDeployment: Optional[GvpNumber] = pydantic.Field(
        None, description="0 unknown, 1 indoor, 2 outdoor")

    @pydantic.validator("indoorDeployment")
    def known_deployment(cls, v):
        """ Table 9 maps this field to 0, 1 or 2. """
        if v not in GVP_INDOOR_DEPLOYMENT_VALUES:
            raise ValueError(
                "must be one of %s" % (GVP_INDOOR_DEPLOYMENT_VALUES,))
        return v

    @pydantic.root_validator(skip_on_failure=True)
    def exactly_one_geometry(cls, values):
        """ Apply the Table 9 mutual exclusion. """
        present = [name for name in GVP_GEOMETRY_FIELDS
                   if values.get(name) is not None]
        if not present:
            raise GvpMissingParam(
                "one of %s is required" % (", ".join(GVP_GEOMETRY_FIELDS),))
        if len(present) > 1:
            raise GvpUnexpectedParam(
                "only one geometry may be given, found %s"
                % (", ".join(present),))
        return values


class Rest_Gvp_ExclusionZoneInquiryRequest(pydantic.BaseModel,
                                           extra=pydantic.Extra.allow):
    """ ExclusionZoneInquiryRequest object (SDI Table 6). """
    requestId: str = pydantic.Field(
        min_length=1,
        description="Unique ID of this inquiry within the message")
    deviceDescriptor: Rest_Gvp_DeviceDescriptor
    areaOfIntendedOperation: Rest_Gvp_Location
    inquiredFrequencyRange: Optional[List[Rest_Gvp_FrequencyRange]] = \
        pydantic.Field(
            None, description="Frequency ranges of interest; absent means "
                              "all GVP-applicable bands")
    desiredPsd: Optional[List[GvpNumber]] = pydantic.Field(
        None, description="Power spectral density levels in dBm/MHz")
    desiredPrecision: Optional[str] = pydantic.Field(
        None, description="Requested precision: low, medium or high")
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None


class Rest_Gvp_ReqMsg(pydantic.BaseModel, extra=pydantic.Extra.allow):
    """ ExclusionZoneInquiryRequestMessage object (SDI Table 5). """
    version: str = pydantic.Field(
        min_length=1, description="Protocol Version")
    exclusionZoneInquiryRequests: \
        List[Rest_Gvp_ExclusionZoneInquiryRequest] = pydantic.Field(
            min_items=1, description="One or more exclusion zone inquiries")
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None

    @pydantic.validator("exclusionZoneInquiryRequests")
    def unique_request_ids(cls, v):
        """ Table 6 requires requestId unique within the message. """
        ids = [r.requestId for r in v]
        if len(set(ids)) != len(ids):
            raise ValueError("requestId must be unique within the message")
        return v


class Rest_Gvp_ExclusionZoneInfo(pydantic.BaseModel,
                                 extra=pydantic.Extra.forbid):
    """ ExclusionZoneInfo object (SDI Table 19). """
    exclusionZoneId: Optional[str] = pydantic.Field(
        None, description="Optional non-unique label, for debugging only")
    exclusionZoneFrequencyRange: Rest_Gvp_FrequencyRange = pydantic.Field(
        description="Range over which operation is not permitted inside the "
                    "zone")
    psdLevel: GvpNumber = pydantic.Field(
        description="PSD in dBm/MHz; operation at this level is allowed only "
                    "outside the zone")
    enclosingCircle: Optional[Rest_Gvp_Circle] = pydantic.Field(
        None, description="Single circle covering the zone, for cheap "
                          "rejection by the GVP Access Point")
    polygons: Optional[List[List[GvpLatLonPair]]] = pydantic.Field(
        None, min_items=1,
        description="Polygons as arrays of [latitude, longitude] corner "
                    "points, WGS 84")
    circles: Optional[List[GvpCircleTriple]] = pydantic.Field(
        None, min_items=1,
        description="Circles as [center latitude, center longitude, radius "
                    "in meters], WGS 84")

    @pydantic.validator("psdLevel")
    def whole_psd(cls, v):
        """ Table 19 requires an integer value. """
        return _require_whole(v, "psdLevel")

    @pydantic.validator("polygons")
    def check_polygons(cls, v):
        """ Range check the [latitude, longitude] corner points. """
        for polygon in v:
            for lat, lon in polygon:
                _check_lat_lon(lat, lon)
        return v

    @pydantic.validator("circles")
    def check_circles(cls, v):
        """ Table 19 requires an integer radius in meters. """
        for lat, lon, radius in v:
            _check_lat_lon(lat, lon)
            if radius < 0:
                raise ValueError("radius must not be negative")
            _require_whole(radius, "radius")
        return v

    @pydantic.root_validator(skip_on_failure=True)
    def has_some_geometry(cls, values):
        """ A zone with neither polygons nor circles describes no area. """
        if values.get("polygons") is None and values.get("circles") is None:
            raise GvpMissingParam(
                "at least one of polygons or circles is required")
        return values


class Rest_Gvp_ExclusionZoneInquiryResponse(pydantic.BaseModel,
                                            extra=pydantic.Extra.forbid):
    """ ExclusionZoneInquiryResponse object (SDI Table 18). """
    requestId: str = pydantic.Field(
        description="Echoes the requestId of the inquiry")
    rulesetId: str = pydantic.Field(
        min_length=1,
        description="Regulatory rules used to determine these zones")
    exclusionZoneInfo: Optional[List[Rest_Gvp_ExclusionZoneInfo]] = \
        pydantic.Field(
            None, description="Zero or more zones; empty means none overlap "
                              "the area of intended operation")
    availabilityExpireTime: Optional[str] = pydantic.Field(
        None, description="UTC expiry as YYYY-MM-DDThh:mm:ssZ")
    response: Rest_Response = pydantic.Field(
        description="Outcome of the inquiry")
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None

    @pydantic.validator("availabilityExpireTime")
    def utc_expire_time(cls, v):
        """ Table 18 fixes the format as YYYY-MM-DDThh:mm:ssZ. """
        try:
            datetime.datetime.strptime(v, GVP_EXPIRE_TIME_FORMAT)
        except ValueError:
            raise ValueError(
                "must be UTC formatted as YYYY-MM-DDThh:mm:ssZ")
        return v

    @pydantic.root_validator(skip_on_failure=True)
    def zone_data_iff_success(cls, values):
        """ Apply the Table 18 conditional requirement in both directions. """
        conditional = ("exclusionZoneInfo", "availabilityExpireTime")
        response = values.get("response")
        success = (response is not None and
                   response.responseCode == GVP_SUCCESS_RESPONSE_CODE)
        absent = [n for n in conditional if values.get(n) is None]
        if success and absent:
            raise GvpMissingParam(
                "%s required when the response code is SUCCESS"
                % (", ".join(absent),))
        present = [n for n in conditional if values.get(n) is not None]
        if not success and present:
            raise GvpUnexpectedParam(
                "%s permitted only when the response code is SUCCESS"
                % (", ".join(present),))
        return values


class Rest_Gvp_RespMsg(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ ExclusionZoneInquiryResponseMessage object (SDI Table 17). """
    version: str = pydantic.Field(
        min_length=1, description="Protocol Version, matching the request")
    exclusionZoneInquiryResponses: \
        List[Rest_Gvp_ExclusionZoneInquiryResponse] = pydantic.Field(
            min_items=1, description="One response per inquiry")
    vendorExtensions: Optional[List[Rest_VendorExtension]] = None

    @pydantic.validator("exclusionZoneInquiryResponses")
    def unique_request_ids(cls, v):
        """ Each response echoes a distinct inquiry. """
        ids = [r.requestId for r in v]
        if len(set(ids)) != len(ids):
            raise ValueError("requestId must be unique within the message")
        return v


def render_error_location(loc):
    """ Render a pydantic error location as a dotted field path, such as
    exclusionZoneInquiryRequests[0].areaOfIntendedOperation. """
    parts = []
    for item in loc:
        if item == "__root__":
            continue
        if isinstance(item, int):
            if parts:
                parts[-1] = "%s[%d]" % (parts[-1], item)
            else:
                parts.append("[%d]" % item)
        else:
            parts.append(str(item))
    return ".".join(parts)


def response_from_validation_error(error):
    """ Translate a pydantic ValidationError into an SDI Response (SDI
    Tables 23 and 24). """
    return response_from_error_entries(error.errors())


def response_from_error_entries(entries):
    """ Translate pydantic error entries into an SDI Response (SDI Tables 23
    and 24). """
    buckets = {
        GVP_MISSING_PARAM_CODE: [],
        GVP_UNEXPECTED_PARAM_CODE: [],
        GVP_INVALID_VALUE_CODE: [],
    }
    for entry in entries:
        kind = entry["type"]
        if kind == "value_error.missing" or \
                kind.endswith(GvpMissingParam.__name__.lower()):
            code = GVP_MISSING_PARAM_CODE
        elif kind == "value_error.extra" or \
                kind.endswith(GvpUnexpectedParam.__name__.lower()):
            code = GVP_UNEXPECTED_PARAM_CODE
        else:
            code = GVP_INVALID_VALUE_CODE
        path = render_error_location(entry["loc"])
        if path and path not in buckets[code]:
            buckets[code].append(path)

    for code in (GVP_MISSING_PARAM_CODE, GVP_UNEXPECTED_PARAM_CODE,
                 GVP_INVALID_VALUE_CODE):
        if buckets[code]:
            chosen = code
            break
    else:
        chosen = GVP_INVALID_VALUE_CODE

    field = {
        GVP_MISSING_PARAM_CODE: "missingParams",
        GVP_UNEXPECTED_PARAM_CODE: "unexpectedParams",
        GVP_INVALID_VALUE_CODE: "invalidParams",
    }[chosen]
    supplemental = Rest_SupplementalInfo(**{field: buckets[chosen] or None})

    described = []
    for entry in entries:
        path = render_error_location(entry["loc"])
        described.append("%s: %s" % (path, entry["msg"]) if path
                         else entry["msg"])
    return Rest_Response(responseCode=chosen,
                         shortDescription="; ".join(described),
                         supplementalInfo=supplemental)


def _inquiry_index(loc):
    """ Index of the inquiry an error belongs to, None at message level. """
    return loc[1] if (len(loc) >= 2 and
                      loc[0] == "exclusionZoneInquiryRequests" and
                      isinstance(loc[1], int)) else None


def _echoed_request_id(inquiry):
    """ requestId to echo per SDI Table 18, empty when the inquiry omits
    it. """
    value = inquiry.get("requestId") if isinstance(inquiry, dict) else None
    return value if isinstance(value, str) else ""


def resp_msg_from_validation_error(error, body, ruleset_id):
    """ Build the response message for a request that failed validation. A
    message level failure yields one response, since Table 18 correlates by
    requestId alone; per inquiry failures yield one each per section 3.2. """
    inquiries = body.get("exclusionZoneInquiryRequests")
    if not isinstance(inquiries, list):
        inquiries = []
    message_level = []
    grouped = [[] for _ in inquiries]
    for entry in error.errors():
        index = _inquiry_index(entry["loc"])
        if index is None or index >= len(inquiries):
            message_level.append(entry)
        else:
            grouped[index].append(entry)

    def response_entry(request_id, response):
        return Rest_Gvp_ExclusionZoneInquiryResponse(
            requestId=request_id, rulesetId=ruleset_id, response=response)

    if message_level or not inquiries:
        all_entries = message_level + [e for g in grouped for e in g]
        return _resp_msg(
            body, [response_entry("",
                                  response_from_error_entries(all_entries))])

    responses = []
    for inquiry, entries in zip(inquiries, grouped):
        if entries:
            response = response_from_error_entries(entries)
        else:
            response = Rest_Response(
                responseCode=GVP_GENERAL_FAILURE_CODE,
                shortDescription="inquiry not processed, another inquiry "
                                 "in the message was invalid")
        responses.append(
            response_entry(_echoed_request_id(inquiry), response))
    return _resp_msg(body, responses)


def _resp_msg(body, responses):
    """ Wrap responses in a message, echoing the request version per SDI
    Table 17. """
    version = body.get("version")
    return Rest_Gvp_RespMsg(
        version=version if isinstance(version, str) and version
        else GVP_PROTOCOL_VERSION,
        exclusionZoneInquiryResponses=responses)
