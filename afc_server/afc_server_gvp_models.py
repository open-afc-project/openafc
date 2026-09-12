""" Request and response models for the Geofencing System to GVP Device
Interface (SDI). """

from typing import List, Optional, Union

import pydantic

from afc_server_models import Rest_VendorExtension

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
            raise ValueError(
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
            raise ValueError(
                "one of %s is required" % (", ".join(GVP_GEOMETRY_FIELDS),))
        if len(present) > 1:
            raise ValueError(
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
