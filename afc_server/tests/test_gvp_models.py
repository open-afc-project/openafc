""" Unit tests for the GVP SDI request and response models. """

import pydantic
import pytest

from afc_server_gvp_models import (GVP_GEOMETRY_FIELDS,
                                   GVP_MAX_POLYGON_VERTICES,
                                   GVP_MIN_POLYGON_VERTICES,
                                   Rest_Gvp_CertificationId,
                                   Rest_Gvp_Circle,
                                   Rest_Gvp_DeviceDescriptor,
                                   Rest_Gvp_Ellipse,
                                   Rest_Gvp_ExclusionZoneInquiryRequest,
                                   Rest_Gvp_FrequencyRange,
                                   Rest_Gvp_LinearPolygon, Rest_Gvp_Location,
                                   Rest_Gvp_Point, Rest_Gvp_RadialPolygon,
                                   Rest_Gvp_ReqMsg, Rest_Gvp_Vector)

_ELLIPSE = {"center": {"longitude": -121.98, "latitude": 37.37},
            "majorAxis": 5000, "minorAxis": 5000, "orientation": 0}
_CIRCLE = {"longitude": -121.98, "latitude": 37.37, "radius": 5000}
_DEVICE = {"serialNumber": "GVP-AP-0001",
           "certificationId": [{"rulesetId": "US_47_CFR_PART_15_SUBPART_E_GVP",
                                "id": "WFA-GVP-CERT-0001"}]}


def _request(**over):
    """ A minimal valid ExclusionZoneInquiryRequest, overridable. """
    data = {"requestId": "req-1", "deviceDescriptor": dict(_DEVICE),
            "areaOfIntendedOperation": {"ellipse": dict(_ELLIPSE)}}
    data.update(over)
    return data


def _points(n):
    """ n distinct points, spaced far enough apart to stay unique. """
    return [{"longitude": -122.0 + i * 0.01, "latitude": 37.0} for i in range(n)]


def _vectors(n):
    """ n distinct vectors, differing by bearing. """
    return [{"length": 1000.0, "angle": i * (360.0 / n)} for i in range(n)]


def test_point_accepts_appendix_a_center():
    """ The center of the Appendix A area of intended operation. """
    p = Rest_Gvp_Point(longitude=-121.983601, latitude=37.375397)
    assert p.longitude == -121.983601
    assert p.latitude == 37.375397


@pytest.mark.parametrize("lon,lat", [
    (-180.0, -90.0),
    (180.0, 90.0),
    (0.0, 0.0),
])
def test_point_accepts_range_boundaries(lon, lat):
    """ Table 14 ranges, read as inclusive. """
    Rest_Gvp_Point(longitude=lon, latitude=lat)


@pytest.mark.parametrize("field,value", [
    ("longitude", -180.000001),
    ("longitude", 180.000001),
    ("latitude", -90.000001),
    ("latitude", 90.000001),
])
def test_point_rejects_out_of_range(field, value):
    data = {"longitude": 0.0, "latitude": 0.0}
    data[field] = value
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Point(**data)
    assert exc.value.errors()[0]["loc"] == (field,)


@pytest.mark.parametrize("missing", ["longitude", "latitude"])
def test_point_requires_both_fields(missing):
    data = {"longitude": 0.0, "latitude": 0.0}
    del data[missing]
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Point(**data)
    assert exc.value.errors()[0]["loc"] == (missing,)


def test_point_forbids_extra_fields():
    """ The named field is what populates supplementalInfo (SDI Table 24). """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Point(longitude=0.0, latitude=0.0, altitude=30)
    assert exc.value.errors()[0]["loc"] == ("altitude",)


def test_vector_accepts_typical_value():
    v = Rest_Gvp_Vector(length=5000.0, angle=45.0)
    assert v.length == 5000.0
    assert v.angle == 45.0


@pytest.mark.parametrize("angle", [0.0, 360.0])
def test_vector_accepts_angle_boundaries(angle):
    """ Table 15 range, read as inclusive; 0 and 360 are the same bearing. """
    Rest_Gvp_Vector(length=1.0, angle=angle)


def test_vector_accepts_zero_length():
    """ Zero puts a vertex at the polygon center, degenerate and permitted. """
    Rest_Gvp_Vector(length=0.0, angle=0.0)


