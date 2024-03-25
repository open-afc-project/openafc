import { clone, distancePtoPSI, distancePtoP, bearing } from '../Lib/Utils';
import { emptyChannels } from '../Components/ChannelDisplay';
import { MobileAPConfig } from './MobileAP';
import {
  PAWSRequest,
  ColorEvent,
  HeightType,
  IndoorOutdoorType,
  PAWSResponse,
  SpectrumProfile,
  ChannelData,
} from '../Lib/RatApiTypes';
import { group } from 'console';

/**
 * PathProcessor.tsx: additional mobile ap helper functions
 * author: Sam Smucny
 */

/**
 * Path configuration object for Mobile AP
 * @see https://jira.rkf-engineering.com/jira/wiki/p/RAT/view/mobile-ap-demo/176
 */
export interface PathConfig {
  path: {
    mph: number;
    lat: number;
    lon: number;
  }[];
  requestLeadTimeSec: number;
  extraHandoffTimeSec: number;
  serialNumber: string;
  eccentricity: number;
  fociBuffer: number;
  antenna: {
    height: number;
    heightType: HeightType;
    heightUncertainty: number;
  };
  capabilities: {
    indoorOutdoor: IndoorOutdoorType;
  };
}

/**
 * comuptes a timestamp based on multiple relative input conditions
 * @param initialOffset offset to add before all others
 * @param legs sections of path that add time
 * @param finalOffset offset to add after all others
 */
const timeStamp = (initialOffset: number) => (legs: { frameSpan: number }[]) => (finalOffset: number) =>
  initialOffset + finalOffset + legs.map((l) => l.frameSpan).reduce((x, y) => x + y, 0);

/**
 * compute midpoint between two other cartesian points
 * @param p1 point 1
 * @param p2 point 2
 */
const meanPoint = (p1: { lat: number; lon: number }, p2: { lat: number; lon: number }) => ({
  latitude: (p1.lat + p2.lat) / 2,
  longitude: (p1.lon + p2.lon) / 2,
});

/**
 * computes the major axis of an ellipse in m
 * @param eccentricity eccentricity of ellipse
 * @param fociSeparation distance between ellipse foci in m
 */
const majorAxis = (eccentricity: number, fociSeparation: number) =>
  (fociSeparation / 2) * Math.sqrt(1 + 1 / (1 / (1 - eccentricity ** 2) - 1));

/**
 * computes the minor axis of an ellipse in m
 * @param eccentricity eccentricity of ellipse
 * @param fociSeparation distance between ellipse foci in m
 */
const minorAxis = (eccentricity: number, fociSeparation: number) =>
  (fociSeparation / 2) * Math.sqrt(1 / (1 / (1 - eccentricity ** 2) - 1));

/**
 * convert megahertz to hertz
 * @param MHz megahertz
 */
const toHz = (MHz: number) => MHz * 1.0e6;

/**
 * lower <= x <= upper
 * @param lower lower bound
 * @param x value
 * @param upper upper bound
 */
const between = (lower: number, x: number, upper: number) => lower <= x && x <= upper;

/**
 * Modifies a bandwidth group so that certain frequency bands are excluded
 * @param bandwidthGroup group to modify
 * @param lower1 lower bound of band 1 to exclude
 * @param upper1 upper bound of band 1 to exclude
 * @param lower2 lower bound of band 2 to exclude
 * @param upper2 upper bound of band 2 to exclude
 */
const processGroup = (
  bandwidthGroup: {
    resolutionBwHz: number;
    profiles: SpectrumProfile[][];
  },
  lower1: number,
  upper1: number,
  lower2: number,
  upper2: number,
) => {
  const low1 = toHz(lower1);
  const up1 = toHz(upper1);
  const low2 = toHz(lower2);
  const up2 = toHz(upper2);
  for (let i = 0; i < bandwidthGroup.profiles.length; i++) {
    for (let j = 0; j < bandwidthGroup.profiles[i].length - 1; j++) {
      let curr = bandwidthGroup.profiles[i][j];
      let next = bandwidthGroup.profiles[i][j + 1];

      if (
        (curr.hz !== next.hz && between(low1, curr.hz, up1) && between(low1, next.hz, up1)) ||
        (curr.hz !== next.hz && between(low2, curr.hz, up2) && between(low2, next.hz, up2))
      ) {
        curr.dbm = -Infinity;
        next.dbm = -Infinity;
        j++; // we can skip next
      }
    }
  }
};

