import * as React from "react";
import { AvailableSpectrumInquiryRequest, LinearPolygon, RadialPolygon, Ellipse, DeploymentEnum } from "../Lib/RatAfcTypes";
import { logger } from "../Lib/Logger";
import { Modal, Button, ClipboardCopy, ClipboardCopyVariant, Alert, Gallery, GalleryItem, FormGroup, TextInput, InputGroup, InputGroupText, FormSelect, FormSelectOption, Checkbox, ChipGroup, Chip } from "@patternfly/react-core";
import { Timer } from "../Components/Timer";
import { getCacheItem, cacheItem, getAfcConfigFile } from "../Lib/RatApi";
import { AFCConfigFile, RatResponse, IndoorOutdoorType } from "../Lib/RatApiTypes";
import { ifNum } from "../Lib/Utils";
import { LocationForm } from "./LocationForm";
import { Limit } from "../Lib/Admin";

interface RatAfcFormParams {
    onSubmit: (a: AvailableSpectrumInquiryRequest) => Promise<void>,
    config: RatResponse<AFCConfigFile>,
    limit: Limit
}

interface RatAfcFormState {
    isModalOpen: boolean,
    status?: "Info" | "Error",
    message: string,
    submitting: boolean,

    location: {
        ellipse?: Ellipse,
        linearPolygon?: LinearPolygon,
        radialPolygon?: RadialPolygon
    },
    serialNumber?: string,
    certificationId?: string,
    // latitude?: number,
    // longitude?: number,
    // majorAxis?: number,
    // minorAxis?: number,
    // orientation?: number,
    height?: number,
    heightUncertainty?: number,
    indoorDeployment?: DeploymentEnum,
    minDesiredPower?: number,
    minFreqMHz?: number,
    maxFreqMHz?: number,
    globalOperatingClass?: number
    channelIndicies: number[],
    newChannel?: number,
    config?: AFCConfigFile
}

export class RatAfcForm extends React.Component<RatAfcFormParams, RatAfcFormState> {

    constructor(props: RatAfcFormParams) {
        super(props);

        this.state = {
            isModalOpen: false,
            status: props.config.kind === "Error" ? "Error" : undefined,
            message: props.config.kind === "Error" ? "AFC config file could not be loaded" : "",
            submitting: false,
            location: {},
            serialNumber: undefined,
            certificationId: undefined,
            // latitude: undefined,
            // longitude: undefined,
            // majorAxis: undefined,
            // minorAxis: undefined,
            // orientation: undefined,
            height: undefined,
            heightUncertainty: undefined,
            indoorDeployment: undefined,
            minDesiredPower: undefined,
            minFreqMHz: undefined,
            maxFreqMHz: undefined,
            globalOperatingClass: 133,
            channelIndicies: [],
            config: props.config.kind === "Success" ? props.config.result : undefined
        };
    }

    componentDidMount() {
        const st: RatAfcFormState = getCacheItem("ratAfcFormCache");
        if (st !== undefined) {
            st.config = this.state.config;
            this.setState(st);
        }
    }

    componentWillUnmount() {
        // before removing object, let's cache the state in case we want to come back
        const state = this.state;
        cacheItem("ratAfcFormCache", state);
    }

    private setConfig(value: string) {
        try {
          const params = JSON.parse(value) as AvailableSpectrumInquiryRequest;
          // set params
          const location = params?.location;
          const ellipse = location?.ellipse;
          this.setState({
            serialNumber: params?.deviceDescriptor?.serialNumber,
            certificationId: params?.deviceDescriptor?.certificationId,
            location: location,
            // latitude: ellipse?.center?.latitude,
            // longitude: ellipse?.center?.longitude,
            // majorAxis: ellipse?.majorAxis,
            // minorAxis: ellipse?.minorAxis,
            // orientation: ellipse?.orientation,
            height: location?.height,
            heightUncertainty: location?.verticalUncertainty,
            indoorDeployment: location?.indoorDeployment,
            minDesiredPower: params?.minDesiredPower,
            globalOperatingClass: params?.inquiredChannels && params.inquiredChannels[0].globalOperatingClass,
            channelIndicies: (params?.inquiredChannels && params.inquiredChannels[0].channelCfi) || [],
            minFreqMHz: params?.inquiredFrequencyRange && params.inquiredFrequencyRange[0].lowFrequency,
            maxFreqMHz: params?.inquiredFrequencyRange && params.inquiredFrequencyRange[0].highFrequency,
            config: params?.vendorExtensions && params.vendorExtensions[0]?.parameters
          })
        } catch (e) {
          logger.error("Pasted value is not valid JSON");
        }
    }

