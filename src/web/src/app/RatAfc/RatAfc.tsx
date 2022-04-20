import * as React from "react";
import { PageSection, Title, Card, Modal, Button, ClipboardCopy, ClipboardCopyVariant, CardBody, Expandable, Alert, AlertActionCloseButton } from "@patternfly/react-core";
import { spectrumInquiryRequest, sampleRequestObject } from "../Lib/RatAfcApi";
import { getCacheItem, cacheItem } from "../Lib/RatApi";
import { PAWSResponse, ResError, PAWSRequest, AFCConfigFile, RatResponse, error, ChannelData } from "../Lib/RatApiTypes";
import { PAWSForm } from "../VirtualAP/PAWSForm";
import { SpectrumDisplayAFC } from "../Components/SpectrumDisplay";
import { ChannelDisplay, emptyChannels } from "../Components/ChannelDisplay";
import { JsonRawDisp } from "../Components/JsonRawDisp";
import { AvailableSpectrumInquiryRequest, AvailableSpectrumInquiryResponse, AvailableChannelInfo } from "../Lib/RatAfcTypes";
import { Timer } from "../Components/Timer";
import { logger } from "../Lib/Logger";
import { RatAfcForm } from "./RatAfcForm";
import { clone } from "../Lib/Utils";
import Measure from "react-measure";
import { Limit } from "../Lib/Admin";

/**
 * RatAfc.tsx: virtual AP page for AP-AFC specification
 * author: Sam Smucny
 */

/**
 * Properties for RatAfc
 */
interface RatAfcProps {
    afcConfig: RatResponse<AFCConfigFile>
    limit: RatResponse<Limit>
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
    width: number
}

/**
 * generates channel data to be displayed from AP-AFC `AvailableChannelInfo` results
 * @param channelClasses AP-AFC formatted channels
 * @param minEirp min eirp to use for coloring
 * @param maxEirp maz eirp to use for coloring
 */
const generateChannelData = (channelClasses: AvailableChannelInfo[], minEirp: number, maxEirp: number): ChannelData[] => {
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

    return channelData;
}

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
                err: error("AFC config was not loaded properly. Try refreshing the page.")
            }
        } else {
            this.state = {
                width: 500,
                minEirp: props.afcConfig.result.minEIRP,
                maxEirp: props.afcConfig.result.maxEIRP,
                extraWarning: undefined,
                extraWarningTitle: undefined
            };
        }
    }

    componentDidMount() {
        const st = getCacheItem("ratAfcCache");
        if (st !== undefined)
            this.setState(st);
    }

    componentWillUnmount() {
        // before removing object, let's cache the state in case we want to come back
        const state = this.state;
        cacheItem("ratAfcCache", state);
    }

    /**
     * make a request to AFC Engine
     * @param request request to send
     */
    private sendRequest = (request: AvailableSpectrumInquiryRequest) => {

        // Removed per RAT-285
        // check AGL settings and possibly truncate
        // if (request.location.elevation.height - request.location.elevation.verticalUncertainty < 1) {
        //     // modify if height is not 1m above terrain height
        //     const minHeight = 1;
        //     const maxHeight = request.location.elevation.height + request.location.elevation.verticalUncertainty;
        //     if (maxHeight < minHeight) {
        //         this.setState({
        //             err: error(`The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height is ${maxHeight}m`),
        //             status: "Error"
        //         });
        //         return;
        //     }
        //     const newHeight = (minHeight + maxHeight) / 2;
        //     const newUncertainty = newHeight - minHeight;
        //     request.location.elevation.height = newHeight;
        //     request.location.elevation.verticalUncertainty = newUncertainty;
        //     logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
        //     this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m` });
        // }

        // make api call
        this.setState({ status: "Info" });
        return spectrumInquiryRequest(request)
            .then(resp => {
                if (resp.kind == "Success") {
                    const response = resp.result;
                    if (response.response.responseCode === 0) {
                        const minEirp = request.minDesiredPower || this.state.minEirp;
                        this.setState({
                            status: "Success",
                            response: resp.result,
                            minEirp: minEirp,
                        });
                    }
                } else if (!resp.kind || resp.kind == "Error") {
                    this.setState({ status: "Error", err: error(resp.description, resp.errorCode, resp.body), response: resp.body });
                }
            });
    }

    render() {
        return (
            <PageSection id="ap-afc-page">
                <Title size="lg">RAT AFC AP</Title>
                <Card>
                    <CardBody>
                        <RatAfcForm limit={this.props.limit.kind == "Success" ? this.props.limit.result : new Limit(false, 0)} config={this.props.afcConfig} onSubmit={req => this.sendRequest(req)} />
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
                <Card isHoverable={true}><CardBody><Measure bounds={true}
                    onResize={contentRect => this.setState({ width: contentRect.bounds!.width })}>
                    {({ measureRef }) => <div ref={measureRef}>
                        <ChannelDisplay
                            totalWidth={this.state.width - 10}
                            topLeft={{ x: 5, y: 10 }}
                            channelHeight={30}
                            channels={this.state.response?.availableChannelInfo ? generateChannelData(this.state.response.availableChannelInfo, this.state.minEirp, this.state.maxEirp) : emptyChannels} />
                    </div>}
                </Measure></CardBody></Card>
                <br />
                {this.state.response?.availableFrequencyInfo && <SpectrumDisplayAFC spectrum={this.state.response} />}
                <br />
                <JsonRawDisp value={this.state?.response ? this.state.response : this.state?.err?.body} />
            </PageSection>);
    }
}
