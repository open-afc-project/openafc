""" GVP Exclusion Zone Inquiry request handling. """

import datetime
import fastapi
import pydantic
from typing import Any, Dict

from afc_server_gvp_models import (GVP_EXPIRE_TIME_FORMAT,
                                   GVP_SUCCESS_RESPONSE_CODE,
                                   Rest_Gvp_ExclusionZoneInfo,
                                   Rest_Gvp_ExclusionZoneInquiryResponse,
                                   Rest_Gvp_FrequencyRange, Rest_Gvp_ReqMsg,
                                   Rest_Gvp_RespMsg,
                                   resp_msg_from_validation_error)
from afc_server_models import Rest_Response

__all__ = ["router"]

# rulesetId this deployment serves (SDI Table 8).
GVP_RULESET_ID = "US_47_CFR_PART_15_SUBPART_E_GVP"

# Frequency range and PSD level assumed when an inquiry omits them, which
# SDI Table 6 leaves to the Geofencing System.
GVP_DEFAULT_FREQUENCY_RANGE = (6182, 6212)
GVP_DEFAULT_PSD_LEVEL = 11

# Stub geometry: a zone centered near the Appendix A area of intended
# operation, growing with PSD level because more power reaches further.
GVP_STUB_CENTER = (37.38042, -121.96694)
GVP_STUB_BASE_RADIUS_M = 20000
GVP_STUB_RADIUS_PER_DB_M = 1000

router = fastapi.APIRouter()


def _gvp_stub_zones(request):
    """ Build one zone per inquired frequency range and PSD level.

    NOTE: Temporary. The cross product is real; the geometry is invented.
    Replace the circles with the afc-engine result.
    """
    ranges = request.inquiredFrequencyRange or [
        Rest_Gvp_FrequencyRange(
            lowFrequency=GVP_DEFAULT_FREQUENCY_RANGE[0],
            highFrequency=GVP_DEFAULT_FREQUENCY_RANGE[1])]
    psd_levels = request.desiredPsd or [GVP_DEFAULT_PSD_LEVEL]
    latitude, longitude = GVP_STUB_CENTER
    zones = []
    for range_index, frequency_range in enumerate(ranges):
        for psd_level in psd_levels:
            radius = int(GVP_STUB_BASE_RADIUS_M +
                         psd_level * GVP_STUB_RADIUS_PER_DB_M)
            zones.append(
                Rest_Gvp_ExclusionZoneInfo(
                    exclusionZoneId="stub-%d-%g" % (range_index, psd_level),
                    exclusionZoneFrequencyRange=frequency_range,
                    psdLevel=psd_level,
                    circles=[[latitude,
                              longitude + 0.05 * range_index,
                              radius]]))
    return zones


def _gvp_stub_resp_msg(req_msg):
    """ Build a success response for every inquiry in the message. """
    expire_time = \
        (datetime.datetime.now(datetime.timezone.utc) +
         datetime.timedelta(hours=24)).strftime(GVP_EXPIRE_TIME_FORMAT)
    responses = []
    for request in req_msg.exclusionZoneInquiryRequests:
        responses.append(
            Rest_Gvp_ExclusionZoneInquiryResponse(
                requestId=request.requestId,
                # Table 18 ties this to the request on SUCCESS
                rulesetId=(request.deviceDescriptor
                           .certificationId[0].rulesetId),
                exclusionZoneInfo=_gvp_stub_zones(request),
                availabilityExpireTime=expire_time,
                response=Rest_Response(
                    responseCode=GVP_SUCCESS_RESPONSE_CODE,
                    shortDescription="Success")))
    return Rest_Gvp_RespMsg(
        version=req_msg.version,
        exclusionZoneInquiryResponses=responses)


@router.post("/fbrat/ap-gvp/exclusionZoneInquiry",
             summary="Process GVP Exclusion Zone Inquiry from outside the "
                     "cluster")
async def exclusion_zone_inquiry(request: fastapi.Request) -> Dict[str, Any]:
    """ Process GVP Exclusion Zone Inquiry Request message

    The payload is read untyped so that a malformed request is answered in
    the form SDI Table 24 defines, instead of the error FastAPI produces
    from a model annotated parameter.
    """
    try:
        body = await request.json()
    except ValueError:
        body = None
    if not isinstance(body, dict) or \
            not isinstance(body.get("exclusionZoneInquiryRequests"), list):
        # SDI section 3.4.1 asks for 400 when the payload is neither an
        # ExclusionZoneInquiryRequestMessage nor a StandaloneVendorExtension
        raise fastapi.HTTPException(
            status_code=fastapi.status.HTTP_400_BAD_REQUEST,
            detail="payload is not an ExclusionZoneInquiryRequestMessage")
    try:
        req_msg = Rest_Gvp_ReqMsg(**body)
    except pydantic.ValidationError as exc:
        resp_msg = resp_msg_from_validation_error(exc, body,
                                                  GVP_RULESET_ID)
    else:
        resp_msg = _gvp_stub_resp_msg(req_msg)
    # SDI section 3.2: an optional field that is not used shall be omitted
    return resp_msg.dict(exclude_none=True)
