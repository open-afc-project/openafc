/**
 * Type definitions for the Geofencing System to GVP Device Interface (SDI)
 *
 * Import it namespaced when a module also needs AFC types:
 *
 *   import * as GvpTypes from './GvpTypes';
 */

/** Protocol Version of SDI section 4.1. */
export const GVP_PROTOCOL_VERSION = '1.0';

/** rulesetId served by this deployment (SDI Table 8). */
export const GVP_RULESET_ID = 'US_47_CFR_PART_15_SUBPART_E_GVP';

/** desiredPrecision values of SDI Table 6. */
export const GVP_PRECISION_VALUES = ['low', 'medium', 'high'];

// ----------------------------------------------------------------- request --

/** Point object (SDI Table 14). Degrees, WGS 84. */
export interface Point {
  longitude: number;
  latitude: number;
}

/** Vector object (SDI Table 15). */
export interface Vector {
  /** Distance from the Point, in meters. */
  length: number;
  /** Bearing in degrees, clockwise from true north. */
  angle: number;
}

/** FrequencyRange object (SDI Table 16). Whole MHz, low strictly below high. */
export interface FrequencyRange {
  lowFrequency: number;
  highFrequency: number;
}

/** Ellipse object (SDI Table 10). */
export interface Ellipse {
  center: Point;
  /** Major semi axis, in meters. */
  majorAxis: number;
  /** Minor semi axis, in meters. */
  minorAxis: number;
  /** Orientation of majorAxis in degrees, clockwise from true north, 0 to 180. */
  orientation: number;
}

/** Circle object (SDI Table 11). Degrees, WGS 84. */
export interface Circle {
  longitude: number;
  latitude: number;
  /** Radius in meters. */
  radius: number;
}

/** LinearPolygon object (SDI Table 12). 3 to 300 unique vertices. */
export interface LinearPolygon {
  outerBoundary: Point[];
}

/** RadialPolygon object (SDI Table 13). 3 to 300 unique vectors. */
export interface RadialPolygon {
  center: Point;
  outerBoundary: Vector[];
}

/** CertificationId object (SDI Table 8). */
export interface CertificationId {
  /** Identifier of the regulatory rules. */
  rulesetId: string;
  /** Certification ID of the GVP Access Point. */
  id: string;
}

/** DeviceDescriptor object (SDI Table 7). Requires serialNumber or deviceModel. */
export interface DeviceDescriptor {
  certificationId: CertificationId[];
  serialNumber?: string;
  deviceModel?: string;
}

/** Location object (SDI Table 9), the area of intended operation. Exactly one geometry. */
export interface Location {
  ellipse?: Ellipse;
  circle?: Circle;
  linearPolygon?: LinearPolygon;
  radialPolygon?: RadialPolygon;
  indoorDeployment?: DeploymentType;
}

/** ExclusionZoneInquiryRequest object (SDI Table 6). */
export interface ExclusionZoneInquiryRequest {
  /** Unique within the message. */
  requestId: string;
  deviceDescriptor: DeviceDescriptor;
  areaOfIntendedOperation: Location;
  /** Frequency ranges of interest. Absent means all GVP-applicable bands. */
  inquiredFrequencyRange?: FrequencyRange[];
  /** Power spectral density levels in dBm/MHz. */
  desiredPsd?: number[];
  /** Requested precision: low, medium or high. */
  desiredPrecision?: string;
  vendorExtensions?: VendorExtension[];
}

/** ExclusionZoneInquiryRequestMessage object (SDI Table 5). */
export interface ExclusionZoneInquiryRequestMessage {
  version: string;
  exclusionZoneInquiryRequests: ExclusionZoneInquiryRequest[];
  vendorExtensions?: VendorExtension[];
}

// ---------------------------------------------------------------- response --

/** A [latitude, longitude] pair (SDI Table 19). */
export type LatLonPair = [number, number];

/** A [center latitude, center longitude, radius in meters] triple (SDI Table 19). */
export type CircleTriple = [number, number, number];

/** ExclusionZoneInfo object (SDI Table 19). At least one of polygons or circles. */
export interface ExclusionZoneInfo {
  /** Non-unique label, for debugging only. */
  exclusionZoneId?: string;
  /** Range over which operation is not permitted inside the zone. */
  exclusionZoneFrequencyRange: FrequencyRange;
  /** PSD in dBm/MHz. */
  psdLevel: number;
  /** Single circle covering the zone. */
  enclosingCircle?: Circle;
  /** Polygons as arrays of corner points. */
  polygons?: LatLonPair[][];
  circles?: CircleTriple[];
}

/** SupplementalInfo object (SDI Table 24). */
export interface SupplementalInfo {
  missingParams?: string[];
  invalidParams?: string[];
  unexpectedParams?: string[];
}

/** Response object (SDI Table 22). */
export interface InquiryResponse {
  responseCode: ResponseCode | number;
  shortDescription?: string;
  supplementalInfo?: SupplementalInfo;
}

/** ExclusionZoneInquiryResponse object (SDI Table 18). Zone data and expiry only on Success. */
export interface ExclusionZoneInquiryResponse {
  /** Echoes the requestId of the corresponding inquiry. */
  requestId: string;
  rulesetId: string;
  /** Zones overlapping the area of intended operation. */
  exclusionZoneInfo?: ExclusionZoneInfo[];
  /** UTC expiry as YYYY-MM-DDThh:mm:ssZ. */
  availabilityExpireTime?: string;
  response: InquiryResponse;
  vendorExtensions?: VendorExtension[];
}

/** ExclusionZoneInquiryResponseMessage object (SDI Table 17). */
export interface ExclusionZoneInquiryResponseMessage {
  /** Matches the version of the request. */
  version: string;
  exclusionZoneInquiryResponses: ExclusionZoneInquiryResponse[];
  vendorExtensions?: VendorExtension[];
}

// ------------------------------------------------------------------ shared --

/** VendorExtension object. */
export interface VendorExtension {
  extensionId: string;
  parameters: any;
}

/** indoorDeployment values of SDI Table 9. */
export enum DeploymentType {
  Unknown = 0,
  Indoor = 1,
  Outdoor = 2,
}

/** Response codes of SDI Table 23. */
export enum ResponseCode {
  GeneralFailure = -1,
  Success = 0,
  MissingParam = 102,
  InvalidValue = 103,
  UnexpectedParam = 106,
}
