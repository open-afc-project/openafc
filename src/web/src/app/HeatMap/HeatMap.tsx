import * as React from "react";
import {
  PageSection,
  CardHead,
  CardBody,
  Card,
  Title,
  Expandable,
  FormGroup,
  InputGroup,
  TextInput,
  InputGroupText,
  Gallery,
  GalleryItem,
  Alert,
  AlertActionCloseButton,
  Button,
  Modal,
  ClipboardCopy,
  ClipboardCopyVariant
} from "@patternfly/react-core";
import { MapContainer, MapProps } from "../Components/MapContainer";
import { HeatMapResult, Bounds, GeoJson, error, success, HeatMapRequest, IndoorOutdoorType, HeatMapFsIdType, HeatMapAnalysisType } from "../Lib/RatApiTypes";
import { cacheItem, getCacheItem, heatMapRequestObject } from "../Lib/RatApi";
import { HeatMapForm, HeatMapFormData } from "./HeatMapForm";
import { logger } from "../Lib/Logger";
import { ColorLegend, getColor } from "./ColorLegend";
import { RegionSize } from "./RegionSize";
import { AnalysisProgress } from "../Components/AnalysisProgress";
import LoadLidarBounds from "../Components/LoadLidarBounds";
import LoadRasBounds from "../Components/LoadRasBounds";
import { Limit } from "../Lib/Admin";
import { spectrumInquiryRequestByString } from "../Lib/RatAfcApi"
import { VendorExtension, AvailableSpectrumInquiryRequest } from "../Lib/RatAfcTypes";
import { fromPer } from "../Lib/Utils";

/**
 * HeatMap.tsx: Page used for graphical heatmap. Includes map display, channels, and spectrums
 * author: Sam Smucny
 */

/**
 * Initial value of map properties
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
  mode: "Heatmap",
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
 * Page level component where user can run heat map analyses
 */
