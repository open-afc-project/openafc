import * as React from "react";
import { PageSection, Title, Card, CardBody, Alert, AlertActionCloseButton, Button, Modal, TextArea } from "@patternfly/react-core";
import { spectrumInquiryRequest, downloadMapData, spectrumInquiryRequestByString } from "../Lib/RatAfcApi";
import { getCacheItem, cacheItem } from "../Lib/RatApi";
import { ResError, AFCConfigFile, RatResponse, error, ChannelData, GeoJson } from "../Lib/RatApiTypes";
import { SpectrumDisplayAFC, SpectrumDisplayLineAFC } from "../Components/SpectrumDisplay";
import { ChannelDisplay, emptyChannels } from "../Components/ChannelDisplay";
import { JsonRawDisp } from "../Components/JsonRawDisp";
import { MapContainer, MapProps } from "../Components/MapContainer";
import DownloadContents from "../Components/DownloadContents";
import LoadLidarBounds from "../Components/LoadLidarBounds";
import LoadRasBounds from "../Components/LoadRasBounds";
import { AvailableSpectrumInquiryRequest, AvailableSpectrumInquiryResponse, AvailableChannelInfo, Ellipse, Point, AvailableSpectrumInquiryResponseMessage } from "../Lib/RatAfcTypes";
import { Timer } from "../Components/Timer";
import { logger } from "../Lib/Logger";
import { RatAfcForm } from "./RatAfcForm";
import { clone } from "../Lib/Utils";
import Measure from "react-measure";
import { Limit } from "../Lib/Admin";
import { rotate, meterOffsetToDeg } from "../Lib/Utils"


/**
 * RatAfc.tsx: virtual AP page for AP-AFC specification
 * author: Sam Smucny
 */

/**
 * Properties for RatAfc
 */
interface RatAfcProps {
    afcConfig: RatResponse<AFCConfigFile>
    limit: RatResponse<Limit>,
    rulesetIds: RatResponse<string[]>
}

/**
 * State for RatAfc
 */
interface RatAfcState {
    response?: AvailableSpectrumInquiryResponse,
    status?: "Success" | "Info" | "Error",
    err?: ResError,
    extraWarning?: string,
    extraWarningTitle?: string,
    minEirp: number,
    maxEirp: number
    width: number,
    mapState: MapState,
    mapCenter: {
        lat: number,
        lng: number
    },
    kml?: Blob,
    includeMap: boolean,
    clickedMapPoint?: Point,
    fullJsonResponse?: string,
    redChannels?: AvailableChannelInfo[],
    blackChannels?: AvailableChannelInfo[],
    showSendDirect: boolean, // shows the send direct box used for legacy support - turned off (false) for now
    sendDirectModalOpen: boolean,
    sendDirectValue?: string
}

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
 * generates channel data to be displayed from AP-AFC `AvailableChannelInfo` results
 * @param channelClasses AP-AFC formatted channels
 * @param minEirp min eirp to use for coloring
 * @param maxEirp maz eirp to use for coloring
 */
const generateChannelData = (channelClasses: AvailableChannelInfo[], minEirp: number, maxEirp: number,
    blackChannels?: AvailableChannelInfo[], redChannels?: AvailableChannelInfo[]): ChannelData[] => {
    let channelData = clone(emptyChannels);
    channelClasses.forEach((channelClass) =>
        channelData.forEach((channelGroup) =>
            channelGroup.channels.forEach((channel) => {
                for (let i = 0; i < channelClass.channelCfi.length; i++) {
                    if (channel.name === String(channelClass.channelCfi[i])) {
                        channel.maxEIRP = channelClass.maxEirp[i];
                        // RAS Exclusion will produce null channel EIRP
                        if (channel.maxEIRP == null || channel.maxEIRP === undefined) {
                            channel.color = "black"
                        } else if (channel.maxEIRP >= maxEirp) {
                            channel.color = "green";
                        } else if (channel.maxEIRP >= minEirp) {
                            channel.color = "yellow";
                        } else {
                            channel.color = "red";
                        }
                    }
                }
            }))
    );

    if (!!blackChannels) {
        blackChannels.forEach((channelClass) =>
            channelData.forEach((channelGroup) =>
                channelGroup.channels.forEach((channel) => {
                    for (let i = 0; i < channelClass.channelCfi.length; i++) {
                        if (channel.name === String(channelClass.channelCfi[i])) {
                            channel.color = "black";
                            channel.maxEIRP = undefined;
                        }
                    }
                })
            )
        )
    }


    if (!!redChannels) {
        redChannels.forEach((channelClass) =>
            channelData.forEach((channelGroup) =>
                channelGroup.channels.forEach((channel) => {
                    for (let i = 0; i < channelClass.channelCfi.length; i++) {
                        if (channel.name === String(channelClass.channelCfi[i])) {
                            channel.color = "red";
                            channel.maxEIRP = undefined;
                        }
                    }
                })
            )
        )
    }

    return channelData;
}

