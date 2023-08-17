import { Ellipse, AnalysisResults, SpectrumProfile, PAWSRequest, PAWSResponse } from "./RatApiTypes";

/**
 * Utils.ts: reusable functions that are accessible across app
 * author: Sam Smucny
 */

/**
 * curried function that rotates `[x,y]` by `theta` (in radians) in the ccw direction
 * @param theta angle to rotate by in counter clockwise direction
 * @param (x, y) Point coordinates to be rotated by `theta`
 * @returns The rotated point
 */
export const rotate = (theta: number) => ([x, y]: [number, number]): [number, number] =>
    [x * Math.cos(theta) - y * Math.sin(theta), y * Math.cos(theta) + x * Math.sin(theta)];

/**
 * Calculates how much a translation in meters is in lat/lon using flat earth approximation.
 * `dx` and `dy` specify an offset vector from some point.
 * @param lat latitude at which to approximate
 * @param (dy, dx) amount to offset in units of meters in each direction
 * @returns a new offset vector in units of degrees of `[longitude, latitude]`
 */
export const meterOffsetToDeg = (lat: number) => ([dy, dx]: [number, number]): [number, number] => {
    const latOffset = dy / 111111; // 1 degree of lat ~ 111111 m in flat approximation which should work here since we have small meter values of uncertainty and it is only a visual display
    const lngOffset = (dx / Math.cos(lat * Math.PI / 180)) / 111111; // 1 degree lng ~ 111111 * sec(lat) m
    return [lngOffset, latOffset]
};

/**
 * approximates an ellipse shape using a polygon with n sides
 * @param e Ellipse object to generate
 * @param n number of sides (more sides is smoother)
 * @returns points along elipse (length is `n+1`)
 */
export const rasterizeEllipse = (e: Ellipse, n: number): [number, number][] => {
    const omega = 2 * Math.PI / n; // define the angle increment in angle to use in raster
    return Array(n + 1).fill(undefined).map((_, i) => {
        const alpha = omega * i; // define angle to transform for this step
        // use ellipse parameterization to generate offset point before rotation
        return ([e.semiMajorAxis * Math.sin(alpha), e.semiMinorAxis * Math.cos(alpha)]) as [number, number];
    })
        .map(rotate(e.orientation * Math.PI / 180))    // rotate offset in meter coordinates by orientation (- sign gives correct behavior with sin/cos)
        .map(meterOffsetToDeg(e.center.latitude))   // transform offset point in meter coordinates to degree offset
        .map(([dLng, dLat]) => [dLng + e.center.longitude, dLat + e.center.latitude])   // add offset to center point
};

/**
 * Number checking continuation.
 * @param s `string` that may be a number
 * @param f continuation function that specifies what to do if value is a number. If `s` is not a number or is not valid then this is not called.
 * @param validate optional validation function which provides additional restrictions on `s`'s value
 */
export function ifNum(s: string, f: (n: number) => void, validate?: (n: number) => boolean): void {
    try {
        const n = Number(s);
        if (Number.isNaN(n)) {
            return;
        } else if (validate === undefined || validate(n)) {
            f(n);
        }
    } catch { }
}

/**
 * Convert decimal to percent
 * @param n decimal value
 * @param percentage value
 */
export const toPer = (n: number) => n * 100;

/**
 * Convert percent to decimal
 * @param n percent value
 * @returns decimal value
 */
export const fromPer = (n: number) => n / 100;

/**
 * Convert from degrees to radians
 * @param n argument in degrees
 */
export const toRad = (n: number) => n * Math.PI / 180.0;

/**
 * Creates an empty promise that resolves after `ms` milliseconds
 * @returns promise that resolves after `ms` milliseconds
 */
export const delay = (ms: number): Promise<void> => new Promise(resolve => setTimeout(resolve, ms));

/**
 * Creates deep copy of object.
 * @param obj object to copy
 * @returns deep copy of `obj`
 */
