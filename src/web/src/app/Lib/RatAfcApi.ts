import { AvailableSpectrumInquiryRequest, AvailableSpectrumInquiryResponse, AvailableSpectrumInquiryResponseMessage, DeploymentEnum, VendorExtension } from "./RatAfcTypes";
import { RatResponse, success, error } from "./RatApiTypes";
import { guiConfig, getDefaultAfcConf } from "./RatApi";


/**
 * RatAfcApi.ts
 * API functions for RAT AFC
 */

/**
 * Call RAT AFC resource
 */
export const spectrumInquiryRequest = (request: AvailableSpectrumInquiryRequest): Promise<RatResponse<AvailableSpectrumInquiryResponseMessage>> =>
    fetch(guiConfig.rat_afc + "?debug=True&gui=True", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ version: "1.4", availableSpectrumInquiryRequests: [request] })
    })
        .then(async resp => {
            if (resp.status == 200) {
                const data = (await resp.json()) as AvailableSpectrumInquiryResponseMessage;
                // Get the first response until API can handle multiple requests
                const response = data.availableSpectrumInquiryResponses[0];
                if (response.response.responseCode == 0) {
                    return success(data);
                } else {
                    return error(response.response.shortDescription, response.response.responseCode, response);
                }
            } else {
                return error(resp.statusText, resp.status, resp);
            }
        })
        .catch(e => {
            return error("encountered an error when running request", undefined, e);
        })



export const spectrumInquiryRequestByString = (version: string, requestAsJsonString: string): Promise<RatResponse<AvailableSpectrumInquiryResponseMessage>> =>
    fetch(guiConfig.rat_afc + "?debug=True&gui=True", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ version: version, availableSpectrumInquiryRequests: [JSON.parse(requestAsJsonString)] })
    })
        .then(async resp => {
            if (resp.status == 200) {
                const data = (await resp.json()) as AvailableSpectrumInquiryResponseMessage;
                // Get the first response until API can handle multiple requests
                const response = data.availableSpectrumInquiryResponses[0];
                if (response.response.responseCode == 0) {
                    return success(data);
                } else {
                    return error(response.response.shortDescription, response.response.responseCode, response);
                }
            } else {
                return error(resp.statusText, resp.status, resp);
            }
        })
        .catch(e => {
            return error("encountered an error when running request", undefined, e);
        })


/**
* name
*/
export function downloadMapData(kml_filename: any, method: string): Promise<Response> {
    var url = guiConfig.afc_kml.replace('p_kml_file', kml_filename)
    return fetch(url, {
        method: method,
        headers: {
            "Content-Type": "application/json",
            "Content-Encoding": "gzip"
        }
    })
}

export const sampleRequestObject: AvailableSpectrumInquiryRequest = {
    requestId: "0",
    deviceDescriptor: {
        serialNumber: "sample-ap",
        certificationId: [
            {
                rulesetId: "US_47_CFR_PART_15_SUBPART_E",
                id: "1234567890"
            }
        ]
    },
    location: {
        ellipse: {
            center: {
                latitude: 41,
                longitude: -74
            },
            majorAxis: 200,
            minorAxis: 100,
            orientation: 90
        },
        elevation: {
            height: 15,
            verticalUncertainty: 5,
            heightType: "AGL"
        },

        indoorDeployment: DeploymentEnum.indoorDeployment
    },
    minDesiredPower: 15,
    vendorExtensions: [{
        extensionId: "RAT v1.3 AFC Config",
        parameters: getDefaultAfcConf("US")
    }],
    inquiredChannels: [
        {
            globalOperatingClass: 133,
        },
        {
            globalOperatingClass: 134,
            channelCfi: [15, 47, 79]
        }
    ],
    inquiredFrequencyRange: [
        { lowFrequency: 5925000000, highFrequency: 6425000000 }
    ]
}