def test_vector_rejects_negative_length():
    """ Table 15 states no range; a negative distance is uninterpretable. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Vector(length=-0.000001, angle=0.0)
    assert exc.value.errors()[0]["loc"] == ("length",)


@pytest.mark.parametrize("angle", [-0.000001, 360.000001])
def test_vector_rejects_angle_out_of_range(angle):
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Vector(length=1.0, angle=angle)
    assert exc.value.errors()[0]["loc"] == ("angle",)


@pytest.mark.parametrize("missing", ["length", "angle"])
def test_vector_requires_both_fields(missing):
    data = {"length": 1.0, "angle": 0.0}
    del data[missing]
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Vector(**data)
    assert exc.value.errors()[0]["loc"] == (missing,)


def test_vector_forbids_extra_fields():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Vector(length=1.0, angle=0.0, elevation=10)
    assert exc.value.errors()[0]["loc"] == ("elevation",)


@pytest.mark.parametrize("low,high", [
    (5925, 6425),
    (6525, 6875),
])
def test_frequency_range_accepts_appendix_a_ranges(low, high):
    """ The two inquiredFrequencyRange entries from Appendix A. """
    fr = Rest_Gvp_FrequencyRange(lowFrequency=low, highFrequency=high)
    assert fr.lowFrequency == low
    assert fr.highFrequency == high


def test_frequency_range_accepts_float_written_whole():
    """ RFC 8259 draws no int/float distinction, so 5925.0 equals 5925. """
    Rest_Gvp_FrequencyRange(lowFrequency=5925.0, highFrequency=6425.0)


def test_frequency_range_accepts_out_of_band():
    """ Band membership is response code 300, which needs the ruleset and
    location this object does not carry. """
    Rest_Gvp_FrequencyRange(lowFrequency=100, highFrequency=200)


@pytest.mark.parametrize("field", ["lowFrequency", "highFrequency"])
def test_frequency_range_rejects_fractional(field):
    """ Table 16: "The value shall be an integer." """
    data = {"lowFrequency": 5925, "highFrequency": 6425}
    data[field] = data[field] + 0.5
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(**data)
    assert exc.value.errors()[0]["loc"] == (field,)


@pytest.mark.parametrize("value", ["5925", True, None])
def test_frequency_range_rejects_non_numeric(value):
    """ Table 16 gives the data type as number. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(lowFrequency=value, highFrequency=6425)
    assert exc.value.errors()[0]["loc"] == ("lowFrequency",)


@pytest.mark.parametrize("low,high", [
    (6425, 5925),
    (6425, 6425),
])
def test_frequency_range_rejects_low_at_or_above_high(low, high):
    """ Table 16 states no ordering rule; such a range is empty or
    inverted. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(lowFrequency=low, highFrequency=high)
    assert "less than highFrequency" in str(exc.value)


@pytest.mark.parametrize("value", [0, -100])
def test_frequency_range_rejects_non_positive(value):
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(lowFrequency=value, highFrequency=6425)
    assert exc.value.errors()[0]["loc"] == ("lowFrequency",)


@pytest.mark.parametrize("missing", ["lowFrequency", "highFrequency"])
def test_frequency_range_requires_both_fields(missing):
    data = {"lowFrequency": 5925, "highFrequency": 6425}
    del data[missing]
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(**data)
    assert exc.value.errors()[0]["loc"] == (missing,)


def test_frequency_range_forbids_extra_fields():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(lowFrequency=5925, highFrequency=6425,
                                bandwidth=500)
    assert exc.value.errors()[0]["loc"] == ("bandwidth",)


# ---------------------------------------------------------------- Ellipse --

def test_ellipse_accepts_appendix_a_value():
    """ The area of intended operation from Appendix A. """
    e = Rest_Gvp_Ellipse(
        center={"longitude": -121.983601, "latitude": 37.375397},
        majorAxis=5000, minorAxis=5000, orientation=0)
    assert e.center.latitude == 37.375397
    assert e.majorAxis == 5000


@pytest.mark.parametrize("orientation", [0.0, 180.0])
def test_ellipse_accepts_orientation_boundaries(orientation):
    """ Table 10 gives the range as "from 0 to 180", read as inclusive. """
    Rest_Gvp_Ellipse(center={"longitude": 0.0, "latitude": 0.0},
                     majorAxis=1, minorAxis=1, orientation=orientation)


@pytest.mark.parametrize("orientation", [-0.000001, 180.000001])
def test_ellipse_rejects_orientation_out_of_range(orientation):
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Ellipse(center={"longitude": 0.0, "latitude": 0.0},
                         majorAxis=1, minorAxis=1, orientation=orientation)
    assert exc.value.errors()[0]["loc"] == ("orientation",)


@pytest.mark.parametrize("field", ["majorAxis", "minorAxis"])
@pytest.mark.parametrize("value", [0, -1, 5000.5])
def test_ellipse_rejects_bad_axis(field, value):
    """ Table 10: "The value is a positive integer in meters." """
    data = {"center": {"longitude": 0.0, "latitude": 0.0},
            "majorAxis": 1, "minorAxis": 1, "orientation": 0.0}
    data[field] = value
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Ellipse(**data)
    assert exc.value.errors()[0]["loc"] == (field,)


def test_ellipse_accepts_minor_axis_above_major():
    """ The same shape rotated 90 degrees, so no ordering is imposed. """
    Rest_Gvp_Ellipse(center={"longitude": 0.0, "latitude": 0.0},
                     majorAxis=100, minorAxis=500, orientation=0.0)


def test_ellipse_forbids_extra_fields():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Ellipse(center={"longitude": 0.0, "latitude": 0.0},
                         majorAxis=1, minorAxis=1, orientation=0.0, height=3)
    assert exc.value.errors()[0]["loc"] == ("height",)


# ----------------------------------------------------------------- Circle --

def test_circle_accepts_typical_value():
    c = Rest_Gvp_Circle(longitude=-121.98, latitude=37.37, radius=5000)
    assert c.radius == 5000


def test_circle_accepts_zero_radius():
    """ Zero describes an area known to a single point, and is permitted. """
    Rest_Gvp_Circle(longitude=0.0, latitude=0.0, radius=0)


@pytest.mark.parametrize("radius", [-1, 5000.5])
def test_circle_rejects_bad_radius(radius):
    """ Table 11 requires an integer; negative is uninterpretable. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Circle(longitude=0.0, latitude=0.0, radius=radius)
    assert exc.value.errors()[0]["loc"] == ("radius",)