class HeatMap extends React.Component<{},
  {
    mapState: MapState,
    selectionRectangle?: Bounds,
    spacing?: number,
    mapCenter: {
      lat: number,
      lng: number
    },
    results?: HeatMapResult,
    showParams: boolean,
    canCancelTask: boolean,
    messageType?: "info" | "warning" | "danger",
    messageTitle: string,
    messageValue: string,
    extraWarning?: string,
    extraWarningTitle?: string,
    progress: {
      percent: number,
      message: string
    },
    isModalOpen: boolean,
    limit: Limit,
    rulesetIds: string[],
    submittedAnalysisType?: HeatMapAnalysisType
  }> {

  handleJsonChange: (value: any) => void;

  /**
   * Function used to cancel a long running task
   */
  cancelTask?: () => void;

  minItoN: number;
  maxItoN: number;
  ItoNthreshold: number;
  private paramValue?: HeatMapRequest;
  private formUpdateCallback?: (val: HeatMapRequest) => void

  constructor(props: any) {
    super(props);

    const apiLimit = props.limit.kind === "Success" ? props.limit.result : new Limit(false, false, 18, 18);
    const rulesetIds = props.rulesetIds.kind === "Success" ? props.rulesetIds.result : [];
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
      messageType: undefined,
      selectionRectangle: undefined,
      canCancelTask: false,
      progress: {
        percent: 0,
        message: "No Task Started"
      },
      isModalOpen: false,
      rulesetIds: rulesetIds
    };

    this.minItoN = Number.NaN;
    this.maxItoN = Number.NaN;
    this.ItoNthreshold = Number.NaN;

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
  }

  componentDidMount() {
    const st = getCacheItem("heatmapStateCache");
    if (st !== undefined) {
      this.ItoNthreshold = st.ItoNThreshold;
      this.setState(st);
    }
  }

  componentWillUnmount() {
    // before removing object, let's cache the state in case we want to come back
    const state: any = this.state;
    state.messageType = undefined;
    state.ItoNThreshold = this.ItoNthreshold;

    // cancel running task
    this.cancelTask && this.cancelTask();

    cacheItem("heatmapStateCache", state);
  }

  private setN = (rect: Bounds, n: number) => {
    rect.north = n; return rect;
  }
  private setS = (rect: Bounds, s: number) => {
    rect.south = s; return rect;
  }
  private setE = (rect: Bounds, e: number) => {
    rect.east = e; return rect;
  }
  private setW = (rect: Bounds, w: number) => {
    rect.west = w; return rect;
  }

  private validN = (r?: Bounds) => r && r.north <= 90 && r.north >= -90;
  private validS = (r?: Bounds) => r && r.south <= 90 && r.south >= -90;
  private validE = (r?: Bounds) => r && r.east <= 180 && r.east >= -180;
  private validW = (r?: Bounds) => r && r.west <= 180 && r.west >= -180;
  private validSpacing = (s?: number) => !!s && s > 0;

  private setDir = (setter: (r: Bounds, i: number) => Bounds) => (s: string) => {
    if (!this.state.selectionRectangle) {
      const val = Number.parseFloat(s);
      this.setState({
        selectionRectangle: setter({
          north: 0,
          south: 0,
          east: 0,
          west: 0
        }, val)
      })
    } else {
      const rect = this.state.selectionRectangle;
      setter(rect, Number.parseFloat(s));
      this.setState({ selectionRectangle: rect })
      this.setMapState({ versionId: this.state.mapState.versionId + 1 })
    }
  }

  private setMapState(obj: any) {
    this.setState({ mapState: Object.assign(this.state.mapState, obj) });
  }


  private onRectUpdate(rect: any): void {
    this.setState({ selectionRectangle: rect.getBounds().toJSON() })
  }

  private formSubmitted(form: HeatMapFormData) {
    if (!this.state.selectionRectangle)
      return Promise.resolve(error("No Bounding Box Specified", undefined, "Specifiy a bounding box to submit a request"));
    const validBounds = [...([
      this.validN,
      this.validE,
      this.validS,
      this.validW
    ]
      .map(f => f(this.state.selectionRectangle))),
    this.validSpacing(this.state.spacing)]
      .reduce((p, c) => p && c);

    if (!validBounds)
      return Promise.resolve(error("Invalid Bounding Box", undefined, "One or more parameters for the bounding box are invalid"));

    // check AGL settings and possibly truncate
    if (form.indoorOutdoor.kind === IndoorOutdoorType.BUILDING) {
      let stringHeight = "";

      // check indoor
      const formIn = form.indoorOutdoor.in;
      if (formIn.heightType === "AGL" && formIn.height - formIn.heightUncertainty < 1) {
        const minHeight = 1;
        const maxHeight = formIn.height + formIn.heightUncertainty;
        if (maxHeight < minHeight) {
          this.setState({
            messageType: "danger",
            messageTitle: "Invalid Height",
            messageValue: `The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height for inside is ${maxHeight}m`
          });
          return;
        }
        const newHeight = (minHeight + maxHeight) / 2;
        const newUncertainty = newHeight - minHeight;
        form.indoorOutdoor.in.height = newHeight;
        form.indoorOutdoor.in.heightUncertainty = newUncertainty;
        stringHeight = `${newHeight}+/-${newUncertainty}m for indoors`;
      }

      // check outdoor
      const formOut = form.indoorOutdoor.out;
      if (formOut.heightType === "AGL" && formOut.height - formOut.heightUncertainty < 1) {
        const minHeight = 1;
        const maxHeight = formOut.height + formOut.heightUncertainty;
        if (maxHeight < minHeight) {
          this.setState({
            messageType: "danger",
            messageTitle: "Invalid Height",
            messageValue: `The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height for outside is ${maxHeight}m`
          });
          return;
        }
        const newHeight = (minHeight + maxHeight) / 2;
        const newUncertainty = newHeight - minHeight;
        form.indoorOutdoor.out.height = newHeight;
        form.indoorOutdoor.out.heightUncertainty = newUncertainty;
        if (stringHeight !== "") {
          stringHeight += " and ";
        }
        stringHeight += `${newHeight}+/-${newUncertainty}m for outdoors`;
      }

      // display warning if something was changed
      if (stringHeight !== "") {
        logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
        this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${stringHeight}` });
      }


    } else {
      if (form.indoorOutdoor.heightType === "AGL" && form.indoorOutdoor.height - form.indoorOutdoor.heightUncertainty < 1) {
        // modify if height is not 1m above terrain height
        const minHeight = 1;
        const maxHeight = form.indoorOutdoor.height + form.indoorOutdoor.heightUncertainty;
        if (maxHeight < minHeight) {
          this.setState({
            messageType: "danger",
            messageTitle: "Invalid Height",
            messageValue: `The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height is ${maxHeight}m`
          });
          return;
        }
        const newHeight = (minHeight + maxHeight) / 2;
        const newUncertainty = newHeight - minHeight;
        form.indoorOutdoor.height = newHeight;
        form.indoorOutdoor.heightUncertainty = newUncertainty;
        logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
        this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m` });
      }
    }

    logger.info("running heat map: ", this.state);
    this.setState({ messageType: "info", messageTitle: "Processing", messageValue: "", progress: { percent: 0, message: "Submitting..." }, submittedAnalysisType: form.analysis });

    //Build request
    let extension = generateVendorExtension(form, this.state.spacing!, this.state.selectionRectangle!);

    let dummyRequest = heatMapRequestObject(extension, form.certificationId, form.serialNumber)

    return spectrumInquiryRequestByString("1.4", JSON.stringify(dummyRequest))
      .then((r) => {
        logger.info(r);
        if (r.kind === "Success") {
          const response = r.result.availableSpectrumInquiryResponses[0];
          if (response.vendorExtensions
            && response.vendorExtensions.length > 0
            && response.vendorExtensions.findIndex(x => x.extensionId == "openAfc.mapinfo") >= 0) {
            //Get the KML file and load it into the state.kml parameters; get the GeoJson if present
            let kml_filename = response.vendorExtensions.find(x => x.extensionId == "openAfc.mapinfo")?.parameters["kmzFile"];
            if (!!kml_filename) {
              //this.setKml(kml_filename)
            }
            let geoJsonData = response.vendorExtensions.find(x => x.extensionId == "openAfc.mapinfo")?.parameters["geoJsonFile"];
            let centerLat = (this.state.selectionRectangle!.north + this.state.selectionRectangle!.south) / 2
            let centerLon = (this.state.selectionRectangle!.east + this.state.selectionRectangle!.west) / 2

            if (!!geoJsonData) {
              let geojson = JSON.parse(geoJsonData);
              this.ItoNthreshold = geojson.threshold;
              this.maxItoN = geojson.maxItoN;
              this.minItoN = geojson.minItoN;
              //this.heatMapStyle.set("HMAP", (feature: any) => ({ strokeWeight: 0, fillColor: feature.getProperty("ItoN"), zIndex: 5 }));
              this.heatMapStyle.set("HMAP", (feature: any) => ({ strokeWeight: 0, fillColor: feature.getProperty("fill"), "fill-opacity": feature.getProperty("fill-opacity"), zIndex: 5 }));
              this.setMapState({ val: geojson.geoJson as GeoJson, valid: true, versionId: this.state.mapState.versionId + 1 })
              this.setState({ canCancelTask: false, messageType: undefined, messageTitle: "", messageValue: "", mapCenter: { lng: centerLon, lat: centerLat } });

            }
          }
          return success("Request completed successfully");
        } else {
          logger.error(r);
          this.setState({ canCancelTask: false, messageType: "danger", messageTitle: "An error occured while running your request", messageValue: r.description })
          return success("");
        }
      })
      .catch((e) => {
        this.setState({ canCancelTask: false, messageType: "danger", messageTitle: "An unexpected error occured", messageValue: JSON.stringify(e) });
        return error("Your request was unable to be processed", undefined, e);
      })
  }

  private heatMapStyle = new Map<string, React.CSSProperties | ((feature: any) => React.CSSProperties)>([[
    "HMAP",
    (feature: any) => ({ strokeWeight: 0, fillColor: feature.getProperty("fill"), "fill-opacity": feature.getProperty("fill-opacity"), zIndex: 5 })
  ], [
    "BLDB",
    // @ts-ignore
    { strokeColor: "blue", fillOpacity: 0, zIndex: 0 }
  ]])

  private setConfig(value: string) {
    try {
      const params: HeatMapRequest = JSON.parse(value) as HeatMapRequest;
      this.setState({ selectionRectangle: params.bounds, spacing: params.spacing });
      this.setMapState({ versionId: this.state.mapState.versionId + 1 });
      if (this.formUpdateCallback)
        this.formUpdateCallback(params);
    } catch (e) {
      logger.error("Pasted value is not valid JSON");
    }
  }

  private copyPaste(formData: HeatMapRequest, updateCallback: (v: HeatMapRequest) => void) {
    this.formUpdateCallback = updateCallback;
    formData.bounds = this.state.selectionRectangle!;
    formData.spacing = this.state.spacing!;
    this.paramValue = formData;
    this.setState({ isModalOpen: true });
  }

  private getParamsText = () => this.paramValue ? JSON.stringify(this.paramValue) : "";

  render() {

    const toggleParams = () => this.setState({ showParams: !this.state.showParams });

    return (
      <PageSection id="heat-map-page">
        <Card>
          <CardHead>
            <Title size="lg">Run Heat Map</Title>
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
              <Title size="md">Heat Map Bounds</Title>
              <RegionSize bounds={this.state.selectionRectangle} />
              <Gallery gutter="sm">
                <GalleryItem>
                  <FormGroup label="North"
                    fieldId="horizontal-form-north">
                    <InputGroup>
                      <TextInput
                        value={this.state.selectionRectangle && this.state.selectionRectangle.north}
                        onChange={this.setDir(this.setN)}
                        type="number"
                        step="any"
                        id="horizontal-form-north"
                        name="horizontal-form-north"
                        isValid={this.state.selectionRectangle && this.validN(this.state.selectionRectangle)}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                  <FormGroup label="South"
                    fieldId="horizontal-form-south">
                    <InputGroup>
                      <TextInput
                        value={this.state.selectionRectangle && this.state.selectionRectangle.south}
                        onChange={this.setDir(this.setS)}
                        type="number"
                        step="any"
                        id="horizontal-form-south"
                        name="horizontal-form-south"
                        isValid={this.state.selectionRectangle && this.validS(this.state.selectionRectangle)}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                  <FormGroup label="East"
                    fieldId="horizontal-form-east">
                    <InputGroup>
                      <TextInput
                        value={this.state.selectionRectangle && this.state.selectionRectangle.east}
                        onChange={this.setDir(this.setE)}
                        type="number"
                        step="any"
                        id="horizontal-form-east"
                        name="horizontal-form-east"
                        isValid={this.state.selectionRectangle && this.validE(this.state.selectionRectangle)}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                  <FormGroup label="West"
                    fieldId="horizontal-form-west">
                    <InputGroup>
                      <TextInput
                        value={this.state.selectionRectangle && this.state.selectionRectangle.west}
                        onChange={this.setDir(this.setW)}
                        type="number"
                        step="any"
                        id="horizontal-form-west"
                        name="horizontal-form-west"
                        isValid={this.state.selectionRectangle && this.validW(this.state.selectionRectangle)}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>degrees</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
                <GalleryItem>
                  <FormGroup label="RLAN Spacing" fieldId="horizontal-form-spacing">
                    <InputGroup>
                      <TextInput
                        value={this.state.spacing}
                        onChange={x => this.setState({ spacing: Number(x) })}
                        type="number"
                        step="any"
                        id="horizontal-form-fsid"
                        name="horizontal-form-fsid"
                        isValid={this.validSpacing(this.state.spacing)}
                        style={{ textAlign: "right" }}
                      />
                      <InputGroupText>meters</InputGroupText>
                    </InputGroup>
                  </FormGroup>
                </GalleryItem>
              </Gallery>
              <br />
              <Title size="md">Simulation Parameters</Title>
              <br />
              <HeatMapForm limit={this.state.limit} rulesetIds={this.state.rulesetIds} onSubmit={(a: HeatMapFormData) => this.formSubmitted(a)} onCopyPaste={(form, callback) => this.copyPaste(form, callback)} />
              {this.state.canCancelTask && <>
                {" "}<Button variant="secondary" onClick={this.cancelTask}>Cancel</Button>
              </>
              }
              {" "}<LoadLidarBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />
              <LoadRasBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />

            </Expandable>
          </CardBody>
        </Card>
        {
          this.state.extraWarning &&
          <Alert title={this.state.extraWarningTitle || "Warning"} variant="warning" action={<AlertActionCloseButton onClose={() => this.setState({ extraWarning: undefined, extraWarningTitle: undefined })} />}>
            <pre>{this.state.extraWarning}</pre>
          </Alert>
        }
        {this.state.messageType && (<><br />
          <Alert
            variant={this.state.messageType}
            title={this.state.messageTitle || ""}
            action={this.state.messageType !== "info" && <AlertActionCloseButton onClose={() => this.setState({ messageType: undefined })} />}
          >
            <pre>{this.state.messageValue}</pre>
            {this.state.messageType === "info" && <AnalysisProgress percent={this.state.progress.percent} message={this.state.progress.message} />}
          </Alert>
        </>)}
        <br />
        {!!this.state.submittedAnalysisType &&
          <ColorLegend threshold={this.ItoNthreshold} analysisType={this.state.submittedAnalysisType} />
        }
        <br />
        <div style={{ width: "100%" }}>
          <MapContainer
            mode="Heatmap"
            selectionRectangle={correctBounds(this.state.selectionRectangle)}
            onRectUpdate={(rect: any) => this.onRectUpdate(rect)}
            geoJson={this.state.mapState.val}
            center={this.state.mapCenter}
            zoom={mapProps.zoom}
            styles={this.heatMapStyle}
            versionId={this.state.mapState.versionId} />
        </div>
      </PageSection>)
  }
}