/**
 * removes U-NII-6/8 from channel list
 * @param channelsIn channels to be processed
 * @returns processed channels
 */
export const filterChannels = (channelsIn: ChannelData[]): ChannelData[] => {
  let channels = clone(channelsIn);
  for (let b = 0; b < channels.length; b++) {
    let group = channels[b];
    for (let c = 0; c < group.channels.length; c++) {
      let channel = group.channels[c];
      if (group.channelWidth === 20) {
        if (between(97, Number(channel.name), 113) || between(185, Number(channel.name), 233)) {
          channel.maxEIRP = -Infinity;
          channel.color = 'grey';
        }
      } else if (group.channelWidth === 40) {
        if (between(99, Number(channel.name), 115) || between(187, Number(channel.name), 227)) {
          channel.maxEIRP = -Infinity;
          channel.color = 'grey';
        }
      } else if (group.channelWidth === 80) {
        if (between(103, Number(channel.name), 119) || between(183, Number(channel.name), 215)) {
          channel.maxEIRP = -Infinity;
          channel.color = 'grey';
        }
      } else if (group.channelWidth === 160) {
        if (between(111, Number(channel.name), 111) || between(175, Number(channel.name), 207)) {
          channel.maxEIRP = -Infinity;
          channel.color = 'grey';
        }
      }
    }
  }
  return channels;
};

/**
 * modifies `channels` so that they have the correct colors and dbm values to match the given `prof`
 * @param prof spectrum profile to use for reference
 * @param channels channels to modify
 * @param minEIRP min EIRP to use to color channels
 * @param maxEIRP max EIRP to use to color channels
 */
const assignChannels = (prof: SpectrumProfile[], channels: ChannelData, minEIRP: number, maxEIRP: number): void => {
  for (let i = 0; i < channels.channels.length; i++) {
    let chan = channels.channels[i];
    let point = prof[i * 2];
    const dbm = point.dbm;
    chan.color =
      dbm === null
        ? 'red'
        : dbm === undefined
          ? 'grey'
          : dbm === -Infinity
            ? 'grey'
            : dbm >= maxEIRP
              ? 'green'
              : dbm >= minEIRP
                ? 'yellow'
                : 'red';
    chan.maxEIRP = dbm || undefined;
  }
};

/**
 * processes a paws response into channel data
 * @param resp response to process
 * @param minEIRP min EIRP
 * @param maxEIRP max EIRP
 * @returns the processed channel list
 */
export const pawsToChannels = (resp: PAWSResponse, minEIRP: number, maxEIRP: number): ChannelData[] => {
  const channels = clone(emptyChannels);
  const spectra = resp.spectrumSpecs[0].spectrumSchedules[0].spectra;
  spectra.forEach((spectrum) => {
    if (toHz(20) === spectrum.resolutionBwHz) {
      assignChannels(spectrum.profiles[0], channels[0], minEIRP, maxEIRP);
    } else if (toHz(40) === spectrum.resolutionBwHz) {
      assignChannels(spectrum.profiles[0], channels[1], minEIRP, maxEIRP);
    } else if (toHz(80) === spectrum.resolutionBwHz) {
      assignChannels(spectrum.profiles[0], channels[2], minEIRP, maxEIRP);
    } else if (toHz(160) === spectrum.resolutionBwHz) {
      assignChannels(spectrum.profiles[0], channels[3], minEIRP, maxEIRP);
    }
  });
  return channels;
};

/**
 * excludes U-NII-6 and U-NII-8 from channels
 * @param request PAWSRequest to process
 * @returns processed request
 */