    private deleteChannel(channel: number) {
      const copyOfChannels = this.state.channelIndicies;
      const index = copyOfChannels.indexOf(channel as never);
      if (index !== -1) {
        copyOfChannels.splice(index, 1);
        this.setState({ channelIndicies: copyOfChannels });
      }
    }

    private addChannel(channel: number) {
        const copyOfChannels = this.state.channelIndicies;
        const existingChannel = copyOfChannels.indexOf(channel);
        if (existingChannel === -1 && channel > 0 && Number.isInteger(channel)) {
            copyOfChannels.push(channel);
            this.setState({ channelIndicies: copyOfChannels, newChannel: undefined });
        }
    }

    private getParamsJSON = () => ({
        requestId: "0",
        deviceDescriptor: {
            serialNumber: this.state.serialNumber!,
            certificationId: this.state.certificationId!,
            rulesetIds: ["47_CFR_PART_15_SUBPART_E"]
        },
        location: {
            ellipse: this.state.location.ellipse!,
            linearPolygon: this.state.location.linearPolygon!,
            radialPolygon: this.state.location.radialPolygon!,
            height: this.state.height!,
            verticalUncertainty: this.state.heightUncertainty!,
            indoorDeployment: this.state.indoorDeployment!
        },
        minDesiredPower: this.state.minDesiredPower!,
        vendorExtensions: [{
            extensionID: "RAT v0.6 AFC Config",
            parameters: this.state.config!
        }],
        inquiredChannels: ((this.state.channelIndicies.length > 0) ? [
            {
                globalOperatingClass: this.state.globalOperatingClass,
                channelCfi: this.state.channelIndicies
            }
        ] : [
            {
                globalOperatingClass: this.state.globalOperatingClass,
            }
        ]),
        inquiredFrequencyRange: ((this.state.minFreqMHz === undefined || this.state.maxFreqMHz === undefined) ? undefined : [
            { lowFrequency: this.state.minFreqMHz, highFrequency: this.state.maxFreqMHz }
        ])
    }) as AvailableSpectrumInquiryRequest;

    private copyPasteClick() {
        this.setState({ isModalOpen: true });
    }

    /**
     * Validates form data
     */
    private validInputs() {
        return !!this.state.serialNumber
            && !!this.state.certificationId
            && this.locationValid(this.state.location.ellipse, this.state.location.linearPolygon, this.state.location.radialPolygon)
            && this.state.height >= 0
            && this.state.indoorDeployment !== undefined
            && this.state.heightUncertainty >= 0
            && (this.state.minDesiredPower == undefined || this.state.minDesiredPower >= 0)
            && ((this.state.globalOperatingClass >= 0 && Number.isInteger(this.state.globalOperatingClass))
                || (this.state.minFreqMHz >= 0 && this.state.maxFreqMHz >= 0 && this.state.minFreqMHz <= this.state.maxFreqMHz)
            ) && this.props.limit.enforce ? this.state.minDesiredPower === undefined || this.state.minDesiredPower >= this.props.limit.limit : this.state.minDesiredPower === undefined || this.state.minDesiredPower >= 0
    }
    
    /**
     * validates location form data. only one of the location types needs to be present and valid
     * @param ellipse ellipse form data
     * @param linPoly linear polygon form data
     * @param radPoly radial polygon form data
     */
    private locationValid(ellipse?: Ellipse, linPoly?: LinearPolygon, radPoly?: RadialPolygon) {
        if (ellipse !== undefined) {
            return ellipse.center 
                && ellipse.center.latitude >= -90 && ellipse.center.latitude <= 90
                && ellipse.center.longitude >= -180 && ellipse.center.longitude <= 180
                && ellipse.majorAxis >= 0
                && ellipse.minorAxis >= 0
                && ellipse.majorAxis >= ellipse.minorAxis
                && ellipse.orientation >= 0 && ellipse.orientation < 180;
        } else if (linPoly !== undefined) {
            return linPoly.outerBoundary.length >= 3;
        } else if (radPoly !== undefined) {
            return radPoly.center
                && radPoly.center.latitude >= -90 && radPoly.center.latitude <= 90
                && radPoly.center.longitude >= -180 && radPoly.center.longitude <= 180
                && radPoly.outerBoundary.length >= 3;
        } else {
            return false;
        }
    }

