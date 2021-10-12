import * as React from "react";
import { Bounds } from "../Lib/RatApiTypes";

/**
 * RegionSize.tsx: displays size of heat map region in simulation
 * author: Sam Smucny
 */

/**
 * `RegionSize` properties
 */
interface RegionSizeProps {
    bounds?: Bounds
}

const earthRadius = 6.378137e6;
const minAbsLat = (latA: number, latB: number) => Math.min(Math.abs(latA), Math.abs(latB));
const getLatDistance = (latA: number, latB: number) => Math.abs(latA - latB) * Math.PI / 180 * earthRadius;
const getLonDistance = (bounds: Bounds) => 
    Math.abs(bounds.east - bounds.west) * Math.PI / 180 * earthRadius * Math.cos(minAbsLat(bounds.north, bounds.south) * Math.PI / 180);

/**
 * Component that displays the height and width of given bounds in latitude and longitude
 * @param props `RegionSizeProps`
 */
export const RegionSize: React.FunctionComponent<RegionSizeProps> = (props: RegionSizeProps) => (
    <p>
    Selected Region Size (m): {props.bounds ? 
    getLatDistance(props.bounds.north, props.bounds.south).toFixed(0) + " x " + getLonDistance(props.bounds).toFixed(0)
    : "No heat map region defined"}
    </p>
)