import { GeoJson } from '../Lib/RatApiTypes';
import {
  CircleTriple,
  ExclusionZoneInfo,
  ExclusionZoneInquiryResponseMessage,
  LatLonPair,
  Location,
  RadialPolygon,
} from '../Lib/GvpTypes';
import { meterOffsetToDeg, rasterizeEllipse } from '../Lib/Utils';

/**
 * GvpGeoJson.ts: Conversion of SDI exclusion zones and areas of operation to GeoJson for the map
 */

const RING_POINTS = 32;

type Feature = GeoJson['features'][number];

/** A [latitude, longitude, radius] circle as a closed ring of [longitude, latitude] points. */
const circleRing = ([latitude, longitude, radius]: CircleTriple): [number, number][] =>
  rasterizeEllipse(
    { center: { latitude, longitude }, semiMajorAxis: radius, semiMinorAxis: radius, orientation: 0 },
    RING_POINTS,
  );

const polygonRing = (corners: LatLonPair[]): [number, number][] => {
  const ring = corners.map(([latitude, longitude]): [number, number] => [longitude, latitude]);
  ring.push(ring[0]);
  return ring;
};

// Vector angles are clockwise from true north, so north is length * cos and east is length * sin.
const radialRing = (polygon: RadialPolygon): [number, number][] => {
  const { latitude, longitude } = polygon.center;
  const ring = polygon.outerBoundary.map((vector): [number, number] => {
    const angle = (vector.angle * Math.PI) / 180;
    const [dLng, dLat] = meterOffsetToDeg(latitude)([vector.length * Math.cos(angle), vector.length * Math.sin(angle)]);
    return [longitude + dLng, latitude + dLat];
  });
  return ring.concat([ring[0]]);
};

const zoneFeature = (zone: ExclusionZoneInfo, ring: [number, number][], selected: boolean): Feature => ({
  type: 'Feature',
  properties: {
    kind: 'GVP_ZONE',
    exclusionZoneId: zone.exclusionZoneId,
    lowFrequency: zone.exclusionZoneFrequencyRange.lowFrequency,
    highFrequency: zone.exclusionZoneFrequencyRange.highFrequency,
    psdLevel: zone.psdLevel,
    selected: selected,
  },
  geometry: { type: 'Polygon', coordinates: [ring] },
});

export const zoneList = (msg: ExclusionZoneInquiryResponseMessage): ExclusionZoneInfo[] => {
  const zones: ExclusionZoneInfo[] = [];
  for (const response of msg.exclusionZoneInquiryResponses) {
    zones.push(...(response.exclusionZoneInfo || []));
  }
  return zones;
};

/** selectedZone indexes zoneList, the same order the zone table lists. */
export const gvpToGeoJson = (msg: ExclusionZoneInquiryResponseMessage, selectedZone?: number): GeoJson => {
  const features: Feature[] = [];
  const zones = zoneList(msg);
  for (let index = 0; index < zones.length; index++) {
    const zone = zones[index];
    const selected = index === selectedZone;
    for (const circle of zone.circles || []) {
      features.push(zoneFeature(zone, circleRing(circle), selected));
    }
    for (const polygon of zone.polygons || []) {
      features.push(zoneFeature(zone, polygonRing(polygon), selected));
    }
  }
  return { type: 'FeatureCollection', features };
};

const areaRing = (area: Location): [number, number][] | undefined => {
  if (area.ellipse) {
    const { center, majorAxis, minorAxis, orientation } = area.ellipse;
    return rasterizeEllipse({ center, semiMajorAxis: majorAxis, semiMinorAxis: minorAxis, orientation }, RING_POINTS);
  }
  if (area.circle) {
    return circleRing([area.circle.latitude, area.circle.longitude, area.circle.radius]);
  }
  if (area.linearPolygon) {
    return polygonRing(area.linearPolygon.outerBoundary.map((p): LatLonPair => [p.latitude, p.longitude]));
  }
  if (area.radialPolygon) {
    return radialRing(area.radialPolygon);
  }
  return undefined;
};

export const areaFeature = (area: Location): Feature | undefined => {
  const ring = areaRing(area);
  return ring === undefined
    ? undefined
    : {
        type: 'Feature',
        properties: { kind: 'RLAN', FSLonLat: ring[0] },
        geometry: { type: 'Polygon', coordinates: [ring] },
      };
};
