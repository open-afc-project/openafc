import * as React from "react";
import { AvailableSpectrumInquiryRequest, LinearPolygon, RadialPolygon, Ellipse, DeploymentEnum, Elevation, CertificationId, Channels, FrequencyRange } from "../Lib/RatAfcTypes";
import { logger } from "../Lib/Logger";
import {
    Modal, Button, ClipboardCopy, ClipboardCopyVariant, Alert, Gallery, GalleryItem,
    FormGroup, TextInput, InputGroup, InputGroupText, FormSelect, FormSelectOption, FormHelperText,
    ChipGroup, Chip
} from "@patternfly/react-core";
import { Timer } from "../Components/Timer";
import { getCacheItem, cacheItem, getAfcConfigFile } from "../Lib/RatApi";
import { AFCConfigFile, RatResponse, IndoorOutdoorType } from "../Lib/RatApiTypes";
import { ifNum } from "../Lib/Utils";
import { LocationForm } from "./LocationForm";
import { Limit } from "../Lib/Admin";
import { ElevationForm } from "./ElevationForm";
import ExclamationCircleIcon from '@patternfly/react-icons/dist/esm/icons/exclamation-circle-icon';
import { OperatingClass, OperatingClassIncludeType } from "../Lib/RatAfcTypes";
import { OperatingClassForm } from "./OperatingClassForm";
import { InquiredFrequencyForm } from "./InquiredFrequencyForm";


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
        radialPolygon?: RadialPolygon,
        elevation?: Elevation,
    },

    serialNumber?: string,
    certificationId?: CertificationId[],
    newCertificationId?: string,
    newCertificationNra?: string,
    indoorDeployment?: DeploymentEnum,
    minDesiredPower?: number,
    inquiredFrequencyRange: FrequencyRange[],
    newChannel?: number,

    operatingClasses: OperatingClass[]


}

export class RatAfcForm extends React.Component<RatAfcFormParams, RatAfcFormState> {

    constructor(props: RatAfcFormParams) {
        super(props);

        this.state = {
            isModalOpen: false,
            status: props.config.kind === "Error" ? "Error" : undefined,
            message: props.config.kind === "Error" ? "AFC config file could not be loaded" : "",
            submitting: false,
            location: { elevation: {} },
            serialNumber: undefined,
            certificationId: [],
            indoorDeployment: undefined,
            minDesiredPower: undefined,
            inquiredFrequencyRange: [],
            operatingClasses: [
                {
                    num: 131,
                    include: OperatingClassIncludeType.None
                },

                {
                    num: 132,
                    include: OperatingClassIncludeType.None
                },

                {
                    num: 133,
                    include: OperatingClassIncludeType.None
                },

                {
                    num: 134,
                    include: OperatingClassIncludeType.None
                },
            ]
        };
    }

    componentDidMount() {
        const st: RatAfcFormState = getCacheItem("ratAfcFormCache");
        if (st !== undefined) {
          
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
            const elevation = location?.elevation;
            this.setState({
                serialNumber: params?.deviceDescriptor?.serialNumber,
                certificationId: params?.deviceDescriptor?.certificationId || [],
                location: location,
                indoorDeployment: location?.indoorDeployment,
                minDesiredPower: params?.minDesiredPower,
                operatingClasses: this.toOperatingClassArray(params?.inquiredChannels),
                inquiredFrequencyRange: params.inquiredFrequencyRange,
            })
        } catch (e) {
            logger.error("Pasted value is not valid JSON {" + value + "}");
        }
    }


    deleteCertifcationId(currentCid: string): void {
        const copyOfcertificationId = this.state.certificationId.filter(x => x.id != currentCid);
        this.setState({ certificationId: copyOfcertificationId });
    }
    addCertificationId(newCertificationId: CertificationId): void {
        const copyOfcertificationId = this.state.certificationId.slice();
        copyOfcertificationId.push({ id: newCertificationId.id, nra: newCertificationId.nra });
        this.setState({ certificationId: copyOfcertificationId, newCertificationId: '', newCertificationNra: '' });
    }


    private updateOperatingClass(e: OperatingClass, i: number) {
        var opClassesCopy = this.state.operatingClasses.slice();
        opClassesCopy.splice(i, 1, e);
        this.setState({ operatingClasses: opClassesCopy })
    }