// There is another rasterizeEllipse that uses the wrong Ellipse type so reimpliment here
export const rasterizeEllipse = (e: Ellipse, n: number): [number, number][] => {
    const omega = 2 * Math.PI / n; // define the angle increment in angle to use in raster
    return Array(n + 1).fill(undefined).map((_, i) => {
        const alpha = omega * i; // define angle to transform for this step
        // use ellipse parameterization to generate offset point before rotation
        return ([e.majorAxis * Math.sin(alpha), e.minorAxis * Math.cos(alpha)]) as [number, number];
    })
        .map(rotate(e.orientation * Math.PI / 180))    // rotate offset in meter coordinates by orientation (- sign gives correct behavior with sin/cos)
        .map(meterOffsetToDeg(e.center.latitude))   // transform offset point in meter coordinates to degree offset
        .map(([dLng, dLat]) => [dLng + e.center.longitude, dLat + e.center.latitude])   // add offset to center point
};


/**
 * RatAfc component class
 */
export class RatAfc extends React.Component<RatAfcProps, RatAfcState> {
    constructor(props: RatAfcProps) {
        super(props);

        if (props.afcConfig.kind === "Error") {
            this.state = {
                width: 500,
                minEirp: 0,
                maxEirp: 30,
                extraWarning: undefined,
                extraWarningTitle: undefined,
                status: "Error",
                err: error("AFC config was not loaded properly. Try refreshing the page."),
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
                includeMap: false,
                clickedMapPoint: {
                    latitude: 40,
                    longitude: -100
                },
                sendDirectModalOpen: false,
                showSendDirect: false
            }
        } else {
            this.state = {
                width: 500,
                minEirp: props.afcConfig.result.minEIRP,
                maxEirp: props.afcConfig.result.maxEIRP,
                extraWarning: undefined,
                extraWarningTitle: undefined,
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
                includeMap: props.afcConfig.kind === "Success" ? props.afcConfig.result.enableMapInVirtualAp ?? false : false,
                clickedMapPoint: {
                    latitude: 40,
                    longitude: -100
                },
                sendDirectModalOpen: false,
                showSendDirect: false
            };
        }
        this.changeMapLocationChild = React.createRef();
    }

    private changeMapLocationChild: any;

    private styles: Map<string, any> = new Map([
        ["BLDB", { fillOpacity: 0, strokeColor: "blue" }],
        ["RLAN", { strokeColor: "blue", fillColor: "lightblue" }]
    ]);

    componentDidMount() {
        const st = getCacheItem("ratAfcCache") as RatAfcState;
        if (st !== undefined) {
            st.includeMap = this.props.afcConfig.kind === "Success" ? this.props.afcConfig.result.enableMapInVirtualAp ?? false : false
            if (st.mapState.val.features.length <= 646) {
                this.setState(st);
            } else {
                this.setState({
                    ...st, mapState: {
                        isModalOpen: false,
                        val: mapProps.geoJson,
                        text: "", valid: false,
                        dimensions: { width: 0, height: 0 },
                        versionId: 0
                    },
                    extraWarningTitle: "Google Map API Error",
                    extraWarning: "Due to a limitation of the Google Maps API, map data could not be saved. Run again to see map data."
                });
            }
        }
    }

    componentWillUnmount() {
        // before removing object, let's cache the state in case we want to come back
        const state = this.state;
        cacheItem("ratAfcCache", state);
    }

    private setMapState(obj: any) {
        this.setState({ mapState: Object.assign(this.state.mapState, obj) });
    }

    private setKml(kml: Blob) {
        this.setState({ kml: kml })
    }

    //Leaving this in for later addition of marker move functionality
    private onMarkerUpdate(lat: number, lon: number) {

        var newGeoJson = { ...this.state.mapState.val };

        this.setState({ clickedMapPoint: { latitude: lat, longitude: lon } })
        this.setMapState({ val: newGeoJson, versionId: this.state.mapState.versionId + 1 });
        this.changeMapLocationChild.current.setEllipseCenter({ latitude: lat, longitude: lon })
    }

    /**
     * make a request to AFC Engine
     * @param request request to send
     */
    private sendRequest = (request: AvailableSpectrumInquiryRequest) => {
        // make api call
        this.setState({ status: "Info" });
        const rlanLoc = this.getLatLongFromRequest(request);
        this.setState({
            mapCenter: rlanLoc,
            clickedMapPoint: { latitude: rlanLoc.lat, longitude: rlanLoc.lng }
        });
        return spectrumInquiryRequest(request)
            .then(resp => this.processResponse(resp, request, rlanLoc));
    }

