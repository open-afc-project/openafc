""" Request and response models for the Geofencing System to GVP Device
Interface (SDI). """

import pydantic


class Rest_Gvp_Point(pydantic.BaseModel, extra=pydantic.Extra.forbid):
    """ Point object (SDI Table 14). """
    longitude: float = pydantic.Field(ge=-180.0, le=180.0)  # R, degrees east
    latitude: float = pydantic.Field(ge=-90.0, le=90.0)  # R, degrees north
