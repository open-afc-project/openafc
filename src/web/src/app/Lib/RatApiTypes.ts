/**
 * RatApiTypes.ts: type definitions used by RatApi.ts
 * author: Sam Smucny
 */

/**
 * Interface of GUI configuration
 */
export interface GuiConfig {
    paws_url: string,
    uls_url: string,
    antenna_url: string,
    history_url: string,
    afcconfig_defaults: string,
    lidar_bounds: string,
    ras_bounds: string,
    google_apikey: string,
    rat_api_analysis: string,
    uls_convert_url: string,
    uls_daily_url: string,
    login_url: string,
    user_url: string,
    admin_url: string,
    ap_admin_url: string,
    rat_afc: string,
    version: string
}

export interface AFCConfigFile {
    freqBands: FreqRange[]
    version: string,
    maxEIRP: number,
    minEIRP: number,
    ulsDatabase: string,
    buildingPenetrationLoss: PenetrationLossModel,
    polarizationMismatchLoss: PolarizationLossModel,
    receiverFeederLoss: FSReceiverFeederLoss,
    bodyLoss: BodyLossModel,
    threshold: number,
    maxLinkDistance: number,
    ulsDefaultAntennaType: DefaultAntennaType,
    antennaPattern: UserAntennaPattern,
    scanPointBelowGroundMethod: ScanPointBelowGroundMethod
    propagationModel: PropagationModel,
    APUncertainty: APUncertainty,
    propagationEnv: "NLCD Point" | "Population Density Map" | "Urban" | "Suburban" | "Rural",
    ITMParameters: ITMParameters,
    fsReceiverNoise: FSReceiverNoise,
    rlanITMTxClutterMethod?: 'FORCE_TRUE' | 'FORCE_FALSE' | 'BLDG_DATA',
    clutterAtFS: boolean,
    regionStr?: string,
    rasDatabase?: string
}

export type FreqRange = {
    name: string,
    startFreqMHz: number,
    stopFreqMHz: number
}

export type FSReceiverFeederLoss = {
    UNII5: Number,
    UNII7: Number,
    other: Number
}

export type FSReceiverNoise = FSReceiverFeederLoss;

export type PenetrationLossModel = P2109 | FixedValue;

export interface P2109 {
    kind: "ITU-R Rec. P.2109",
    buildingType: "Traditional" | "Efficient",
    confidence: number
}

export interface FixedValue {
    kind: "Fixed Value"
    value: number
}

export type PolarizationLossModel = EU | FixedValue;

export interface EU {
    kind: "EU"
}

export type BodyLossModel = EU | {
    kind: "Fixed Value",
    valueIndoor: number,
    valueOutdoor: number
};

export type APUncertainty = {
    horizontal: number,
    height: number
}

export type ITMParameters = {
    polarization: 'Vertical' | 'Horizontal',
    ground: GroundType,
    dielectricConst: Number,
    conductivity: Number,
    minSpacing: Number,
    maxPoints: Number
}

export type GroundType = 'Average Ground' | 'Poor Ground' | 'Good Ground' | 'Fresh Water' | 'Sea Water';

export type AntennaPatternState = {
    defaultAntennaPattern: DefaultAntennaType,
    userUpload?: UserUpload
}

export type DefaultAntennaType = 'F.1245' | 'F.699' | 'WINNF-AIP-07'

export type UserAntennaPattern = {
    kind: string,
    value: string
}

export interface UserUpload {
    kind: "User Upload" | 'None',
    value: string
}

export type PropagationModel = FSPL | Win2ItmDb | Win2ItmClutter | RayTrace | FCC6GHz | CustomPropagation;

export interface FSPL { kind: "FSPL" }
export interface Win2ItmDb {
    kind: "ITM with building data",
    win2ProbLosThreshold: number,
    win2Confidence: number,
    itmConfidence: number,
    p2108Confidence: number,
    buildingSource: "B-Design3D" | "LiDAR"
}
export interface Win2ItmClutter {
    kind: "ITM with no building data",
    win2ProbLosThreshold: number,
    win2Confidence: number,
    itmConfidence: number,
    p2108Confidence: number,
    terrainSource: "SRTM (90m)" | "3DEP (30m)"
}
export interface RayTrace {
    kind: "Ray Tracing"
}
export interface FCC6GHz {
    kind: "FCC 6GHz Report & Order",
    win2Confidence: number,
    itmConfidence: number,
    p2108Confidence: number,
    buildingSource: "B-Design3D" | "LiDAR" | "None",
    terrainSource: "SRTM (90m)" | "3DEP (30m)"
}

