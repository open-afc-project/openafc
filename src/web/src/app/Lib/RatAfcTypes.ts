/**
 * Type definitions for AFC API
 */

import { AFCConfigFile } from "./RatApiTypes";

/**
 * Main request inquiry type. Passed to server inside of an array
 */
export interface AvailableSpectrumInquiryRequest {
    requestId: string,
    deviceDescriptor: DeviceDescriptor,
    location: Location,
    inquiredFrequencyRange?: FrequencyRange[],
    inquiredChannels?: Channels[],
    minDesiredPower?: number,
    vendorExtensions: VendorExtension[]
}

export interface CertificationId{
    nra:string,
    id:string
}


export interface DeviceDescriptor {
    serialNumber: string,
    certificationId: CertificationId[],
    rulesetIds: ["US_47_CFR_PART_15_SUBPART_E"]
}

export interface Elevation {
    height?: number,
    verticalUncertainty?: number,
    heightType?: string
}

export interface Location {
    ellipse?: Ellipse,
    linearPolygon?: LinearPolygon,
    radialPolygon?: RadialPolygon,
    elevation?: Elevation,
    indoorDeployment: DeploymentEnum
}


export interface Ellipse {
    center: Point,
    majorAxis: number,
    minorAxis: number,
    orientation: number
}

export interface LinearPolygon {
    outerBoundary: Point[]
}

export interface RadialPolygon {
    center: Point,
    outerBoundary: Vector[]
}

export interface Point {
    longitude: number,
    latitude: number
}

export interface Vector {
    length: number,
    angle: number
}

export interface FrequencyRange {
    lowFrequency: number,
    highFrequency: number
}

export interface Channels {
    globalOperatingClass: number, // TODO: check to see if we only want to allow one value
    channelCfi?: number[]
}

export interface AvailableSpectrumInquiryResponse {
    requestId: number,
    availableFrequencyInfo?: AvailableSpectrumInfo[],
    availableChannelInfo?: AvailableChannelInfo[],
    availabilityExpireTime?: string,
    response: InquiryResponse
}

export interface AvailableSpectrumInfo {
    frequencyRange: FrequencyRange,
    maxPSD: number
}

export interface AvailableChannelInfo {
    globalOperatingClass: number,
    channelCfi: number[],
    maxEirp: number[]
}

export interface InquiryResponse {
    responseCode: number,
    shortDescription: string//,
    //supplementalInfo: SupplementalInfo
}

export interface VendorExtension {
    extensionID: "RAT v1.1AFC Config"
    parameters: AFCConfigFile
}

export enum DeploymentEnum {
    indoorDeployment = 1,
    outdoorDeployment = 2,
    unkown = 0
}


export enum OperatingClassIncludeType { None = "None", Some = "Some", All = "All" };

export interface OperatingClass {
    num: number,
    include: OperatingClassIncludeType,
    channels?: number[]
}