export const filterUNII = (respIn: PAWSResponse): PAWSResponse => {
  let resp = clone(respIn);
  resp.spectrumSpecs.forEach((spec) =>
    spec.spectrumSchedules.forEach((spectrum) =>
      spectrum.spectra.forEach((bandwidthGroup) => {
        if (bandwidthGroup.resolutionBwHz === toHz(160)) {
          processGroup(bandwidthGroup, 6425, 6585, 6745, 7125);
        } else if (bandwidthGroup.resolutionBwHz === toHz(80)) {
          processGroup(bandwidthGroup, 6425, 6585, 6825, 7125);
        } else if (bandwidthGroup.resolutionBwHz === toHz(40)) {
          processGroup(bandwidthGroup, 6425, 6545, 6865, 7125);
        } else if (bandwidthGroup.resolutionBwHz === toHz(20)) {
          processGroup(bandwidthGroup, 6425, 6525, 6865, 7125);
        }
      }),
    ),
  );
  return resp;
};

/**
 * Uses a `PathConfig` to build a more detailed `MobileAPConfig` which is used to run the demo
 * @param pathConfig `PathConfig` to expand into a `MobileAPConfig`
 * @param fps The frames per second to use to compute frame numbers from time information
 */
export function generateConfig(pathConfig: PathConfig, fps: number): MobileAPConfig {
  const path = pathConfig.path;
  const stationaryStartOffset = pathConfig.requestLeadTimeSec * fps;
  const extraHandoffFrames = pathConfig.extraHandoffTimeSec * fps;
  const fociBuffer = pathConfig.fociBuffer * 2; // multiply by 2 because we want the buffer on both sides

  const pathOffset = clone(path);
  pathOffset.shift();

  const timeStampOrigin = timeStamp(stationaryStartOffset);

  const legs = pathOffset.map((e, i) => ({
    start: path[i],
    end: e,
    distanceM: distancePtoPSI(path[i], e),
    speedMph: path[i].mph,
    frameSpan: (distancePtoP(path[i], e) / path[i].mph) * 3600 * fps,
  }));

  // calculate axis lengths from eccentricity. check to make sure eccentricity is valid
  let e = pathConfig.eccentricity;
  if (e <= 0 || e >= 1) throw { message: 'Ecentricity is not valid. Must be in (0,1) for an ellipse.' };

  const requests: {
    request: PAWSRequest;
    sendRequestFrame: number;
    frameStart: number;
    frameEnd: number;
  }[] = legs.map((leg, i) => ({
    sendRequestFrame: timeStampOrigin(legs.filter((l, j) => j < i))(-stationaryStartOffset),
    frameStart: timeStampOrigin(legs.filter((l, j) => j < i))(-stationaryStartOffset),
    frameEnd: timeStampOrigin(legs.filter((l, j) => j <= i))(extraHandoffFrames),
    request: {
      type: 'AVAIL_SPECTRUM_REQ',
      version: '1.0',
      deviceDesc: {
        serialNumber: pathConfig.serialNumber,
        rulesetIds: ['AFC-6GHZ-DEMO-1.1'],
      },
      antenna: pathConfig.antenna,
      capabilities: pathConfig.capabilities,
      location: {
        point: {
          // linear eccentricity = leg.distanceM / 2
          center: meanPoint(leg.start, leg.end),
          semiMajorAxis: majorAxis(e, leg.distanceM + fociBuffer),
          semiMinorAxis: minorAxis(e, leg.distanceM + fociBuffer),
          orientation: bearing(leg.start, leg.end),
        },
      },
    },
  }));

  const frammedPath = [{ lat: path[0].lat, lon: path[0].lon, frame: 0 }].concat(
    path.map((stop, i) => ({
      lat: stop.lat,
      lon: stop.lon,
      frame: timeStampOrigin(legs.filter((leg, j) => j < i))(0),
    })),
  );

  const colorEvents: ColorEvent[] = [
    {
      colorA: 'yellow',
      colorB: 'firebrick',
      blinkPeriod: 5,
      require: {
        type: 'NO_SERVICE',
      },
      startFrame: 0,
      endFrame: frammedPath[frammedPath.length - 1].frame,
    } as unknown as ColorEvent,
  ].concat(
    requests.map((req, i) => ({
      colorA: 'yellow',
      colorB: 'green',
      blinkPeriod: 7,
      require: {
        type: 'REQ_PEND',
        requestIndex: i,
      },
      startFrame: req.sendRequestFrame,
      endFrame: req.frameEnd,
    })),
  );

  return {
    path: frammedPath,
    colorEvents: colorEvents,
    requests: requests,
  };
}