export interface CustomPropagation {
    kind: "Custom",
    winner2LOSOption: 'UNKNOWN' | 'FORCE_LOS' | 'FORCE_NLOS' | 'BLDG_DATA_REQ_TX',
    win2Confidence: number,
    itmConfidence: number,
    p2108Confidence: number,
    buildingSource: "B-Design3D" | "LiDAR" | "None",
    terrainSource: "SRTM (90m)" | "3DEP (30m)"
    rlanITMTxClutterMethod?: 'FORCE_TRUE' | 'FORCE_FALSE' | 'BLDG_DATA',
}


export type ScanPointBelowGroundMethod = "discard" | "truncate"


/**
 * PAWS request format is specified in 
 * [PAWS Profile AFC-6GHZ-DEMO-1.1](https://rkfeng.sharepoint.com/:w:/r/sites/FBRLANAFCTool/_layouts/15/Doc.aspx?sourcedoc=%7B0977F769-2B73-495C-8CEC-FDA231393192%7D&file=PAWS%20Profile%20AFC-6GHZ-DEMO-1.1.docx&action=default&mobileredirect=true)
 */
export interface PAWSRequest {
    type: "AVAIL_SPECTRUM_REQ",
    version: "1.0",
    deviceDesc: DeviceDescriptor,
    location: GeoLocation,
    owner?: DeviceOwner,
    antenna: AntennaCharacteristics,
    capabilities: DeviceCapabilities,
    useAdjacentChannel?: boolean
}

/**
 * PAWS response format is specified in 
 * [PAWS Profile AFC-6GHZ-DEMO-1.1](https://rkfeng.sharepoint.com/:w:/r/sites/FBRLANAFCTool/_layouts/15/Doc.aspx?sourcedoc=%7B0977F769-2B73-495C-8CEC-FDA231393192%7D&file=PAWS%20Profile%20AFC-6GHZ-DEMO-1.1.docx&action=default&mobileredirect=true)
 */
export interface PAWSResponse {
    type: "AVAIL_SPECTRUM_RESP",
    version: "1.0",
    timestamp: string,
    deviceDesc: DeviceDescriptor,
    spectrumSpecs: SpectrumSpec[],
    databaseChange?: DbUpdateSpec
}

interface DeviceDescriptor {
    serialNumber: string,
    manufacturerId?: string,
    modelId?: string,
    rulesetIds: ["AFC-6GHZ-DEMO-1.1"]
}

interface GeoLocation {
    point: Ellipse
}

export type Ellipse = {
    center: {
        latitude: number,
        longitude: number
    },
    semiMajorAxis: number,
    semiMinorAxis: number,
    orientation: number // default to 0 if not present
}

interface DeviceOwner { }

interface AntennaCharacteristics {
    height: number,
    heightType: HeightType,
    heightUncertainty: number
}

export enum HeightType {
    AGL = "AGL",
    AMSL = "AMSL"
}

export interface DeviceCapabilities {
    indoorOutdoor: IndoorOutdoorType
}

export enum IndoorOutdoorType {
    INDOOR = "Indoor",
    OUTDOOR = "Outdoor",
    ANY = "Any",
    BUILDING = "Selected per Building Data"
}

interface SpectrumSpec {
    rulesetInfo: { authority: string, rulesetId: string },
    spectrumSchedules: SpectrumSchedule[],
    timeRange?: string,
    frequencyRanges?: FrequencyRange[]
}

interface SpectrumSchedule {
    eventTime: { startTime: string, stopTime: string },
    spectra: {
        resolutionBwHz: number,
        profiles: SpectrumProfile[][]
    }[]
}

export interface SpectrumProfile {
    hz: number, // x axis on graph
    dbm: number | undefined | null // y axis on graph
}

interface FrequencyRange {
    min: number,
    max: number
}

interface DbUpdateSpec { }


export interface AnalysisResults {
    // geoJson object as defined on https://tools.ietf.org/html/rfc7946
    // only properties that are related to this project are listed here.
    geoJson: GeoJson,
    channelData: ChannelData[]
    spectrumData: PAWSResponse,  // Use the same structure as the PAWS response for this part of the response
    statusMessageList?: string[]
}

export interface ExclusionZoneRequest {
    height: number,
    heightType: HeightType,
    heightUncertainty: number,
    indoorOutdoor: IndoorOutdoorType,
    EIRP: number,
    bandwidth: number,
    centerFrequency: number,
    FSID: number
}
export interface ExclusionZoneResult {
    geoJson: GeoJson,
    statusMessageList?: string[]
}

export interface Bounds {
    north: number,
    south: number,
    east: number,
    west: number
}

