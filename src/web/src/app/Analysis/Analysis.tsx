import * as React from "react";
import { PageSection, TextArea, CardHead, CardBody, Card, Modal, Button, Title, Expandable, Alert, GalleryItem, FormGroup, InputGroup, TextInput, InputGroupText, Gallery, ClipboardCopy, ClipboardCopyVariant, AlertActionCloseButton, Checkbox } from "@patternfly/react-core";
import "react-measure";
import { MapContainer, MapProps } from "../Components/MapContainer";
import { ChannelDisplay } from "../Components/ChannelDisplay";
import Measure from "react-measure";
import { rasterizeEllipse } from "../Lib/Utils";
import { PAWSForm } from "../VirtualAP/PAWSForm";
import { AnalysisResults, PAWSRequest, GeoJson } from "../Lib/RatApiTypes";
import { phase1Analysis, cacheItem, getCacheItem } from "../Lib/RatApi";
import { SpectrumDisplayPAWS } from "../Components/SpectrumDisplay";
import DownloadContents from "../Components/DownloadContents";
import { logger } from "../Lib/Logger";
import LoadLidarBounds from "../Components/LoadLidarBounds";
import { AnalysisProgress } from "../Components/AnalysisProgress";
import { filterChannels, filterUNII } from "../MobileAP/PathProcessor";

/**
 * Analysis.tsx: Page used for graphical analysis. Includes map display, channels, and spectrums
 * author: Sam Smucny
 */

/**
 * Initial values of map properties
 */
const mapProps: MapProps = {
  geoJson: {
    type: "FeatureCollection",
    features: []
  },
  center: {
    lat: 40,
    lng: -100
  },
  mode: "Point",
  zoom: 2,
  versionId: 0
}

interface MapState {
  val: GeoJson,
  text: string,
  valid: boolean,
  dimensions: {
    width: number,
    height: number
  },
  isModalOpen: boolean,
  versionId: number
}

/**
 * Page level component for Point Analysis
 */
