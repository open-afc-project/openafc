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

router = fastapi.APIRouter()


def _gvp_stub_resp_msg(req_msg):
    """ Build a success response carrying fixed geometry.

    NOTE: Temporary. afc-engine does not compute exclusion zones yet, so
    every inquiry gets the same zone. Replace the zone list with the engine
    result; the surrounding message shape is final.
    """
    expire_time = \
        (datetime.datetime.now(datetime.timezone.utc) +
         datetime.timedelta(hours=24)).strftime(GVP_EXPIRE_TIME_FORMAT)
    responses = []
    for request in req_msg.exclusionZoneInquiryRequests:
        default_range = Rest_Gvp_FrequencyRange(
            lowFrequency=6182, highFrequency=6212)
        frequency_range = \
            (request.inquiredFrequencyRange or [default_range])[0]
        psd_level = (request.desiredPsd or [11])[0]
        zone = Rest_Gvp_ExclusionZoneInfo(
            exclusionZoneFrequencyRange=frequency_range,
            psdLevel=psd_level,
            circles=[[37.38042, -121.96694, 29269]])
        responses.append(
            Rest_Gvp_ExclusionZoneInquiryResponse(
                requestId=request.requestId,
                # Table 18 ties this to the request on SUCCESS
                rulesetId=(request.deviceDescriptor
                           .certificationId[0].rulesetId),
                exclusionZoneInfo=[zone],
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
