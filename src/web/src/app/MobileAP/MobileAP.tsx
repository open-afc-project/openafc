import * as React from 'react';
import {
  PageSection,
  CardHead,
  CardBody,
  Card,
  Button,
  Title,
  Alert,
  FormGroup,
  InputGroup,
  AlertActionCloseButton,
} from '@patternfly/react-core';
import 'react-measure';
import { MapContainer, MapProps } from '../Components/MapContainer';
import { rasterizeEllipse, distancePtoP, letin } from '../Lib/Utils';
import {
  AnalysisResults,
  PAWSRequest,
  GeoJson,
  PAWSResponse,
  SpectrumProfile,
  ChannelData,
  AFCConfigFile,
} from '../Lib/RatApiTypes';
import { cacheItem, getAfcConfigFile, getCacheItem } from '../Lib/RatApi';
import { SpectrumDisplayPAWS } from '../Components/SpectrumDisplay';
import { logger } from '../Lib/Logger';
import { PAWSAvailableSpectrum } from '../Lib/PawsApi';
import { MobileAPConfig, ColorEvent } from '../Lib/RatApiTypes';
import { filterUNII, generateConfig, PathConfig, pawsToChannels } from './PathProcessor';
import { ChannelDisplay } from '@app/Components/ChannelDisplay';
import Measure from 'react-measure';

/**
 * MobileAP.tsx: Page used for Mobile AP Animation
 * author: Sam Smucny
 */

/**
 * Initial values of map properties
 */
const mapProps: MapProps = {
  geoJson: {
    type: 'FeatureCollection',
    features: [],
  },
  center: {
    lat: 40,
    lng: -100,
  },
  mode: 'Point',
  zoom: 2,
  versionId: 0,
};

interface MapState {
  val: GeoJson;
  text: string;
  valid: boolean;
  dimensions: {
    width: number;
    height: number;
  };
  versionId: number;
}

/**
 * Page level component for Mobile AP
 */
class MobileAP extends React.Component<
  {},
  {
    // Map display data
    mapState: MapState;
    mapCenter: {
      lat: number;
      lng: number;
    };
    zoom: number;
    latVal?: number;
    lonVal?: number;
    latValid: boolean;
    lonValid: boolean;
    markerColor?: string;

    results?: AnalysisResults;
    messageType: 'None' | 'Info' | 'Error' | 'Warn';
    messageTitle: string;
    messageValue: string;
    extraWarning?: string;
    extraWarningTitle?: string;

    /**
     * The current frame being displayed on the map
     */
    currentFrame: number;

    // time line properties (pause/play, playback speed, and timeline size)
    isPlaying: boolean;
    frameIncrement: number;
    timeEditDisabled: boolean;
    maxFrame: number;

    /**
     * PAWS spectrum data that is going to be displayed on the map. Data is
     * drawn from `state.spectrumCache`
     */
    spectrums: (PAWSResponse | undefined)[];

    /**
     * The spectrum cache stores responses and information on when they were received.
     * The cached data is then loaded by reference into the `state.spectrums` object for
     * a given frame to simulate realtime playback
     */
    spectrumCache: { spec: PAWSResponse; channels: ChannelData[]; frameStamp: number }[];

    isCanceled: boolean;
    apConfig?: MobileAPConfig;
    minEIRP: number;
  }