export interface HeatMapRequest {
    bounds: Bounds,
    bandwidth: number,
    centerFrequency: number,
    spacing: number,
    indoorOutdoor: {
        kind: IndoorOutdoorType.INDOOR | IndoorOutdoorType.OUTDOOR | IndoorOutdoorType.ANY,
        EIRP: number,
        height: number,
        heightType: HeightType,
        heightUncertainty: number
    } | {
        kind: IndoorOutdoorType.BUILDING,
        in: {
            EIRP: number,
            height: number,
            heightType: HeightType,
            heightUncertainty: number
        },
        out: {
            EIRP: number,
            height: number,
            heightType: HeightType,
            heightUncertainty: number
        }
    }
}

export interface HeatMapResult {
    geoJson: GeoJson,
    minItoN: number,
    maxItoN: number,
    threshold: number,
    statusMessageList?: string[]
}

/**
 * GeoJson specifications can be found at: [Geo JSON Spec](https://geojson.org/)
 */
export interface GeoJson {
    type: "FeatureCollection",
    features:
    {
        type: "Feature",
        /**
         * A polymorphic member that has different functions
         * depending on what is being displayed on map
         */
        properties: {
            kind: "FS"
            FSID: number,
            startFreq: number,
            stopFreq: number
        } | {
            /**
             * Buildings bounding box
             */
            kind: "BLDB"
        } | {
            kind: "RLAN",
            FSLonLat: [number, number],
            startFrame?: number,
            endFrame?: number
        } | {
            /**
             * FS Exlusion Zone
             */
            kind: "ZONE", // FS exclusion zone
            FSID: number // accociated FS
            lat: number,
            lon: number,
            terrainHeight: number,
            height: number // AGL using terrainHeight
        } | {
            kind: "HMAP", // heat map
            ItoN: number,
            indoor: "Y" | "N"
        }, // arbitrary description data (useful for pop-ups)
        geometry: {
            type: "Polygon",        // Use for elipses and cones
            coordinates: [number, number][][]
        }
    }[]
}

/**
 * Channel data to be displayed on channel plot using `ChannelDisplay` component
 */
export interface ChannelData {
    channelWidth: number,
    channels: {
        color: string,
        name: string,
        maxEIRP?: number
    }[]
}

/**
 * Specifies a color event in the Mobile AP
 */
export interface ColorEvent {
    startFrame: number,
    endFrame: number,
    blinkPeriod: number,
    colorA: string,
    colorB: string,
    require: {
        type: "REQ_PEND",
        requestIndex: number
    } | {
        type: "NO_SERVICE"
    }
}

/**
 * Configuration object used for Mobile AP Demo
 * @see https://jira.rkf-engineering.com/jira/wiki/p/RAT/view/mobile-ap-demo/176
 */
export interface MobileAPConfig {
    path: {
        frame: number,
        lon: number,
        lat: number
    }[],
    requests: {
        request: PAWSRequest,
        sendRequestFrame: number
        frameStart: number,
        frameEnd: number
    }[],
    colorEvents: ColorEvent[]
}

/**
 * User model modeled on database schema
 */
export interface UserModel {
    id: number,
    email: string,
    active: boolean,
    firstName?: string,
    lastName?: string,
    roles: string[]
}

/**
 * Access point model modeled on database schema
 */
export interface AccessPointModel {
    id: number,
    serialNumber: string,
    model?: string,
    manufacturer?: string,
    certificationId?: string,
    ownerId: number
}

/**
 * Exception type generated when the AFC Engine encounters an error
 */
export interface AFCEngineException {
    type: "AFC Engine Exception",
    exitCode: number,
    description: string,
    env?: any
}

/**
 * Simple implementation of the Either sum type. Used to encode the possibility of success or error in type.
 * This Either type is specialized to encode errors that are usually the result of failed HTTP requests
 * but in principle the type can be used anywhere.
 * See the Haskell Either type for more details on the general type and examples of usage.
 */
export type RatResponse<T> = ResError | ResSuccess<T>

/**
 * Constructs a pure RatResponse that is a success
 * @param res payload of type T
 */
export function success<T>(res: T): ResSuccess<T> {
    return {
        kind: "Success",
        result: res
    }
}

/**
 * Constructs a pure RatResponse that is an error
 * @param des textual description of what went wrong
 * @param code optional numerical error code if present
 * @param body optional context that error occured in. Put stack traces, failed HTTP response, lots of text, or anything else that caller downstream user might find helpful.
 */
export function error(des: string, code?: number, body?: any): ResError {
    return {
        kind: "Error",
        description: des,
        errorCode: code,
        body: body
    }
}

/**
 * Error/Left side of RatResponse
 */
export interface ResError {
    kind: "Error"
    description: string,
    errorCode?: number,
    body?: any
}

/**
 * Success/Right side of RatResponse
 */
export interface ResSuccess<T> {
    kind: "Success",
    result: T
}