@pytest.mark.parametrize("field,value", [
    ("longitude", 180.000001),
    ("latitude", 90.000001),
])
def test_circle_rejects_out_of_range_coordinates(field, value):
    """ Table 11 repeats the Table 14 coordinate ranges inline. """
    data = {"longitude": 0.0, "latitude": 0.0, "radius": 1}
    data[field] = value
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Circle(**data)
    assert exc.value.errors()[0]["loc"] == (field,)


# ---------------------------------------------------------- LinearPolygon --

def test_linear_polygon_accepts_minimum_vertices():
    p = Rest_Gvp_LinearPolygon(
        outerBoundary=_points(GVP_MIN_POLYGON_VERTICES))
    assert len(p.outerBoundary) == GVP_MIN_POLYGON_VERTICES


def test_linear_polygon_accepts_maximum_vertices():
    Rest_Gvp_LinearPolygon(outerBoundary=_points(GVP_MAX_POLYGON_VERTICES))


@pytest.mark.parametrize("count", [0, 1, 2, GVP_MAX_POLYGON_VERTICES + 1])
def test_linear_polygon_rejects_vertex_count_out_of_bounds(count):
    """ Table 12: "At least three and no more than 300 unique vertices". """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_LinearPolygon(outerBoundary=_points(count))
    assert exc.value.errors()[0]["loc"] == ("outerBoundary",)


def test_linear_polygon_rejects_duplicate_vertices():
    """ Table 12 counts "unique vertices", read as forbidding duplicates. """
    boundary = _points(GVP_MIN_POLYGON_VERTICES)
    boundary.append(dict(boundary[0]))
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_LinearPolygon(outerBoundary=boundary)
    assert "unique" in str(exc.value)


# ---------------------------------------------------------- RadialPolygon --

def test_radial_polygon_accepts_minimum_vertices():
    p = Rest_Gvp_RadialPolygon(
        center={"longitude": -121.98, "latitude": 37.37},
        outerBoundary=_vectors(GVP_MIN_POLYGON_VERTICES))
    assert len(p.outerBoundary) == GVP_MIN_POLYGON_VERTICES


