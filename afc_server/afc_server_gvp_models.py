""" Request and response models for the Geofencing System to GVP Device
Interface (SDI). """

from typing import Union

import pydantic


class Rest_Gvp_Point(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Point object (SDI Table 14). """
    longitude: float = pydantic.Field(ge=-180.0, le=180.0)  # R, degrees east
    latitude: float = pydantic.Field(ge=-90.0, le=90.0)  # R, degrees north


class Rest_Gvp_Vector(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Vector object (SDI Table 15). """
    length: float = pydantic.Field(ge=0.0)  # R, meters
    angle: float = pydantic.Field(ge=0.0, le=360.0)  # R, degrees from true N


class Rest_Gvp_FrequencyRange(pydantic.BaseModel,
                              extra=pydantic.Extra.forbid):
    """ FrequencyRange object (SDI Table 16). """
    lowFrequency: Union[pydantic.StrictInt, pydantic.StrictFloat]  # R, MHz
    highFrequency: Union[pydantic.StrictInt, pydantic.StrictFloat]  # R, MHz

    @pydantic.validator("lowFrequency", "highFrequency")
    def positive_whole_number(cls, v):
        """ Table 16: "The value shall be an integer."

        Both checks live here because pydantic refuses Field(gt=...) on a
        Union type, where it cannot enforce the constraint.
        """
        if v <= 0:
            raise ValueError("must be a positive frequency in MHz")
        if float(v) != int(v):
            raise ValueError("must be a whole number of MHz")
        return v

    @pydantic.root_validator(skip_on_failure=True)
    def low_below_high(cls, values):
        """ A range with lowFrequency at or above highFrequency is empty or
        inverted, and yields a slice of zero or negative width. """
        low = values.get("lowFrequency")
        high = values.get("highFrequency")
        if low is not None and high is not None and low >= high:
            raise ValueError("lowFrequency must be less than highFrequency")
        return values
