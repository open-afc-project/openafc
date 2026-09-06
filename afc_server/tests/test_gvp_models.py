""" Unit tests for the GVP SDI request and response models. """

import pydantic
import pytest

from afc_server_gvp_models import (Rest_Gvp_FrequencyRange, Rest_Gvp_Point,
                                   Rest_Gvp_Vector)


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
    """ Table 14 states the ranges as "from -180 to +180" and "from -90 to
    +90", read as inclusive. """
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
    """ Extra.forbid names the offending field, which is what populates
    supplementalInfo.unexpectedParams (SDI Table 24). """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_Point(longitude=0.0, latitude=0.0, altitude=30)
    assert exc.value.errors()[0]["loc"] == ("altitude",)


def test_vector_accepts_typical_value():
    v = Rest_Gvp_Vector(length=5000.0, angle=45.0)
    assert v.length == 5000.0
    assert v.angle == 45.0


@pytest.mark.parametrize("angle", [0.0, 360.0])
def test_vector_accepts_angle_boundaries(angle):
    """ Table 15 gives the range as "from 0 to 360", read as inclusive. 0 and
    360 name the same bearing and the spec permits both. """
    Rest_Gvp_Vector(length=1.0, angle=angle)


def test_vector_accepts_zero_length():
    """ Table 15 states no range for length. A zero length vector puts a
    vertex at the polygon center, which is degenerate and not forbidden, so
    it is accepted. """
    Rest_Gvp_Vector(length=0.0, angle=0.0)


def test_vector_rejects_negative_length():
    """ Table 15 states no range for length, so this bound is our decision: a
    negative distance has no interpretation under the field's own definition
    ("a distance in meters from a specified Point object"), and would place a
    vertex opposite the stated bearing. """
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
    """ Table 16 gives the JSON data type as number, and RFC 8259 draws no
    int/float distinction, so 5925.0 is the same value as 5925. """
    Rest_Gvp_FrequencyRange(lowFrequency=5925.0, highFrequency=6425.0)


def test_frequency_range_accepts_out_of_band():
    """ Frequencies outside the GVP bands of Table 2 are accepted here.
    Whether a range lies inside the applicable ruleset's bands is response
    code 300, which needs the ruleset and location this object lacks. """
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
    """ Table 16 gives the data type as number. StrictInt and StrictFloat
    keep strings and booleans out; a plain int annotation would coerce
    them. """
    with pytest.raises(pydantic.ValidationError) as exc:
        Rest_Gvp_FrequencyRange(lowFrequency=value, highFrequency=6425)
    assert exc.value.errors()[0]["loc"] == ("lowFrequency",)


@pytest.mark.parametrize("low,high", [
    (6425, 5925),
    (6425, 6425),
])
def test_frequency_range_rejects_low_at_or_above_high(low, high):
    """ Table 16 states no ordering rule. This bound is our decision, from
    the fields' definitions as the lowest and highest frequency of a range:
    such a range is empty or inverted. """
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