@pytest.mark.parametrize("count", [2, GVP_MAX_POLYGON_VERTICES + 1])
def test_radial_polygon_rejects_vertex_count_out_of_bounds(count):
    """ Table 13 carries the same vertex bounds as Table 12. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_RadialPolygon(
            center={"longitude": 0.0, "latitude": 0.0},
            outerBoundary=_vectors(count))
    assert exc.value.errors()[0]["loc"] == ("outerBoundary",)


def test_radial_polygon_rejects_duplicate_vertices():
    boundary = _vectors(GVP_MIN_POLYGON_VERTICES)
    boundary.append(dict(boundary[0]))
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_RadialPolygon(
            center={"longitude": 0.0, "latitude": 0.0},
            outerBoundary=boundary)
    assert "unique" in str(exc.value)


def test_radial_polygon_requires_center():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_RadialPolygon(outerBoundary=_vectors(3))
    assert exc.value.errors()[0]["loc"] == ("center",)


# ------------------------------------------------------- CertificationId --

def test_certification_id_accepts_appendix_a_value():
    c = Rest_Gvp_CertificationId(
        rulesetId="US_47_CFR_PART_15_SUBPART_E_GVP", id="WFA-GVP-CERT-0001")
    assert c.rulesetId == "US_47_CFR_PART_15_SUBPART_E_GVP"


def test_certification_id_accepts_unlisted_ruleset():
    """ Table 8 lists acceptable rulesetId values; no enumeration is
    applied here. """
    Rest_Gvp_CertificationId(rulesetId="GB_SOME_FUTURE_RULESET", id="x")


@pytest.mark.parametrize("field", ["rulesetId", "id"])
def test_certification_id_rejects_empty_string(field):
    data = {"rulesetId": "r", "id": "i"}
    data[field] = ""
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_CertificationId(**data)
    assert exc.value.errors()[0]["loc"] == (field,)


# ------------------------------------------------------ DeviceDescriptor --

def test_device_descriptor_accepts_serial_only():
    d = Rest_Gvp_DeviceDescriptor(**_DEVICE)
    assert d.serialNumber == "GVP-AP-0001"
    assert d.deviceModel is None


def test_device_descriptor_accepts_model_only():
    """ Table 7 requires serialNumber only when deviceModel is absent. """
    data = dict(_DEVICE)
    del data["serialNumber"]
    data["deviceModel"] = "AP-Model-X"
    Rest_Gvp_DeviceDescriptor(**data)


def test_device_descriptor_accepts_both():
    data = dict(_DEVICE, deviceModel="AP-Model-X")
    Rest_Gvp_DeviceDescriptor(**data)


def test_device_descriptor_rejects_neither():
    """ The Table 7 conditional requirement, unsatisfied. """
    data = dict(_DEVICE)
    del data["serialNumber"]
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_DeviceDescriptor(**data)
    assert "serialNumber or deviceModel" in str(exc.value)


def test_device_descriptor_rejects_empty_certification_list():
    data = dict(_DEVICE, certificationId=[])
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_DeviceDescriptor(**data)
    assert exc.value.errors()[0]["loc"] == ("certificationId",)


# --------------------------------------------------------------- Location --

@pytest.mark.parametrize("field,value", [
    ("ellipse", _ELLIPSE),
    ("circle", _CIRCLE),
])
def test_location_accepts_one_geometry(field, value):
    loc = Rest_Gvp_Location(**{field: value})
    assert getattr(loc, field) is not None


def test_location_accepts_linear_polygon():
    Rest_Gvp_Location(linearPolygon={"outerBoundary": _points(3)})


def test_location_accepts_radial_polygon():
    Rest_Gvp_Location(radialPolygon={
        "center": {"longitude": 0.0, "latitude": 0.0},
        "outerBoundary": _vectors(3)})


def test_location_rejects_no_geometry():
    """ Table 9 requires one of the four when none of the others is
    given. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Location()
    assert "is required" in str(exc.value)


def test_location_rejects_two_geometries():
    """ Table 9 forbids a geometry when another is present. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Location(ellipse=_ELLIPSE, circle=_CIRCLE)
    assert "only one geometry" in str(exc.value)


def test_location_rejects_all_four_geometries():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Location(
            ellipse=_ELLIPSE, circle=_CIRCLE,
            linearPolygon={"outerBoundary": _points(3)},
            radialPolygon={"center": {"longitude": 0.0, "latitude": 0.0},
                           "outerBoundary": _vectors(3)})
    assert "only one geometry" in str(exc.value)


@pytest.mark.parametrize("value", [0, 1, 2])
def test_location_accepts_known_indoor_deployment(value):
    Rest_Gvp_Location(ellipse=_ELLIPSE, indoorDeployment=value)


@pytest.mark.parametrize("value", [-1, 3, 1.5])
def test_location_rejects_unknown_indoor_deployment(value):
    """ Table 9 maps this field to 0, 1 or 2. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Location(ellipse=_ELLIPSE, indoorDeployment=value)
    assert exc.value.errors()[0]["loc"] == ("indoorDeployment",)


def test_location_allows_unknown_field():
    """ SDI section 3.2 requires unknown fields to be ignored, and a later
    revision would most plausibly add a geometry here. """
    Rest_Gvp_Location(ellipse=_ELLIPSE, hexagon={"whatever": 1})


def test_location_geometry_field_names_match_the_models():
    """ Guard against the constant drifting from the field definitions. """
    assert set(GVP_GEOMETRY_FIELDS) <= set(Rest_Gvp_Location.__fields__)