// if any of the bounds are not defined then propogate the undefined
const correctBounds = (rect?: Bounds): Bounds | undefined => (!rect ||
  [rect.north, rect.south, rect.east, rect.west]
    .map(x => !Number.isFinite(x))
    .reduce((a, b) => a || b)
  ? undefined : rect
)


export { HeatMap };

function generateVendorExtension(form: HeatMapFormData, spacing: number, location: Bounds) {
  let v: VendorExtension = {
    extensionId: "openAfc.heatMap",
    parameters: {
      type: "heatmap",
      inquiredChannel: form.inquiredChannel,
      MinLon: location.west,
      MaxLon: location.east,
      MinLat: location.south,
      MaxLat: location.north,
      RLANSpacing: spacing,
      analysis: form.analysis,
      fsIdType: form.fsIdType,
    }
  }

  if (form.fsIdType === HeatMapFsIdType.Single) {
    v.parameters["fsId"] = form.fsId;
  }

  switch (form.indoorOutdoor.kind) {
    case IndoorOutdoorType.INDOOR:
      v.parameters.IndoorOutdoorStr = IndoorOutdoorType.INDOOR;
      v.parameters.RLANIndoorEIRPDBm = form.indoorOutdoor.EIRP
      v.parameters.RLANIndoorHeightType = form.indoorOutdoor.heightType;
      v.parameters.RLANIndoorHeight = form.indoorOutdoor.height
      v.parameters.RLANIndoorHeightUncertainty = form.indoorOutdoor.heightUncertainty;
      break;
    case IndoorOutdoorType.OUTDOOR:
      v.parameters.IndoorOutdoorStr = IndoorOutdoorType.OUTDOOR;
      v.parameters.RLANOutdoorEIRPDBm = form.indoorOutdoor.EIRP
      v.parameters.RLANOutdoorHeightType = form.indoorOutdoor.heightType;
      v.parameters.RLANOutdoorHeight = form.indoorOutdoor.height
      v.parameters.RLANOutdoorHeightUncertainty = form.indoorOutdoor.heightUncertainty;
      break;
    case IndoorOutdoorType.ANY:
      // Is this used?
      break;
    case IndoorOutdoorType.BUILDING:
      v.parameters.IndoorOutdoorStr = IndoorOutdoorType.BUILDING;
      v.parameters.RLANIndoorEIRPDBm = form.indoorOutdoor.in.EIRP
      v.parameters.RLANIndoorHeightType = form.indoorOutdoor.in.heightType;
      v.parameters.RLANIndoorHeight = form.indoorOutdoor.in.height
      v.parameters.RLANIndoorHeightUncertainty = form.indoorOutdoor.in.heightUncertainty;
      v.parameters.RLANOutdoorEIRPDBm = form.indoorOutdoor.out.EIRP
      v.parameters.RLANOutdoorHeightType = form.indoorOutdoor.out.heightType;
      v.parameters.RLANOutdoorHeight = form.indoorOutdoor.out.height
      v.parameters.RLANOutdoorHeightUncertainty = form.indoorOutdoor.out.heightUncertainty;

  }

  return v;

}