    private getLatLongFromRequest(request: AvailableSpectrumInquiryRequest): { lat: number, lng: number } | undefined {
        if (request.location.ellipse) {
            return { lat: request.location.ellipse.center.latitude, lng: request.location.ellipse.center.longitude }
        } else if (request.location.linearPolygon) {
            return {
                lat: request.location.linearPolygon.outerBoundary.map(x => x.latitude).reduce((a, b) => (a + b)) / request.location.linearPolygon.outerBoundary.length,
                lng: request.location.linearPolygon.outerBoundary.map(x => x.longitude).reduce((a, b) => (a + b)) / request.location.linearPolygon.outerBoundary.length
            }
        }
        else if (request.location.radialPolygon) {
            return {
                lat: request.location.radialPolygon.center.latitude,
                lng: request.location.radialPolygon.center.longitude
            }
        }
        else
            return undefined
    }

    private processResponse(resp: RatResponse<AvailableSpectrumInquiryResponseMessage>, request?: AvailableSpectrumInquiryRequest,rlanLoc? ) {
        if (resp.kind == "Success") {
            const response = resp.result.availableSpectrumInquiryResponses[0];
            if (response.response.responseCode === 0) {
                const minEirp = request?.minDesiredPower || this.state.minEirp;

                if (!!rlanLoc){
                    this.setState({
                        status: "Success",
                        response: response,
                        mapCenter:rlanLoc,
                        clickedMapPoint: { latitude: rlanLoc.lat, longitude: rlanLoc.lng },
                        minEirp: minEirp,
                        fullJsonResponse: JSON.stringify(resp.result, (k, v) => (k == "kmzFile") ? "Removed binary data" : v, 2),
                    })
                }else{
                    this.setState({
                        status: "Success",
                        response: response,
                        minEirp: minEirp,
                        fullJsonResponse: JSON.stringify(resp.result, (k, v) => (k == "kmzFile") ? "Removed binary data" : v, 2),
                    })
                }

                if (response.vendorExtensions
                    && response.vendorExtensions.length > 0
                    && response.vendorExtensions.findIndex(x => x.extensionId == "openAfc.redBlackData") >= 0) {
                    let extraChannels = response.vendorExtensions.find(x => x.extensionId == "openAfc.redBlackData").parameters;
                    this.setState({ redChannels: extraChannels["redChannelInfo"], blackChannels: extraChannels["blackChannelInfo"] })
                } else {
                    this.setState({ redChannels: undefined, blackChannels: undefined })
                }


                if (this.state.includeMap
                    && response.vendorExtensions
                    && response.vendorExtensions.length > 0
                    && response.vendorExtensions.findIndex(x => x.extensionId == "openAfc.mapinfo") >= 0) {
                    //Get the KML file and load it into the state.kml parameters; get the GeoJson if present
                    let kml_filename = response.vendorExtensions.find(x => x.extensionId == "openAfc.mapinfo").parameters["kmzFile"];
                    let geoJson_filename = response.vendorExtensions.find(x => x.extensionId == "openAfc.mapinfo").parameters["geoJsonFile"];
                    this.setKml(kml_filename)
                    let geojson = JSON.parse(geoJson_filename);
                    if (request?.location.ellipse && geojson && geojson.geoJson) {

                        geojson.geoJson.features.push(
                            {
                                type: "Feature",
                                properties: {
                                    kind: "RLAN",
                                    FSLonLat: [
                                        this.state.mapCenter.lng,
                                        this.state.mapCenter.lat
                                    ]
                                },
                                geometry: {
                                    type: "Polygon",
                                    coordinates: [rasterizeEllipse(request.location.ellipse, 32)]
                                }
                            });

                    }
                    this.setMapState({ val: geojson.geoJson, valid: true, versionId: this.state.mapState.versionId + 1 })

                }
            }
        } else if (!resp.kind || resp.kind == "Error") {
            this.setState({ status: "Error", err: error(resp.description, resp.errorCode, resp.body), response: resp.body });
        }


    }

    private showDirectSendModal(b: boolean): void {
        this.setState({ sendDirectModalOpen: b })
    }

    /***
     * Sends a json string to an eariler version of the API directly (not using the controls on the page to 
     * generate the values).  Used to support legacy values when/if supported during transitions.  Uses the 
     * showSendDirect state property to hide/show
     */
    private sendDirect(jsonString: string | undefined) {
        if (!!jsonString) {
            this.setState({ sendDirectModalOpen: false, status: "Info" });
            return spectrumInquiryRequestByString("1.3", jsonString)
                .then(resp => this.processResponse(resp));
        }
    }