# ------------------------------------- ExclusionZoneInquiryRequest --------

def test_request_accepts_minimum_required_fields():
    r = Rest_Gvp_ExclusionZoneInquiryRequest(**_request())
    assert r.inquiredFrequencyRange is None
    assert r.desiredPsd is None
    assert r.desiredPrecision is None


@pytest.mark.parametrize("missing", ["requestId", "deviceDescriptor",
                                     "areaOfIntendedOperation"])
def test_request_requires_mandatory_fields(missing):
    data = _request()
    del data[missing]
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_ExclusionZoneInquiryRequest(**data)
    assert exc.value.errors()[0]["loc"] == (missing,)


def test_request_accepts_fractional_psd():
    """ Table 6 says PSD levels should be whole numbers in 1 dB steps.
    Section 1.2.1 makes should a recommendation, so it is not enforced. """
    Rest_Gvp_ExclusionZoneInquiryRequest(**_request(desiredPsd=[11.5]))


def test_request_accepts_unlisted_precision():
    """ Table 6 makes desiredPrecision a recommendation, so any value is
    accepted. """
    Rest_Gvp_ExclusionZoneInquiryRequest(**_request(desiredPrecision="ultra"))


def test_request_rejects_non_numeric_psd():
    """ Table 6 gives desiredPsd as an array of number. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_ExclusionZoneInquiryRequest(**_request(desiredPsd=["11"]))
    assert exc.value.errors()[0]["loc"][0] == "desiredPsd"


def test_request_allows_unknown_field():
    """ SDI section 3.2 requires unknown fields to be ignored. """
    Rest_Gvp_ExclusionZoneInquiryRequest(**_request(futureOption=True))


# --------------------------------------------------------------- ReqMsg --

def test_req_msg_parses_appendix_a(load_fixture):
    """ The spec's own example request, end to end. desiredPsd is [11, 5]
    in the JSON while the prose says 11, 8 and 5; the assertion guards the
    fixture. """
    msg = Rest_Gvp_ReqMsg(**load_fixture("appendix_a_request.json"))
    assert msg.version == "1.0"
    assert len(msg.exclusionZoneInquiryRequests) == 1

    req = msg.exclusionZoneInquiryRequests[0]
    assert req.requestId == "wfa-gvp-request-0001"
    assert req.desiredPsd == [11, 5]
    assert req.desiredPrecision == "low"

    ellipse = req.areaOfIntendedOperation.ellipse
    assert ellipse.center.longitude == -121.983601
    assert ellipse.center.latitude == 37.375397
    assert ellipse.majorAxis == 5000

    assert [(f.lowFrequency, f.highFrequency)
            for f in req.inquiredFrequencyRange] == [(5925, 6425),
                                                     (6525, 6875)]


def test_req_msg_rejects_duplicate_request_ids():
    """ Table 6 requires requestId unique within the message. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_ReqMsg(version="1.0", exclusionZoneInquiryRequests=[
            _request(requestId="dup"), _request(requestId="dup")])
    assert "unique within the message" in str(exc.value)


def test_req_msg_accepts_multiple_distinct_requests():
    """ Section 3.2 permits aggregating one or more requests. """
    msg = Rest_Gvp_ReqMsg(version="1.0", exclusionZoneInquiryRequests=[
        _request(requestId="a"), _request(requestId="b")])
    assert len(msg.exclusionZoneInquiryRequests) == 2


def test_req_msg_rejects_empty_request_array():
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_ReqMsg(version="1.0", exclusionZoneInquiryRequests=[])
    assert exc.value.errors()[0]["loc"] == ("exclusionZoneInquiryRequests",)


def test_req_msg_accepts_any_version_string():
    """ Section 4.1 gives the Protocol Version as 1.0. Rejecting another
    value belongs where a VERSION_NOT_SUPPORTED code can be produced. """
    Rest_Gvp_ReqMsg(version="9.9",
                    exclusionZoneInquiryRequests=[_request()])


def test_req_msg_error_location_reaches_nested_fields():
    """ The reported path is what populates supplementalInfo (SDI Table 24),
    so it must survive four levels of nesting. """
    bad = _request()
    bad["areaOfIntendedOperation"]["ellipse"]["center"]["latitude"] = 91.0
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_ReqMsg(version="1.0", exclusionZoneInquiryRequests=[bad])
    assert exc.value.errors()[0]["loc"] == (
        "exclusionZoneInquiryRequests", 0, "areaOfIntendedOperation",
        "ellipse", "center", "latitude")
