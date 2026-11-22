import {
  ExclusionZoneInquiryRequest,
  ExclusionZoneInquiryRequestMessage,
  ExclusionZoneInquiryResponseMessage,
  GVP_PROTOCOL_VERSION,
  ResponseCode,
} from './GvpTypes';
import { RatResponse, success, error } from './RatApiTypes';
import { getCSRF } from './RatApi';

/**
 * GvpApi.ts
 * API functions for the Geofencing System to GVP Device Interface (SDI)
 */

const GVP_URL = '/fbrat/ap-gvp/exclusionZoneInquiry';

export const exclusionZoneInquiry = async (
  request: ExclusionZoneInquiryRequest,
): Promise<RatResponse<ExclusionZoneInquiryResponseMessage>> => {
  const message: ExclusionZoneInquiryRequestMessage = {
    version: GVP_PROTOCOL_VERSION,
    exclusionZoneInquiryRequests: [request],
  };
  return exclusionZoneInquiryByString(JSON.stringify(message));
};

/** Sends the text unchanged, so the server reports what is wrong with a malformed message. */
export const exclusionZoneInquiryByString = async (
  message: string,
): Promise<RatResponse<ExclusionZoneInquiryResponseMessage>> => {
  const csrf_token = getCSRF();

  const resp = await fetch(GVP_URL, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'X-CSRF-Token': csrf_token,
    },
    body: message,
  });

  // The server returns 400 when the payload is not an inquiry message (SDI section 3.4.1).
  if (resp.status !== 200) {
    return error(resp.statusText, resp.status, resp);
  }

  const data = (await resp.json()) as ExclusionZoneInquiryResponseMessage;
  // The page shows one inquiry per message.
  const first = data.exclusionZoneInquiryResponses[0];
  if (first === undefined) {
    return error('Exclusion zone inquiry returned no responses', undefined, data);
  }

  if (first.response.responseCode !== ResponseCode.Success) {
    const description = first.response.shortDescription || 'Exclusion zone inquiry failed';
    return error(description, first.response.responseCode, first);
  }

  return success(data);
};