    private getParamsJSON = () => ({
        requestId: "0",
        deviceDescriptor: {
            serialNumber: this.state.serialNumber!,
            certificationId: this.state.certificationId!,
            rulesetIds: ["US_47_CFR_PART_15_SUBPART_E"]
        },
        location: {
            ellipse: this.state.location.ellipse!,
            linearPolygon: this.state.location.linearPolygon!,
            radialPolygon: this.state.location.radialPolygon!,
            elevation: this.state.location.elevation!,
            indoorDeployment: this.state.indoorDeployment!
        },
        minDesiredPower: this.state.minDesiredPower!,
        vendorExtensions: [],
        inquiredChannels:
            this.state.operatingClasses.filter(x => x.include != OperatingClassIncludeType.None).map(x => this.fromOperatingClass(x)),

        inquiredFrequencyRange:this.state.inquiredFrequencyRange && this.state.inquiredFrequencyRange.length > 0 ? this.state.inquiredFrequencyRange : []
    }) as AvailableSpectrumInquiryRequest;


    private fromOperatingClass(oc: OperatingClass) {
        switch (oc.include) {
            case OperatingClassIncludeType.None:
                return {};
            case OperatingClassIncludeType.All:
                return {
                    globalOperatingClass: oc.num,
                }
            case OperatingClassIncludeType.Some:
                return {
                    globalOperatingClass: oc.num,
                    channelCfi: oc.channels
                }
            default:
                break;
        }
    }

    private toOperatingClassArray(c: Channels[]) {
        var empties = [
            {
                num: 131,
                channels: [],
                include: OperatingClassIncludeType.None
            },
            {
                num: 132,
                channels: [],
                include: OperatingClassIncludeType.None
            },
            {
                num: 133,
                channels: [],
                include: OperatingClassIncludeType.None
            },
            {
                num: 134,
                channels: [],
                include: OperatingClassIncludeType.None
            },
        ];

        if (!c)
            return empties;

        var converted = c.map(v => this.toOperatingClass(v));
        var convertedClasses = converted.map(x => x.num);
        var emptiesNeeded = empties.filter(x => !convertedClasses.includes(x.num))
        var merge = converted.concat(emptiesNeeded).sort((a, b) => a.num - b.num);

        return merge;
    }

    private toOperatingClass(c: Channels) {
        var include = OperatingClassIncludeType.None
        if (!c.channelCfi) {
            include = OperatingClassIncludeType.All
        } else if (c.channelCfi && c.channelCfi.length > 0) {
            include = OperatingClassIncludeType.Some
        }

        var oc: OperatingClass = {
            num: c.globalOperatingClass,
            channels: c.channelCfi,
            include: include
        }
        return oc;

    }

    private mapChannelsToOperatingClassArray(channels: number[]) {
        var result = [
            {
                globalOperatingClass: 131,
                channelCfi: []
            },
            {
                globalOperatingClass: 132,
                channelCfi: []
            },
            {
                globalOperatingClass: 133,
                channelCfi: []
            },
            {
                globalOperatingClass: 134,
                channelCfi: []
            },
        ];

        channels.forEach(n => {
            if (n % 4 == 1) {
                result[0].channelCfi.push(n);
            } else if (n % 8 == 3) {
                result[1].channelCfi.push(n);
            } else if (n % 16 == 7) {
                result[2].channelCfi.push(n);
            } else if (n % 32 == 15) {
                result[3].channelCfi.push(n);
            }
        });

        var filteredResult = result.filter(x => x.channelCfi.length > 0);


        return filteredResult;
    }



    private copyPasteClick() {
        this.setState({ isModalOpen: true });
    }

