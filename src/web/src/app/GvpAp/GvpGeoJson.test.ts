import { gvpToGeoJson } from './GvpGeoJson';
import { ExclusionZoneInfo, ExclusionZoneInquiryResponseMessage } from '../Lib/GvpTypes';

const range = { lowFrequency: 5925, highFrequency: 6425 };

const message = (zone: ExclusionZoneInfo): ExclusionZoneInquiryResponseMessage => ({
  version: '1.0',
  exclusionZoneInquiryResponses: [
    {
      requestId: '0',
      rulesetId: 'US_47_CFR_PART_15_SUBPART_E_GVP',
      exclusionZoneInfo: [zone],
      response: { responseCode: 0 },
    },
  ],
});

it('flips polygon corners to [longitude, latitude] and closes the ring', () => {
  const zone: ExclusionZoneInfo = {
    exclusionZoneFrequencyRange: range,
    psdLevel: 11,
    polygons: [
      [
        [37, -121],
        [38, -121],
        [38, -122],
      ],
    ],
  };
  const ring = gvpToGeoJson(message(zone)).features[0].geometry.coordinates[0];
  expect(ring).toEqual([
    [-121, 37],
    [-121, 38],
    [-122, 38],
    [-121, 37],
  ]);
});

it('draws a circle as a closed ring around its center', () => {
  const zone: ExclusionZoneInfo = {
    exclusionZoneFrequencyRange: range,
    psdLevel: 11,
    circles: [[37.38, -121.97, 31000]],
  };
  const ring = gvpToGeoJson(message(zone)).features[0].geometry.coordinates[0];
  const points = ring.slice(0, -1);
  expect(ring[0]).toEqual(ring[ring.length - 1]);
  expect(points.reduce((sum, p) => sum + p[0], 0) / points.length).toBeCloseTo(-121.97, 2);
  expect(points.reduce((sum, p) => sum + p[1], 0) / points.length).toBeCloseTo(37.38, 2);
});
