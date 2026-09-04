""" Unit tests for the GVP SDI request and response models. """

import pydantic
import pytest

from afc_server_gvp_models import Rest_Gvp_Point


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