    render() {
        return (
            <PageSection id="ap-afc-page">
                <Title size="lg">RAT AFC AP</Title>
                <Card>
                    <CardBody>
                        <RatAfcForm ref={this.changeMapLocationChild}
                            limit={this.props.limit.kind == "Success" ? this.props.limit.result : new Limit(false, 0)}
                            config={this.props.afcConfig}
                            onSubmit={req => this.sendRequest(req)}
                            ellipseCenterPoint={!this.state.clickedMapPoint ? undefined : this.state.clickedMapPoint}
                            rulesetIds={this.props.rulesetIds.kind == "Success" ? this.props.rulesetIds.result : ['No rulesets']}
                        />
                        {this.state.showSendDirect && (
                            <>
                                <Button key="open-manualSend" variant="secondary" onClick={() => this.showDirectSendModal(true)}>Send 1.3 Request Data</Button>
                                <Modal key={"directSendModal"} title="Direct Send Request"
                                    isLarge={true}
                                    isOpen={this.state.sendDirectModalOpen}
                                    onClose={() =>  this.showDirectSendModal(false)}
                                    actions={[
                                        <Button key="sendDirect-close" variant="secondary" onClick={() => this.showDirectSendModal(false)}>Close</Button>,
                                        <Button key="sendDirect-send" variant="primary" onClick={() => this.sendDirect(this.state.sendDirectValue)}>Send</Button>
                                    ]} >
                                    <TextArea
                                        resizeOrientation="vertical"
                                        onChange={(text: string) => this.setState({ sendDirectValue: text })} aria-label="text area"></TextArea>
                                </Modal>
                            </>)}
                    </CardBody>
                </Card>
                <br />
                {
                    this.state.status === "Success" &&
                    <Alert title={"Success"} variant="success">
                        {"Request completed successfully."}
                    </Alert>
                }
                {
                    this.state.extraWarning &&
                    <Alert title={this.state.extraWarningTitle || "Warning"} variant="warning" action={<AlertActionCloseButton onClose={() => this.setState({ extraWarning: undefined, extraWarningTitle: undefined })} />}>
                        <pre>{this.state.extraWarning}</pre>
                    </Alert>
                }
                {
                    this.state.status === "Info" &&
                    <Alert title={"Processing"} variant="info">
                        {"Your request has been submitted. "}
                        <Timer />
                    </Alert>
                }
                {
                    this.state.status === "Error" &&
                    <Alert title={"Error: " + this.state.err?.errorCode} variant="danger">
                        <pre>{this.state.err?.description}</pre>
                        {
                            this.state.err?.body?.response?.supplementalInfo &&
                            <pre>{JSON.stringify(this.state.err?.body?.response?.supplementalInfo)}</pre>
                        }
                    </Alert>
                }

                <br />
                {this.state.includeMap ? <>
                    <Card><CardBody>
                        <div style={{ width: "100%" }}>
                            {" "}<LoadLidarBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />
                            <LoadRasBounds currentGeoJson={this.state.mapState.val} onLoad={data => this.setMapState({ val: data, versionId: this.state.mapState.versionId + 1 })} />
                            <MapContainer
                                mode="Point"
                                onMarkerUpdate={(lat: number, lon: number) => this.onMarkerUpdate(lat, lon)}
                                markerPosition={({ lat: this.state.clickedMapPoint.latitude, lng: this.state.clickedMapPoint.longitude })}
                                geoJson={this.state.mapState.val}
                                styles={this.styles}
                                center={mapProps.center}
                                zoom={mapProps.zoom}
                                versionId={this.state.mapState.versionId} />
                        </div>
                        {
                            (this.state.response?.response && this.state.kml) &&
                            <DownloadContents contents={() => this.state.kml!} fileName="results.kmz" />
                        }
                    </CardBody></Card>
                    <br />
                </>
                    : <></>
                }
                <Card isHoverable={true}><CardBody><Measure bounds={true}
                    onResize={contentRect => this.setState({ width: contentRect.bounds!.width })}>
                    {({ measureRef }) => <div ref={measureRef}>
                        <ChannelDisplay
                            totalWidth={this.state.width - 10}
                            topLeft={{ x: 5, y: 10 }}
                            channelHeight={30}
                            channels={this.state.response?.availableChannelInfo ?
                                generateChannelData(this.state.response.availableChannelInfo, this.state.minEirp, this.state.maxEirp,
                                    this.state.blackChannels, this.state.redChannels
                                ) : emptyChannels}

                        />
                    </div>}
                </Measure></CardBody></Card>
                <br />

                {this.state.response?.availableFrequencyInfo && <SpectrumDisplayAFC spectrum={this.state.response} />}
                <br />
                {this.state.response?.availableChannelInfo && <SpectrumDisplayLineAFC spectrum={this.state.response} />}
                <br />
                <JsonRawDisp value={this.state?.fullJsonResponse ? this.state.fullJsonResponse : this.state?.err?.body} />
            </PageSection>);
    }

}
