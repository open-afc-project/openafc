import * as React from "react";
import { PageSection, CardHead, CardBody, Card, Title, Expandable, Alert, Modal, Button, ClipboardCopy, ClipboardCopyVariant, AlertActionCloseButton } from "@patternfly/react-core";
import "react-measure";
import { MapContainer, MapProps } from "../Components/MapContainer";
import { ExclusionZoneResult, GeoJson, ExclusionZoneRequest, } from "../Lib/RatApiTypes";
import { runExclusionZone, cacheItem, getCacheItem, guiConfig } from "../Lib/RatApi";
import { Limit } from "../Lib/Admin"
import { Timer } from "../Components/Timer"
import { ExclusionZoneForm } from "./ExclusionZoneForm";
import DownloadContents from "../Components/DownloadContents";
import { logger } from "../Lib/Logger";
import LoadLidarBounds from "../Components/LoadLidarBounds";
import { AnalysisProgress } from "../Components/AnalysisProgress";

/**
 * ExclusionZone.tsx: Page used for graphical ExclusionZone. Includes map display, channels, and spectrums
 * author: Sam Smucny
 */

/**
 * Initial map properties
 */
const mapProps: MapProps = {
  geoJson: {
    type: "FeatureCollection",
    features: []
  },
  center: {
    lat: 20,
    lng: 180
  },
  mode: "Exclusion",
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
 * Page level component for running Exclusion Zone Analyses
 */
class ExclusionZone extends React.Component<{},
  {
    mapState: MapState,
    mapCenter: {
      lat: number,
      lng: number
    },
    results?: ExclusionZoneResult,

    /**
     * KML file that can be downloaded for more fidelity of results
     */
    kml?: Blob,

    showParams: boolean,
    messageType: "None" | "Info" | "Warn" | "Error",
    messageTitle: string,
    messageValue: string,
    extraWarning?: string,
    extraWarningTitle?: string,
    isModalOpen: boolean,
    progress: {
      percent: number,
      message: string
    },
    canCancelTask: boolean,
    limit: Limit
  }> {

  private styles: Map<string, any>;
  private paramValue?: ExclusionZoneRequest;
  private formUpdateCallback?: (val: ExclusionZoneRequest) => void

  /**
   * Function used to cancel a long running task
   */
  cancelTask?: () => void;

  handleJsonChange: (value: any) => void;

  constructor(props: any) {
    super(props);

    const apiLimit = props.limit.kind === "Success" ? props.limit.result : new Limit(false, 18);

    // set the default start state, but load from cache if available
    this.state = {
      limit: apiLimit,
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
      messageTitle: "",
      messageValue: "",
      extraWarning: undefined,
      extraWarningTitle: undefined,
      messageType: "None",
      isModalOpen: false,
      progress: {
        percent: 0,
        message: "No Task Started"
      },
      canCancelTask: false
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
      [ "BLDB", { fillOpacity: 0, strokeColor: "blue" } ]
    ]);
  }

  componentDidMount() {
    const st = getCacheItem("ExclusionZoneStateCache");
    if (st !== undefined)
      this.setState(st);
  }

  componentWillUnmount() {
    // before removing object, let's cache the state in case we want to come back
    const state: any = this.state;
    state.messageType = "None";

    // cancel running task
    this.cancelTask && this.cancelTask();

    cacheItem("ExclusionZoneStateCache", state);
  }

  private setMapState(obj: any) {
    this.setState({ mapState: Object.assign(this.state.mapState, obj) });
  }

  /**
   * Run an Exclusion Zone analysis on the AFC Engine
   * @param params Parameters to send to AFC Engine
   */
  private async runExclusionZone(params: ExclusionZoneRequest) {

    // check AGL settings and possibly truncate
    if (params.heightType === "AGL" && params.height - params.heightUncertainty < 1) {
      // modify if height is not 1m above terrain height
      const minHeight = 1;
      const maxHeight = params.height + params.heightUncertainty;
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
      params.height = newHeight;
      params.heightUncertainty = newUncertainty;
      logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
      this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m`});
    }

    this.setState({
      messageType: "Info",
      messageTitle: "Working...",
      messageValue: "Your request is being processed",
      progress: {
        percent: 0,
        message: "Submitting..."
      }
    })

    let taskCanceled = false;
    this.cancelTask = () => {
      taskCanceled = true;
      this.cancelTask = undefined;
      this.setState({ canCancelTask: false });
    }
    this.setState({ canCancelTask: true });

    await runExclusionZone(params, () => taskCanceled, (prog) => this.setState({ progress: prog }), (kml: Blob) => this.setState({ kml: kml })).then(res => {
      if (res.kind === "Success") {
        if (res.result.geoJson.features.length !== 1 || res.result.geoJson.features[0].properties.kind !== "ZONE")
        {
          this.setState({
            messageType: "Error",
            messageTitle: "Response Error",
            messageValue: "The result value from the analysis was malformed.",
            canCancelTask: false
          });
          // short circuit
          return;
        }

        this.setState({
          results: res.result,
          messageType: res.result.statusMessageList.length ? "Warn" : "None",
          messageTitle: res.result.statusMessageList.length ? "Status messages" : "",
          messageValue: res.result.statusMessageList.length ? res.result.statusMessageList.join("\n") : "",
          mapCenter: {
            lat: res.result.geoJson.features[0].properties.lat,
            lng: res.result.geoJson.features[0].properties.lon
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

        // error in running ExclusionZone
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
      const params: ExclusionZoneRequest = JSON.parse(value) as ExclusionZoneRequest;
      if (this.formUpdateCallback)
        this.formUpdateCallback(params);
    } catch (e) {
      logger.error("Pasted value is not valid JSON");
    }
  }

  private copyPaste(formData: ExclusionZoneRequest, updateCallback: (v: ExclusionZoneRequest) => void) {
    this.formUpdateCallback = updateCallback;
    this.paramValue = formData;
    this.setState({isModalOpen: true});
  }

  private getParamsText = () => this.paramValue ? JSON.stringify(this.paramValue) : "";
  render() {

    const toggleParams = () => this.setState({ showParams: !this.state.showParams });
    const runAnalysis = (x: ExclusionZoneRequest) => this.runExclusionZone(x);

    return (
      <PageSection id="exclusion-contour-page">
        <Card>
          <CardHead>
            <Title size="lg">Run Exclusion Zone</Title>
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
              <ExclusionZoneForm limit={this.state.limit} onSubmit={runAnalysis} onCopyPaste={(formData: ExclusionZoneRequest, updateCallback: (v: ExclusionZoneRequest) => void) => this.copyPaste(formData, updateCallback)} />
              {this.state.canCancelTask && <>
                {" "}<Button variant="secondary" onClick={this.cancelTask}>Cancel</Button>
              </>
              }
              {" "}<LoadLidarBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />
            </Expandable>
          </CardBody>
        </Card>
        <br/>
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
        <br/>
        <div style={{ width: "100%" }}>
          <MapContainer 
            mode="Exclusion"
            geoJson={this.state.mapState.val} 
            styles={this.styles} 
            center={this.state.mapCenter} 
            zoom={mapProps.zoom} 
            versionId={this.state.mapState.versionId} />
        </div>
        {
          (this.state.results && this.state.kml) &&
          <DownloadContents contents={() => this.state.kml} fileName="results.kmz" />
        }
      </PageSection>)
  }
}

export { ExclusionZone };