class Analysis extends React.Component<{},
  {
    mapState: MapState,
    mapCenter: {
      lat: number,
      lng: number
    },
    zoom: number,
    latVal?: number,
    lonVal?: number,
    latValid: boolean,
    lonValid: boolean,
    results?: AnalysisResults,
    kml?: Blob,
    showParams: boolean,
    messageType: "None" | "Info" | "Error" | "Warn",
    messageTitle: string,
    messageValue: string,
    extraWarning?: string,
    extraWarningTitle?: string,
    isModalOpen: boolean,
    canCancelTask: boolean,
    progress: {
      percent: number,
      message: string
    },
    filterUNII: boolean
  }> {

  private styles: Map<string, any>;
  private paramValue?: PAWSRequest;
  private formUpdateCallback?: (val: PAWSRequest) => void;

  /**
  * Function used to cancel a long running task
  */
  cancelTask?: () => void;

  handleJsonChange: (value: any) => void;

  constructor(props: any) {
    super(props);

    // set the default start state, but load from cache if available
    this.state = {
      showParams: true,
      mapState: {
        isModalOpen: false,
        val: mapProps.geoJson,
        text: "", valid: false,
        dimensions: { width: 0, height: 0 },
        versionId: 0
      },
      mapCenter: {
        lat: 40,
        lng: -100
      },
      latVal: undefined,
      lonVal: undefined,
      extraWarning: undefined,
      extraWarningTitle: undefined,
      latValid: false,
      lonValid: false,
      zoom: 2,
      messageTitle: "",
      messageValue: "",
      messageType: "None",
      isModalOpen: false,
      canCancelTask: false,
      progress: {
        message: "",
        percent: 0
      },
      filterUNII: false
    };

    this.handleJsonChange = (value) => {
      try {
        this.setMapState({ text: value, valid: true, versionId: this.state.mapState.versionId + 1 });
        return;
      } catch (e) {
        this.setMapState({ text: value, valid: false, versionId: this.state.mapState.versionId + 1 });
        return;
      }
    }

    this.cancelTask = undefined;

    this.styles = new Map([
      ["BLDB", { fillOpacity: 0, strokeColor: "blue" }],
      ["RLAN", { strokeColor: "blue", fillColor: "lightblue" }]
    ]);
  }

  componentDidMount() {
    const st = getCacheItem("analysisStateCache");
    if (st !== undefined)
      this.setState(st);
  }

  componentWillUnmount() {
    // before removing object, let's cache the state in case we want to come back
    const state: any = this.state;
    state.messageType = "None";

    // cancel running task
    this.cancelTask && this.cancelTask();

    cacheItem("analysisStateCache", state);
  }

  private setLat(num: number) {
    this.setState({ latVal: num });
    if (num >= -90 && num <= 90) {
      this.setState({ latValid: true });
    } else {
      this.setState({ latValid: false });
    }
    this.setMapState({ versionId: this.state.mapState.versionId + 1 });
  }

  private setLng(num: number) {
    this.setState({ lonVal: num });
    if (num >= -180 && num <= 180) {
      this.setState({ lonValid: true });
    } else {
      this.setState({ lonValid: false });
    }
    this.setMapState({ versionId: this.state.mapState.versionId + 1 });
  }

  private onMarkerUpdate(lat: number, lon: number) {
    this.setState({
      latVal: lat,
      latValid: true,
      lonVal: lon,
      lonValid: true
    })
  }

  private setMapState(obj: any) {
    this.setState({ mapState: Object.assign(this.state.mapState, obj) });
  }

  /**
   * Run a point anlaysis on AFC Engine
   * @param params parameters for AFC Engine
   */
  private async runAnalysis(params: PAWSRequest) {

    if (!this.state.latValid || !this.state.lonValid) {
      this.setState({
        messageType: "Error",
        messageTitle: "Bad location",
        messageValue: "The selected latitude and/or longitude are invalid"
      })
      return;
    } else {
      params.location.point.center = {
        latitude: this.state.latVal!,
        longitude: this.state.lonVal!
      }
    }

    // check AGL settings and possibly truncate
    if (params.antenna.heightType === "AGL" && params.antenna.height - params.antenna.heightUncertainty < 1) {
      // modify if height is not 1m above terrain height
      const minHeight = 1;
      const maxHeight = params.antenna.height + params.antenna.heightUncertainty;
      if (maxHeight < minHeight) {
        this.setState({
          messageType: "Error",
          messageTitle: "Invalid Height",
          messageValue: `The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height is ${maxHeight}m`
        });
        return;
      }
      const newHeight = (minHeight + maxHeight) / 2;
      const newUncertainty = newHeight - minHeight;
      params.antenna.height = newHeight;
      params.antenna.heightUncertainty = newUncertainty;
      logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
      this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m`});
    }

    this.setState({ messageType: "Info", messageTitle: "Processing", messageValue: "", progress: { percent: 0, message: "Submitting..." } });

    let taskCanceled = false;
    this.cancelTask = () => {
      taskCanceled = true;
      this.cancelTask = undefined;
      this.setState({ canCancelTask: false });
    }
    this.setState({ canCancelTask: true });
    const filterUnii = this.state.filterUNII;

    // make api call
    await phase1Analysis(params, () => taskCanceled, (prog) => this.setState({ progress: prog }), (kml: Blob) => this.setState({ kml: kml })).then(res => {
      if (res.kind === "Success") {
        this.setMapState({
          isModalOpen: false,
          val: mapProps.geoJson,
          text: "", valid: false,
          dimensions: { width: 0, height: 0 },
          versionId: 0
        });
        if (filterUnii) {
          res.result.spectrumData = filterUNII(res.result.spectrumData);
          res.result.channelData = filterChannels(res.result.channelData);
        }
        res.result.geoJson.features.push(
          {
            type: "Feature",
            properties: {
              kind: "RLAN",
              FSLonLat: [
                params.location.point.center.longitude,
                params.location.point.center.latitude
              ]
            },
            geometry: {
              type: "Polygon",
              coordinates: [rasterizeEllipse(params.location.point, 32)]
            }
          });
        logger.error("result", res.result, "state", {...this.state})
        this.setState({
          results: res.result,
          messageType: res.result.statusMessageList.length ? "Warn" : "None",
          messageTitle: res.result.statusMessageList.length ? "Status messages" : "",
          messageValue: res.result.statusMessageList.length ? res.result.statusMessageList.join("\n") : "",
          mapCenter: {
            lat: params.location.point.center.latitude,
            lng: params.location.point.center.longitude
          },
          canCancelTask: false
        });
        this.setMapState({
          val: res.result.geoJson,
          text: JSON.stringify(res.result.geoJson, null, 2),
          valid: false,
          versionId: this.state.mapState.versionId + 1
        });
      } else {
        // error in running analysis
        this.setState({
          messageType: "Error",
          messageTitle: "Error: " + res.errorCode,
          messageValue: res.description,
          canCancelTask: false
        });
      }
    })

    return;
  }

  private setConfig(value: string) {
    try {
      const params: PAWSRequest = JSON.parse(value) as PAWSRequest;
      const lat: number | undefined = params.location.point.center.latitude;
      const lon: number | undefined = params.location.point.center.longitude;
      if (lat !== undefined)
        this.setLat(lat);
      if (lon !== undefined)
        this.setLng(lon);
      if (this.formUpdateCallback)
        this.formUpdateCallback(params);
    } catch (e) {
      logger.error("Pasted value is not valid JSON");
    }
  }

  private copyPaste(formData: PAWSRequest, updateCallback: (v: PAWSRequest) => void) {
    formData.location.point.center.latitude = this.state.latVal;
    formData.location.point.center.longitude = this.state.lonVal;
    this.formUpdateCallback = updateCallback;
    this.paramValue = formData;
    this.setState({ isModalOpen: true });
  }

  private getParamsText = () => this.paramValue ? JSON.stringify(this.paramValue) : "";

  render() {

    const toggleParams = () => this.setState({ showParams: !this.state.showParams });

    return (
      <PageSection id="point-analysis-page">
        <Card>
          <CardHead>
            <Title size="lg">Run Point Analysis</Title>
          </CardHead>
          <Modal
            title="Copy/Paste"
            isLarge={true}
            isOpen={this.state.isModalOpen}
            onClose={() => this.setState({ isModalOpen: false })}
            actions={[<Button key="update" variant="primary" onClick={() => this.setState({ isModalOpen: false })}>Close</Button>]}>
            <ClipboardCopy variant={ClipboardCopyVariant.expansion} onChange={(v: string) => this.setConfig(v)} aria-label="text area">{this.getParamsText()}</ClipboardCopy>
          </Modal>
          <CardBody>
            <Expandable toggleText={this.state.showParams ? "Hide parameters" : "Show parameters"} onToggle={toggleParams} isExpanded={this.state.showParams}>
              <Gallery gutter="sm">
                <GalleryItem>
                  <FormGroup label="Latitude"
                    fieldId="horizontal-form-latitude">
                    <InputGroup>
                      <TextInput
                        value={this.state.latVal}
                        onChange={x => this.setLat(Number(x))}
                        type="number"
                        step="any"
                        id="horizontal-form-latitude"
                        name="horizontal-form-latitude"
                        isValid={this.state.latValid}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                  <FormGroup label="Longitude"
                    fieldId="horizontal-form-longitude">
                    <InputGroup>
                      <TextInput
                        value={this.state.lonVal}
                        onChange={x => this.setLng(Number(x))}
                        type="number"
                        step="any"
                        id="horizontal-form-longitude"
                        name="horizontal-form-longitude"
                        isValid={this.state.lonValid}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                      <Checkbox
                        label="Only use U-NII-5/7"
                        isChecked={this.state.filterUNII}
                        onChange={x => this.setState({ filterUNII: x })}
                        id="horizontal-form-unii"
                        name="horizontal-form-unii"
                        style={{ textAlign: "right" }}
                      />
                </GalleryItem>
              </Gallery>
              <PAWSForm parentName="analysisPage" onSubmit={x => this.runAnalysis(x)} onCopyPaste={(formData: PAWSRequest, updateCallback: (v: PAWSRequest) => void) => this.copyPaste(formData, updateCallback)} />
              {this.state.canCancelTask && <>
                {" "}<Button variant="secondary" onClick={this.cancelTask}>Cancel</Button>
              </>
              }
              {" "}<LoadLidarBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />
            </Expandable>
          </CardBody>
        </Card>
        <br />
        {
          this.state.extraWarning &&
          <Alert title={this.state.extraWarningTitle || "Warning"} variant="warning" action={<AlertActionCloseButton onClose={() => this.setState({ extraWarning: undefined, extraWarningTitle: undefined })} />}>
            <pre>{this.state.extraWarning}</pre>
          </Alert>
        }
        {
          this.state.messageType === "Info" &&
          <Alert title={this.state.messageTitle} variant="info">
            {this.state.messageValue}
            <AnalysisProgress percent={this.state.progress.percent} message={this.state.progress.message} />
          </Alert>
        }
        {
          this.state.messageType === "Error" &&
          <Alert title={this.state.messageTitle} variant="danger">
            <pre>{this.state.messageValue}</pre>
          </Alert>
        }
        {
          this.state.messageType === "Warn" &&
          <Alert title={this.state.messageTitle} variant="warning">
            <pre>{this.state.messageValue}</pre>
          </Alert>
        }
        <br />
        <Modal title="Paste Geo Json"
          isOpen={this.state.mapState.isModalOpen}
          onClose={() => this.setMapState({ isModalOpen: false })}
          actions={[<Button key="update" variant="primary" onClick={() => this.setMapState({ val: JSON.parse(this.state.mapState.text), isModalOpen: false })}>Update</Button>]}>
          <TextArea isValid={this.state.mapState.valid} value={this.state.mapState.text} onChange={this.handleJsonChange} aria-label="text area" />
        </Modal>
        <div style={{ width: "100%" }}>
          <MapContainer
            mode="Point"
            onMarkerUpdate={(lat: number, lon: number) => this.onMarkerUpdate(lat, lon)}
            markerPosition={({ lat: this.state.latVal, lng: this.state.lonVal })}
            geoJson={this.state.mapState.val}
            styles={this.styles}
            center={mapProps.center}
            zoom={mapProps.zoom}
            versionId={this.state.mapState.versionId} />
        </div>
        {
          (this.state.results && this.state.kml) &&
          <DownloadContents contents={() => this.state.kml} fileName="results.kmz" />
        }
        <br />
        <Card isHoverable={true}><CardBody><Measure bounds={true}
          onResize={contentRect => this.setMapState({ dimensions: { width: contentRect.bounds!.width, height: contentRect.bounds!.height } })}>
          {({ measureRef }) => <div ref={measureRef}>
            <ChannelDisplay
              totalWidth={this.state.mapState.dimensions.width - 10}
              topLeft={{ x: 5, y: 10 }}
              channelHeight={30}
              channels={this.state.results ? this.state.results.channelData : undefined} />
          </div>}
        </Measure></CardBody></Card>
        <br />
        <SpectrumDisplayPAWS spectrum={this.state.results ? this.state.results.spectrumData : undefined} greyOutUnii={this.state.filterUNII} />
      </PageSection>)
  }
}

export { Analysis };