    /**
     * submit form data after validating it to make the api request
     */
    private submit() {
        if (!this.validInputs()) {
            this.setState({ status: "Error", message: "One or more inputs are invalid."});
        } else if (!this.state.config) {
            this.setState({ status: "Error", message: "The AFC config has not been loaded"});
        } else {
            this.setState({ status: undefined, message: "" });
            const params = this.getParamsJSON();
            this.props.onSubmit(params)
            .then(() => this.setState({ submitting: false }));
        }
    }

    render() {
        return (
            <>
            <Modal
                title="Copy/Paste"
                isLarge={true}
                isOpen={this.state.isModalOpen}
                onClose={() => this.setState({ isModalOpen: false })}
                actions={[<Button key="update" variant="primary" onClick={() => this.setState({ isModalOpen: false })}>Close</Button>]}>
                <ClipboardCopy variant={ClipboardCopyVariant.expansion} onChange={(v: string) => this.setConfig(v)} aria-label="text area">{JSON.stringify(this.getParamsJSON())}</ClipboardCopy>
            </Modal>
                <Gallery gutter="sm">
                    <GalleryItem>
                        <FormGroup
                            label="Serial Number"
                            fieldId="horizontal-form-name"
                            helperText="Must be unique for every AP"
                        >
                            <TextInput
                                value={this.state.serialNumber}
                                type="text"
                                id="horizontal-form-name"
                                aria-describedby="horizontal-form-name-helper"
                                name="horizontal-form-name"
                                onChange={x => this.setState({ serialNumber: x })}
                                isValid={!!this.state.serialNumber}
                                style={{ textAlign: "right" }}
                            />
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup
                            label="Certification Id"
                            fieldId="horizontal-form-certification-id"
                        >
                            <TextInput
                                value={this.state.certificationId}
                                type="text"
                                id="horizontal-form-certification-id"
                                aria-describedby="horizontal-form-certification-id-helper"
                                name="horizontal-form-manufacturer"
                                onChange={x => this.setState({ certificationId: x })}
                                isValid={!!this.state.certificationId}
                                style={{ textAlign: "right" }}
                            />
                        </FormGroup>
                    </GalleryItem>
                    <LocationForm
                        location={this.state.location}
                        onChange={x => this.setState({ location: x })} />
                    <GalleryItem>
                        <FormGroup label="Height (Above Ground Level)" fieldId="horizontal-form-height">
                            <InputGroup>
                                <TextInput
                                    value={this.state.height}
                                    onChange={x => ifNum(x, n => this.setState({ height: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-height"
                                    name="horizontal-form-height"
                                    isValid={this.state.height >= 0}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>meters</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>                    
                    <GalleryItem>
                        <FormGroup label="Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert">
                            <InputGroup>
                                <TextInput
                                    value={this.state.heightUncertainty}
                                    onChange={x => ifNum(x, n => this.setState({ heightUncertainty: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-height-cert"
                                    name="horizontal-form-height-cert"
                                    isValid={this.state.heightUncertainty >= 0}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>meters</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label={this.props.limit.enforce ? "Minimum Desired EIRP (Min: " + this.props.limit.limit  + " dBm)": "Minimum Desired EIRP"} fieldId="horizontal-form-min-eirp">
                            <InputGroup>
                                <TextInput
                                    value={this.state.minDesiredPower}
                                    onChange={x => ifNum(x, n => this.setState({ minDesiredPower: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-min-eirp"
                                    name="horizontal-form-min-eirp"
                                    isValid={this.props.limit.enforce ? this.state.minDesiredPower === undefined || this.state.minDesiredPower >= this.props.limit.limit : this.state.minDesiredPower === undefined || this.state.minDesiredPower >= 0}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>dBm</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Indoor Deployment" fieldId="horizontal-form-indoor-deployment">
                            <InputGroup>                            
                                <FormSelect
                                    value={this.state.indoorDeployment}
                                    onChange={(x:string)=> ((x!==undefined) && this.setState({indoorDeployment: Number.parseInt(x)}))}
                                    isValid={this.state.indoorDeployment !==undefined} 
                                    id="horizontal-form-indoor-deployment"
                                    name="horizontal-form-indoor-deployment">
                                    <FormSelectOption  key={undefined} value={undefined} label="Please select a value" isDisabled={true}/> 
                                    <FormSelectOption  key={DeploymentEnum.indoorDeployment} value={DeploymentEnum.indoorDeployment} label="Indoor" />
                                    <FormSelectOption  key={DeploymentEnum.outdoorDeployment} value={DeploymentEnum.outdoorDeployment} label="Outdoor" />
                                    <FormSelectOption  key={DeploymentEnum.unkown} value={DeploymentEnum.unkown} label="Undefined" /> 
                                </FormSelect>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Minimum Frequency" fieldId="horizontal-form-min-freq">
                            <InputGroup>
                                <TextInput
                                    value={this.state.minFreqMHz}
                                    onChange={x => ifNum(x, n => this.setState({ minFreqMHz: n === 0 ? undefined : n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-min-freq"
                                    name="horizontal-form-min-freq"
                                    isValid={((this.state.globalOperatingClass >= 0 && Number.isInteger(this.state.globalOperatingClass))
                                        || (this.state.minFreqMHz >= 0 && this.state.minFreqMHz <= this.state.maxFreqMHz)
                                    )}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>MHz</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Maximum Frequency" fieldId="horizontal-form-max-freq">
                            <InputGroup>
                                <TextInput
                                    value={this.state.maxFreqMHz}
                                    onChange={x => ifNum(x, n => this.setState({ maxFreqMHz: n === 0 ? undefined : n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-max-freq"
                                    name="horizontal-form-max-freq"
                                    isValid={((this.state.globalOperatingClass >= 0 && Number.isInteger(this.state.globalOperatingClass))
                                        || (this.state.maxFreqMHz >= 0 && this.state.minFreqMHz <= this.state.maxFreqMHz)
                                    )}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>MHz</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    {/* <GalleryItem>
                        <FormGroup label="Global operating class" fieldId="horizontal-form-operating-class">
                            <InputGroup>
                                <TextInput
                                    value={this.state.globalOperatingClass}
                                    onChange={x => ifNum(x, n => this.setState({ globalOperatingClass: n === 0 ? undefined : n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-operating-class"
                                    name="horizontal-form-operating-class"
                                    isValid={((this.state.globalOperatingClass >= 0 && Number.isInteger(this.state.globalOperatingClass))
                                        || (this.state.minFreqMHz >= 0 && this.state.maxFreqMHz >= 0 && this.state.minFreqMHz <= this.state.maxFreqMHz)
                                    )}
                                    style={{ textAlign: "right" }}
                                />
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem> */}
                    <GalleryItem>
                        <FormGroup label="Channel Indices" fieldId="horizontal-form-channel-list">
                            <br/>
                            <ChipGroup>
                                {this.state.channelIndicies.map(currentChannel => (
                                    <Chip key={currentChannel} onClick={() => this.deleteChannel(currentChannel)}>
                                        {currentChannel}
                                    </Chip>
                                ))}
                            </ChipGroup>
                            {" "}<Button key="add-channel" variant="tertiary" onClick={() => this.addChannel(this.state.newChannel)}>+</Button>
                            <TextInput
                                    value={this.state.newChannel}
                                    onChange={x => ifNum(x, n => this.setState({ newChannel: n === 0 ? undefined : n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-channel-list"
                                    name="horizontal-form-channel-list"
                                    isValid={this.state.newChannel === undefined || (this.state.newChannel > 0 && Number.isInteger(this.state.newChannel))}
                                    style={{ textAlign: "right" }}
                                />
                            
                        </FormGroup>
                    </GalleryItem>
                </Gallery>
            <br />
                {
                    this.state.status === "Error" &&
                    <Alert title={"Error: "} variant="danger">
                        <pre>{this.state.message}</pre>
                    </Alert>
                }
            <br />
                <>
                    <Button variant="primary" isDisabled={this.state.submitting} onClick={() => this.submit()}>Send Request</Button>{" "}
                    <Button key="open-modal" variant="secondary" onClick={() => this.copyPasteClick()}>Copy/Paste</Button>
                </>
            </>)
    }
}