    /**
     * Validates form data
     */
    private validInputs() {
        return true;
        // Per RAT-285, move all error checking to the engine


        // return !!this.state.serialNumber
        //     && this.state.certificationId && this.state.certificationId.length
        //     && this.locationValid(this.state.location.ellipse, this.state.location.linearPolygon, this.state.location.radialPolygon)
        //     && this.state.elevation.height >= 0
        //     && this.state.indoorDeployment !== undefined
        //     && this.state.elevation.verticalUncertainty >= 0
        //     && (this.state.minDesiredPower == undefined || this.state.minDesiredPower >= 0)
        //     && ((this.state.globalOperatingClass >= 0 && Number.isInteger(this.state.globalOperatingClass))
        //         || (this.state.minFreqMHz >= 0 && this.state.maxFreqMHz >= 0 && this.state.minFreqMHz <= this.state.maxFreqMHz)
        //     ) && (this.props.limit.enforce ? this.state.minDesiredPower === undefined || 
        //             this.state.minDesiredPower >= this.props.limit.limit : this.state.minDesiredPower === undefined || this.state.minDesiredPower >= 0)
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
        console.log('value for validInput' + this.validInputs())
        if (!this.validInputs()) {
            this.setState({ status: "Error", message: "One or more inputs are invalid." });
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
                    <ClipboardCopy variant={ClipboardCopyVariant.expansion}
                        onChange={(text: string | number) => this.setConfig(String(text))} aria-label="text area">{JSON.stringify(this.getParamsJSON())}</ClipboardCopy>
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
                            helperTextInvalid="Provide at least one Certification Id"
                            // helperTextInvalidIcon={<ExclamationCircleIcon />} //This is not supported in our version of Patternfly
                            validated={this.state.certificationId.length > 0 ? "success" : "error"}
                        >
                            <ChipGroup aria-orientation="vertical"
                            >
                                {this.state.certificationId.map(currentCid => (
                                    <Chip width="100%" key={currentCid.id} onClick={() => this.deleteCertifcationId(currentCid.id)}>
                                        {currentCid.nra + " " + currentCid.id}
                                    </Chip>
                                ))}
                            </ChipGroup>
                            <div>
                                {" "}<Button key="add-certificationId" variant="tertiary"
                                    onClick={() => this.addCertificationId({ id: this.state.newCertificationId, nra: this.state.newCertificationNra })}>
                                    +
                                </Button>
                            </div>
                            <div>
                                <TextInput
                                    label="NRA"
                                    value={this.state.newCertificationNra}
                                    onChange={x => this.setState({ newCertificationNra: x })}
                                    type="text"
                                    step="any"
                                    id="horizontal-form-certification-nra"
                                    name="horizontal-form-certification-nra"
                                    style={{ textAlign: "left" }}
                                    placeholder="NRA"
                                />

                                <TextInput
                                    label="Id"
                                    value={this.state.newCertificationId}
                                    onChange={x => this.setState({ newCertificationId: x })}
                                    type="text"
                                    step="any"
                                    id="horizontal-form-certification-list"
                                    name="horizontal-form-certification-list"
                                    style={{ textAlign: "left" }}
                                    placeholder="Id"
                                />
                            </div>
                        </FormGroup>
                    </GalleryItem>
                    <LocationForm
                        location={this.state.location}
                        onChange={x => this.setState({ location: x })}
                    />
                    <GalleryItem>
                        <FormGroup label={this.props.limit.enforce ? "Minimum Desired EIRP (Min: " + this.props.limit.limit + " dBm)" : "Minimum Desired EIRP"} fieldId="horizontal-form-min-eirp">
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
                                    onChange={(x: string) => ((x !== undefined) && this.setState({ indoorDeployment: Number.parseInt(x) }))}
                                    isValid={this.state.indoorDeployment !== undefined}
                                    id="horizontal-form-indoor-deployment"
                                    name="horizontal-form-indoor-deployment">
                                    <FormSelectOption key={undefined} value={undefined} label="Please select a value" isDisabled={true} />
                                    <FormSelectOption key={DeploymentEnum.indoorDeployment} value={DeploymentEnum.indoorDeployment} label="Indoor" />
                                    <FormSelectOption key={DeploymentEnum.outdoorDeployment} value={DeploymentEnum.outdoorDeployment} label="Outdoor" />
                                    <FormSelectOption key={DeploymentEnum.unkown} value={DeploymentEnum.unkown} label="Undefined" />
                                </FormSelect>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <InquiredFrequencyForm
                            inquiredFrequencyRange={this.state.inquiredFrequencyRange}
                            onChange={x => this.setState({ inquiredFrequencyRange: x.inquiredFrequencyRange })} >
                        </InquiredFrequencyForm>
                        {/* <FormGroup label="Minimum Frequency" fieldId="horizontal-form-min-freq">
                            <InputGroup>
                                <TextInput
                                    value={this.state.minFreqMHz}
                                    onChange={x => ifNum(x, n => this.setState({ minFreqMHz: n === 0 ? undefined : n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-min-freq"
                                    name="horizontal-form-min-freq"
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
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>MHz</InputGroupText>
                            </InputGroup>
                        </FormGroup>*/}
                    </GalleryItem>
                </Gallery>
                <FormGroup label="Inquired Channel(s)" fieldId="horizontal-form-operating-class" className="inquiredChannelsSection">
                    <Gallery className="nestedGallery" width={"1200px"} >
                        {this.state.operatingClasses.map(
                            (e, i) => (
                                <OperatingClassForm key={i}
                                    operatingClass={e}
                                    onChange={x => this.updateOperatingClass(x, i)}
                                ></OperatingClassForm>
                            )
                        )}
                    </Gallery>
                </FormGroup>


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