export const clone = <T>(obj: T): T => JSON.parse(JSON.stringify(obj))

/**
 * Helper continuation function
 * @param obj entity to evaluate
 * @param f continuation function
 * @returns result of `obj` applied to `f` (`f(obj)`)
 */
export const letin = <T, U>(obj: T, f: (a: T) => U) => f(obj);

/**
 * Guaruntees an entity is not undefined
 * @param obj possibly undefined object
 * @param def default to return if object is undefined
 * @returns `obj` value or `def` value if `obj` is `undefined`
 */
export const maybe = <T>(obj: T | undefined, def: T) => obj === undefined ? def : obj;

/**
 * Calculates the distance between two points in miles
 * @param p1 point 1
 * @param p2 point 2
 * @returns distance between `p1` and `p2` in miles
 */
export const distancePtoP = (p1: { lat: number, lon: number }, p2: { lat: number, lon: number }) => {
    // https://www.movable-type.co.uk/scripts/latlong.html
    const R = 6371e3; // metres
    const phi1 = toRad(p1.lat); // φ, λ in radians
    const phi2 = toRad(p2.lat);
    const deltaphi = toRad(p2.lat - p1.lat);
    const deltalambda = toRad(p2.lon - p1.lon);

    const a = Math.sin(deltaphi / 2) * Math.sin(deltaphi / 2) +
        Math.cos(phi1) * Math.cos(phi2) *
        Math.sin(deltalambda / 2) * Math.sin(deltalambda / 2);
    const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

    return R * c / 1609; // in mi
}

/**
 * Calculates the distance between two points in meters
 * @param p1 point 1
 * @param p2 point 2
 * @returns distance between `p1` and `p2` in meters
 */
export const distancePtoPSI = (p1: { lat: number, lon: number }, p2: { lat: number, lon: number }) => {
    return distancePtoP(p1, p2) * 1609; // in m
}

export const bearing = (p1: { lat: number, lon: number }, p2: { lat: number, lon: number }) => {
    // https://www.movable-type.co.uk/scripts/latlong.html
    const dphi = toRad(p2.lat - p1.lat); // radians
    const dlambda = toRad(p2.lon - p1.lon); // radians
    if (dphi === 0)
        return dlambda > 0 ? 90 : 270;
    const y = Math.sin(dlambda) * Math.cos(toRad(p2.lat));
    const x = Math.cos(toRad(p1.lat)) * Math.sin(toRad(p2.lat)) -
        Math.sin(toRad(p1.lat)) * Math.cos(toRad(p2.lat)) * Math.cos(dlambda);
    const θ = Math.atan2(y, x);
    const brng = (θ * 180 / Math.PI + 360) % 360; // bearing in degrees
    return brng;
}

// Gets the last used region from the afc config page value to use in presenting the correct 
// config on the config page and looking up the last used config for showing/hiding the map on the AFC page 
export const getLastUsedRegionFromCookie = () => {

    var lastRegFromCookie: string | undefined = undefined;
    if (document.cookie.indexOf("afc-config-last-region=") >= 0) {
        lastRegFromCookie = document.cookie
            .split("; ")
            .find((row) => row.startsWith("afc-config-last-region="))
            ?.split("=")[1];
    }
    else {
        lastRegFromCookie = "US"
    }

    return lastRegFromCookie!;
}

// Converts the region codes to human readable text
export const mapRegionCodeToName = (code: string) => {
    switch (code) {
        case 'US':
            return 'USA';
            break;
        case 'CA':
            return 'Canada';
            break;
        case 'BR':
            return 'Brazil';
            break;
        case 'GB':
            return 'United Kingdom';
            break;
        default:
            return code;
    }
}

export const trimmedRegionStr = (regionStr: string | undefined) => {
    if (!!regionStr && (regionStr.startsWith("TEST_") || regionStr.startsWith("DEMO_"))) {
        return regionStr.substring(5);
    } else {
        return regionStr;
    }
}