> {
  /**
   * Map styles for drawn objects
   */
  private styles: Map<string, any>;

  /**
   * The period in ms used to update the display
   */
  private intervalUpdatePeriod: number = 100;

  /**
   * Interval Id of play timer. Do NOT update directly.
   * Use `this.setThisInterval()` and `this.clearThisInterval()` instead.
   */
  private intervalId?: number = undefined;

  /**
   * Internal constant that determines the approximate framerate
   */
  private fps = 5;

  handleJsonChange: (value: any) => void;

  constructor(props: any) {
    super(props);

    // set the default start state, but load from cache if available
    this.state = {
      mapState: {
        val: mapProps.geoJson,
        text: '',
        valid: false,
        dimensions: { width: 0, height: 0 },
        versionId: 0,
      },
      mapCenter: {
        lat: 40,
        lng: -100,
      },
      latVal: undefined,
      lonVal: undefined,
      latValid: false,
      lonValid: false,
      zoom: 2,
      messageTitle: '',
      messageValue: '',
      messageType: 'None',
      extraWarning: undefined,
      extraWarningTitle: undefined,
      currentFrame: 0,
      isPlaying: false,
      frameIncrement: 1,
      maxFrame: 30 * 25,
      spectrums: [],
      spectrumCache: [],
      isCanceled: false,
      timeEditDisabled: true,
      minEIRP: 0,
    };

    this.handleJsonChange = (value) => {
      try {
        this.setMapState({ text: value, valid: true, versionId: this.state.mapState.versionId + 1 });
        return;
      } catch (e) {
        this.setMapState({ text: value, valid: false, versionId: this.state.mapState.versionId + 1 });
        return;
      }
    };

    this.styles = new Map([
      ['PATH', { strokeColor: 'red' }],
      ['RLAN', { strokeColor: 'blue', fillColor: 'lightblue' }],
    ]);
  }

  componentDidMount() {
    const st = getCacheItem('mobileAPStateCache');
    if (st !== undefined) this.setState(st);
  }

  componentWillUnmount() {
    // before removing object, let's cache the state in case we want to come back
    const state: any = this.state;
    state.messageType = 'None';
    cacheItem('mobileAPStateCache', state);
  }

  /**
   * Sets the singular private interval
   * @param callback The function to execute when the interval goes off
   * @param interval The period in ms to call `callback`
   */
  private setThisInterval(callback: () => void, interval: number) {
    clearInterval(this.intervalId);
    this.intervalId = setInterval(callback, interval) as unknown as number;
    this.setState({ isPlaying: true });
  }

  /**
   * Clears the singular private interval
   */
  private clearThisInterval() {
    clearInterval(this.intervalId);
    this.intervalId = undefined;
    this.setState({ isPlaying: false });
  }

  /**
   * Increase the speed of playback by 2x
   */
  private speedUp() {
    this.setState({ frameIncrement: this.state.frameIncrement * 2 });
  }

  /**
   * Decrease the speed of playback by 1/2.
   * Does not decrease below 1x.
   */
  private slowDown() {
    if (this.state.frameIncrement >= 2) this.setState({ frameIncrement: this.state.frameIncrement / 2 });
  }

  /**
   * Process a run configuration file and requests fresh data from afc-engine using PAWS
   */
  private async runMobileAP() {
    // get file input and load configuration
    const fileInput = document.getElementById('mobile-ap-run-config-input');
    const files = (fileInput as any).files as File[];

    // if there is a file selected then load it, else do nothing
    if (files.length == 1) {
      logger.info('parsing path config...');
      let pathConf: PathConfig;
      try {
        pathConf = JSON.parse(await files[0].text()) as PathConfig;
      } catch (e) {
        logger.error(e);
        this.setState({
          messageType: 'Error',
          messageTitle: 'Mobile AP Not Loaded',
          messageValue:
            'The Mobile AP configuration could not be loaded from the file. The underlying file may have been modified. Try selecting a configuration file with a different name.',
        });
        return;
      }
      logger.info('parsed path config:', pathConf);

      // convert config into more verbose mobile ap config
      const conf = generateConfig(pathConf, this.fps);

      // load AFC Config from server because we need several properties to compute dispaly information
      logger.info('retreiving afc config');
      //const afcConfig = await getAfcConfigFile();
      const afcConfig = await getAfcConfigFile('US');
      if (afcConfig.kind !== 'Success') {
        this.setState({
          messageType: 'Error',
          messageTitle: 'AFC Config Missing',
          messageValue: 'The AFC Configuration could not be loaded from the server',
        });
        return;
      }

      // update state to prepare for play and load the spectrums
      logger.info('Running mobile AP with config:', conf);
      this.setState({
        apConfig: conf,
        minEIRP: afcConfig.result.minEIRP,
        currentFrame: 0,
        maxFrame: conf.path[conf.path.length - 1].frame,
        spectrumCache: [],
        timeEditDisabled: true,
        frameIncrement: 1,
      });
      this.loadSpectrums(conf.requests, afcConfig.result);
    }
  }

  /**
   * Sequentially loads multiple `PAWSRequest`s.
   * Caches results with timestamp in `this.state.spectrumCache`
   * to be used in playback. This function controls playback
   * as data is loaded.
   * @param requests A list of requests to process in order
   */
  private async loadSpectrums(
    requests: {
      request: PAWSRequest;
      sendRequestFrame: number;
      result?: PAWSResponse;
      frameStart: number;
      frameEnd: number;
    }[],
    config: AFCConfigFile,
  ) {
    try {
      // play and load through each of the PAWS requests in order
      for (let i = 0; i < requests.length; i++) {
        const spectrumResult = await this.loadAndPlayRequest(requests[i]);
        if (spectrumResult === undefined) return;
        const filteredSpectrum = filterUNII(spectrumResult);
        if (spectrumResult !== undefined) {
          this.state.spectrumCache[i] = {
            spec: filteredSpectrum,
            channels: pawsToChannels(filteredSpectrum, config.minEIRP, config.maxEIRP),
            frameStamp: this.state.currentFrame,
          };
          this.setState({ spectrums: this.state.spectrums });
        }
      }

      // play through the remaining paths at the end when no more requests remain
      this.playFromTo(this.state.currentFrame, this.state.maxFrame);

      // Now that all the data is loaded allow replay
      this.setState({ timeEditDisabled: false });
    } catch (e) {
      logger.error(e);
      this.setState({ messageType: 'Error', messageTitle: 'Error: ' + e.code || '', messageValue: e.message });
      this.clearThisInterval();
    }
  }

  /**
   * processes a single request for the mobile ap
   * waits until the request should be sent, sends it,
   * and when the response arrives the playback
   * is paused and the response it is returned
   * @param request the request to process
   */
  private async loadAndPlayRequest(request: {
    request: PAWSRequest;
    sendRequestFrame: number;
    result?: PAWSResponse;
    frameStart: number;
    frameEnd: number;
  }) {
    // If the request is not supposed to be sent yet then continue play until that time
    if (this.state.currentFrame < request.sendRequestFrame) {
      this.setState({ messageType: 'Info', messageTitle: 'No Request Pending...', messageValue: '' });
      await this.playFromTo(this.state.currentFrame, request.sendRequestFrame);
    }

    // Send the request

    // check AGL settings and possibly truncate
    if (
      request.request.antenna.heightType === 'AGL' &&
      request.request.antenna.height - request.request.antenna.heightUncertainty < 1
    ) {
      // modify if height is not 1m above terrain height
      const minHeight = 1;
      const maxHeight = request.request.antenna.height + request.request.antenna.heightUncertainty;
      if (maxHeight < minHeight) {
        this.setState({
          messageType: 'Error',
          messageTitle: 'Invalid Height',
          messageValue: `The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height is ${maxHeight}m`,
        });
        return;
      }
      const newHeight = (minHeight + maxHeight) / 2;
      const newUncertainty = newHeight - minHeight;
      request.request.antenna.height = newHeight;
      request.request.antenna.heightUncertainty = newUncertainty;
      logger.warn('Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement');
      this.setState({
        extraWarningTitle: 'Truncated Height',
        extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m`,
      });
    }
    this.setState({ messageType: 'Info', messageTitle: 'Sending Request...', messageValue: '' });
    const resultPromise = PAWSAvailableSpectrum(request.request);

    // asyncronously play while waiting for response
    this.playFromTo(this.state.currentFrame, this.state.maxFrame);

    // once the result arrives we pause play and load the data before continuing
    const result = await resultPromise;
    this.setState({ messageType: 'Info', messageTitle: 'Result Received.', messageValue: '' });

    // clear the interval set by playFromTo()
    this.clearThisInterval();

    // return the result or throw an error
    if (result.kind === 'Success') {
      logger.info(result.result);
      return result.result;
    } else {
      logger.error(result);
      this.setState({
        messageType: 'Error',
        messageTitle: 'A PAWS request encountered an error',
        messageValue: result.description,
      });
      throw result;
    }
  }

  /**
   * Starts playback of animation between two frames
   * @param from Frame to start at
   * @param to Frame to end at
   */
  private async playFromTo(from: number, to: number) {
    return new Promise((resolve) => {
      this.setState({ currentFrame: from });
      this.setThisInterval(() => {
        const newFrame = Math.min(this.state.currentFrame + this.state.frameIncrement, to);
        this.displayFrame(newFrame);

        if (this.state.currentFrame == to) {
          this.clearThisInterval();
          resolve(); // return to awaiter once interval has cleared
        }
      }, this.intervalUpdatePeriod);
    });
  }

  /**
   * Indicates if a given spectrum should be displayed
   * @param spectrumIndex Index of spectrum to check
   * @param requests List of all requests
   */
  private displaySpectrum = (
    spectrumIndex: number,
    requests: { request: PAWSRequest; result?: PAWSResponse; frameStart: number; frameEnd: number }[],
  ) =>
    requests[spectrumIndex] &&
    requests[spectrumIndex].frameStart <= this.state.currentFrame &&
    requests[spectrumIndex].frameEnd >= this.state.currentFrame;

  /**
   * finds the channels for a given spectrum by reference
   * @param spectrum the coresponding spectrum
   */
  private getChannels = (spectrum?: PAWSResponse): ChannelData[] | undefined =>
    spectrum &&
    letin(
      this.state.spectrumCache.findIndex((o) => o.spec === spectrum),
      (index) => (index !== -1 ? this.state.spectrumCache[index].channels : undefined),
    );

  /**
   * Get the marker position at a given frame
   * @param path Path information to interpolate from
   * @param frameNum Frame at which to get marker position
   */
  private getMarkerPosition(path: { lat: number; lon: number; frame: number }[], frameNum: number) {
    // Find the path that the marker is currently in
    for (let i = 1; i < path.length; i++) {
      if (frameNum <= path[i].frame) {
        // Interpolate marker path
        const deltaLat = path[i].lat - path[i - 1].lat;
        const deltaLon = path[i].lon - path[i - 1].lon;
        const pathFrac = (frameNum - path[i - 1].frame) / (path[i].frame - path[i - 1].frame);
        return { lat: path[i - 1].lat + deltaLat * pathFrac, lng: path[i - 1].lon + deltaLon * pathFrac };
      }
    }
    throw { message: 'Marker position out of frame bounds', frameNum: frameNum, path: path };
  }

  /**
   * Get the GeoJson that is visible at a given frame
   * @param requests List of all requests
   * @param frameNum The current frame
   */
  private getVisibleGeoJson = (
    requests: { request: PAWSRequest; frameStart: number; frameEnd: number }[],
    frameNum: number,
  ) =>
    requests
      // only include requests that overlap with the current frame
      .filter((r, i) => r.frameStart <= frameNum && r.frameEnd >= frameNum && this.state.spectrums[i])
      // convert each visible request into an ellipse
      .map((r) => ({
        type: 'Feature',
        properties: {
          kind: 'RLAN',
          FSLonLat: [r.request.location.point.center.longitude, r.request.location.point.center.latitude],
          startFrame: r.frameStart,
          endFrame: r.frameEnd,
        },
        geometry: {
          type: 'Polygon',
          coordinates: [rasterizeEllipse(r.request.location.point, 16)],
        },
      }))
      // add the polyline for the AP's path
      //@ts-ignore
      .concat(
        !this.state.apConfig
          ? []
          : [
              {
                type: 'Feature',
                properties: {
                  kind: 'PATH',
                },
                geometry: {
                  type: 'LineString',
                  coordinates: this.state.apConfig.path.map((line) => [line.lon, line.lat]),
                },
              },
            ],
      );

  /**
   * Checks if any channel is available in a spectrum (ie. there exists EIRP >= min EIRP)
   * @param spectra specta to check
   * @param minEIRP minimum EIRP value that needs to be met to be available
   */
  private channelAvailable(
    spectra: {
      resolutionBwHz: number;
      profiles: SpectrumProfile[][];
    }[],
    minEIRP: number,
  ) {
    for (let s = 0; s < spectra.length; s++) {
      const spectrum = spectra[s];
      for (let p = 0; p < spectrum.profiles.length; p++) {
        const profile = spectrum.profiles[p];
        for (let i = 0; i < profile.length; i++) {
          if (Number.isFinite(profile[i].dbm) && profile[i].dbm! >= minEIRP) return true;
        }
      }
    }
    return false;
  }

  /**
   * Indicates if a given color event should be engaged
   * based on some requirement
   * @param req `ColorEvent` requirement to check
   */
  private canBlink(req: { type: 'REQ_PEND'; requestIndex: number } | { type: 'NO_SERVICE' }, frameNum: number) {
    if (req.type === 'REQ_PEND') {
      // color event requires that there is a pending request so the coresponding spectrum should be `undefined` (not recieved yet)
      return !this.state.spectrums[req.requestIndex];
    } else if (req.type === 'NO_SERVICE') {
      // color event requres that there is no service so all visible request ellipses should have no available channels
      const minEIRP = this.state.minEIRP;
      // check if AP is in the covered ellipse of any request
      for (let i = 0; i < this.state.apConfig!.requests.length; i++) {
        const request = this.state.apConfig!.requests[i];
        if (
          !!this.state.spectrums[i] &&
          request.frameStart <= frameNum &&
          request.frameEnd >= frameNum &&
          this.channelAvailable(this.state.spectrums[i]!.spectrumSpecs[0].spectrumSchedules[0].spectra, minEIRP)
        ) {
          return false;
        }
      }
      // if nothing in the for loop returned then it has no service
      return true;
    } else {
      logger.warn('invalid color event detected');
      return false;
    }
  }

  /**
   * Gets the marker color at a specific frame
   * @param colorEvents List of `ColorEvent`s with display priority given in order of array
   * @param frameNum Frame to display
   */
  private getMarkerColor = (colorEvents: ColorEvent[], frameNum: number) => {
    const colorEventsFiltered = colorEvents.filter((e) => e.startFrame <= frameNum && e.endFrame >= frameNum);

    for (let i = 0; i < colorEventsFiltered.length; i++) {
      const colorEvent = colorEventsFiltered[i];
      if (this.canBlink(colorEvent.require, frameNum)) {
        // divide two colors in half to simulate blinking
        return (frameNum - colorEvent.startFrame) % colorEvent.blinkPeriod < colorEvent.blinkPeriod / 2
          ? colorEvent.colorA
          : colorEvent.colorB;
      }
    }
    return 'blue';
  };

  private setMapState(obj: any) {
    this.setState({ mapState: Object.assign(this.state.mapState, obj) });
  }

  /**
   * Handler for clicking pause/play button
   */
  private playPause = () => {
    if (!this.state.apConfig) return;
    if (this.state.isPlaying) {
      this.clearThisInterval();
    } else {
      this.playFromTo(this.state.currentFrame, this.state.maxFrame);
    }
  };

  /**
   * Renders a frame on the map
   * @param frameNum The frame to display
   */
  private displayFrame(frameNum: number) {
    if (!this.state.apConfig) return;

    // Update the spectrum list from cache so that it follows playback accuratly
    for (let i = 0; i < this.state.apConfig.requests.length; i++) {
      const cache = this.state.spectrumCache[i];
      this.state.spectrums[i] = cache && cache.frameStamp <= this.state.currentFrame ? cache.spec : undefined;
    }

    // get map display data
    const markerPosition = this.getMarkerPosition(this.state.apConfig.path, frameNum);
    const markerColor = this.getMarkerColor(this.state.apConfig.colorEvents, frameNum);
    const geoJson: GeoJson = {
      type: 'FeatureCollection',
      //@ts-ignore
      features: this.getVisibleGeoJson(this.state.apConfig.requests, frameNum),
    };

    // draw on map
    this.setState({
      currentFrame: frameNum,
      latVal: markerPosition.lat,
      lonVal: markerPosition.lng,
      markerColor: markerColor,
      spectrums: this.state.spectrums,
    });
    this.setMapState({ val: geoJson, versionId: this.state.mapState.versionId + 1 });
  }

  /**
   * Get the estimated speed of the AP in mph
   */
  private getSpeed(frameNum: number) {
    if (!this.state.apConfig) return 0;

    const path = this.state.apConfig.path;
    for (let i = 1; i < path.length; i++) {
      if (frameNum <= path[i].frame) {
        const distMi = distancePtoP(path[i - 1], path[i]);
        const deltaTimeHr = (path[i].frame - path[i - 1].frame) / this.fps / 3600;
        return distMi / deltaTimeHr;
      }
    }
    return 0;
  }

  render() {
    return (
      <PageSection id="mobile-ap-page">
        <Card>
          <CardHead>
            <Title size="lg">Run Mobile AP</Title>
          </CardHead>
          <CardBody>
            <FormGroup label="Mobile AP Path Configuration" fieldId="mobile-ap-run-config">
              <InputGroup>
                <input id="mobile-ap-run-config-input" type="file" accept=".json,application/json" />
              </InputGroup>
            </FormGroup>
            <br />
            <Button key="run-mobile-ap" variant="primary" onClick={() => this.runMobileAP()}>
              Load
            </Button>
          </CardBody>
        </Card>
        <br />
        {this.state.extraWarning && (
          <Alert
            title={this.state.extraWarningTitle || 'Warning'}
            variant="warning"
            action={
              <AlertActionCloseButton
                onClose={() => this.setState({ extraWarning: undefined, extraWarningTitle: undefined })}
              />
            }
          >
            <pre>{this.state.extraWarning}</pre>
          </Alert>
        )}
        {this.state.messageType === 'Info' && (
          <Alert title={this.state.messageTitle} variant="info">
            {this.state.messageValue}
          </Alert>
        )}
        {this.state.messageType === 'Error' && (
          <Alert title={this.state.messageTitle} variant="danger">
            <pre>{this.state.messageValue}</pre>
          </Alert>
        )}
        {this.state.messageType === 'Warn' && (
          <Alert title={this.state.messageTitle} variant="warning">
            <pre>{this.state.messageValue}</pre>
          </Alert>
        )}
        <br />
        <Card>
          <CardHead>Time Line</CardHead>
          <CardBody>
            <span>
              <Button
                key="play-pause"
                variant="primary"
                isDisabled={this.state.timeEditDisabled}
                onClick={() => this.playPause()}
              >
                {this.state.isPlaying ? 'Pause' : 'Play'}
              </Button>{' '}
              <Button
                key="slower-butn"
                variant="secondary"
                isDisabled={this.state.frameIncrement < 2 || this.state.timeEditDisabled}
                onClick={() => this.slowDown()}
              >
                Slower
              </Button>{' '}
              <Button
                key="faster-butn"
                variant="secondary"
                isDisabled={this.state.timeEditDisabled}
                onClick={() => this.speedUp()}
              >
                Faster
              </Button>{' '}
              {this.state.frameIncrement + 'x'}
            </span>
            <br />
            <br />
            <input
              disabled={this.state.timeEditDisabled}
              type="range"
              className="slider"
              min="0"
              max={this.state.maxFrame}
              value={this.state.currentFrame}
              id="time-slider"
              onChange={(event) => this.displayFrame(Number.parseInt(event.target.value))}
            />
            <p>{Math.round(this.state.currentFrame / this.fps)} sec</p>
            <p>{Math.round(this.getSpeed(this.state.currentFrame))} mph</p>
          </CardBody>
        </Card>
        <br />
        <div style={{ width: '100%' }}>
          <MapContainer
            mode="Mobile"
            //@ts-ignore
            markerPosition={
              this.state.latVal && this.state.lonVal && { lat: this.state.latVal, lng: this.state.lonVal }
            }
            markerColor={this.state.markerColor}
            geoJson={this.state.mapState.val}
            styles={this.styles}
            center={mapProps.center}
            zoom={mapProps.zoom}
            versionId={this.state.mapState.versionId}
          />
        </div>
        {this.state.spectrums
          .filter((spec) => !!spec)
          .filter((_, i) => this.displaySpectrum(i, this.state.apConfig!.requests))
          .map((spec, i) => (
            <React.Fragment key={i}>
              <br />
              <Card isHoverable={true}>
                <CardBody>
                  <Measure
                    bounds={true}
                    onResize={(contentRect) =>
                      this.setMapState({
                        dimensions: { width: contentRect.bounds!.width, height: contentRect.bounds!.height },
                      })
                    }
                  >
                    {({ measureRef }) => (
                      <div ref={measureRef}>
                        <ChannelDisplay
                          totalWidth={this.state.mapState.dimensions.width - 10}
                          topLeft={{ x: 5, y: 10 }}
                          channelHeight={30}
                          channels={this.getChannels(spec)}
                        />
                      </div>
                    )}
                  </Measure>
                </CardBody>
              </Card>
              <SpectrumDisplayPAWS key={i} spectrum={spec} greyOutUnii={true} />
            </React.Fragment>
          ))}
      </PageSection>
    );
  }
}

export { MobileAP, MobileAPConfig };
