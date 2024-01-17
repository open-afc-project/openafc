import * as React from "react";
import { FormGroup, InputGroup, TextInput, InputGroupText, FormSelect, FormSelectOption, ActionGroup, Checkbox, Button, AlertActionCloseButton, Alert, Gallery, GalleryItem, Card, CardBody, Modal, TextArea, ClipboardCopy, ClipboardCopyVariant, Tooltip, TooltipPosition, Radio, CardHead, PageSection } from "@patternfly/react-core";
import { IglooIcon, OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import BuildlingPenetrationLossForm from "./BuildingPenetrationLossForm";
import PolarizationMismatchLossForm from "./PolarizationMismatchLossForm";
import { ITMParametersForm } from "./ITMParametersForm";
import BodyLossForm from "./BodyLossForm";
import { APUncertaintyForm } from "./APUncertaintyForm"
import { PropogationModelForm } from "./PropogationModelForm";
import { AFCConfigFile, PenetrationLossModel, PolarizationLossModel, BodyLossModel, AntennaPatternState, DefaultAntennaType, UserAntennaPattern, RatResponse, PropagationModel, APUncertainty, ITMParameters, FSReceiverFeederLoss, FSReceiverNoise, FreqRange, CustomPropagation, ChannelResponseAlgorithm, FSClutterModel } from "../Lib/RatApiTypes";
import { Limit } from "../Lib/Admin";
import { AllowedRangesDisplay } from './AllowedRangesForm'
import AntennaPatternForm from "./AntennaPatternForm";

/**
* AFCForm.tsx: form for generating afc configuration files to be used to update server
* author: Sam Smucny
*/

/**
 * Form component for AFC Config
 */
export class AFCFormUSACanada extends React.Component<
    {
        limit: Limit,
        frequencyBands: FreqRange[],
        config: AFCConfigFile,
        antennaPatterns: AntennaPatternState,
        updateConfig: (x: AFCConfigFile) => void,
        updateAntennaData: (x: AntennaPatternState) => void,
    },
    {
    }
> {


    private trimmedRegionStr = () => {
        if (!!this.props.config.regionStr && (this.props.config.regionStr?.startsWith("TEST_") || this.props.config.regionStr?.startsWith("DEMO_"))) {
            return this.props.config.regionStr.substring(5);
        } else {
            return this.props.config.regionStr
        }
    }


    private hasValue = (val: any) => val !== undefined && val !== null;
    private hasValueExists = (f: () => any): boolean => {
        try {
            return this.hasValue(f());
        } catch (error) {
            return false;
        }

    }

    private setMinEIRPIndoor = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { minEIRPIndoor: n }));
    private setMinOutdoorEIRP = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { minEIRPOutdoor: n }));
    private setMaxEIRP = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { maxEIRP: n }));
    private setMinPSD = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { minPSD: n }));
    private setFeederLoss = (f: FSReceiverFeederLoss) => this.props.updateConfig(Object.assign(this.props.config, { receiverFeederLoss: f }));
    private setReceiverNoise = (f: FSReceiverNoise) => this.props.updateConfig(Object.assign(this.props.config, { fsReceiverNoise: f }));
    private setThreshold = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { threshold: n }));
    private setMaxLinkDistance = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { maxLinkDistance: n }));
    private setEnableMapInVirtualAp = (n: boolean) => this.props.updateConfig(Object.assign(this.props.config, { enableMapInVirtualAp: n }));
    private setRoundPSDEIRPFlag = (n: boolean) => this.props.updateConfig(Object.assign(this.props.config, { roundPSDEIRPFlag: n }));
    private setNearFieldAdjFlag = (n: boolean) => this.props.updateConfig(Object.assign(this.props.config, { nearFieldAdjFlag: n }));
    private setVisiblityThreshold = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { visibilityThreshold: n }));
    private setPropogationEnv = (n: string) => {

        const newConfig = { ...this.props.config };

        newConfig.propagationEnv = n as "NLCD Point" | "Population Density Map" | "Urban" | "Suburban" | "Rural";
        if (n != "NLCD Point" && this.props.config.propagationEnv == 'NLCD Point') {
            delete newConfig.nlcdFile;
        }
        if (n == "NLCD Point" && this.props.config.propagationEnv != 'NLCD Point' && !this.props.config.nlcdFile) {
            newConfig.nlcdFile = "nlcd_production";
        }
        this.props.updateConfig(newConfig);
    }
    private setNlcdFile = (n: string) => this.props.updateConfig(Object.assign(this.props.config, { nlcdFile: n }));

    private setScanBelowGround = (n: string) => {
        const conf = this.props.config;
        switch (n) {
            case "truncate":
                conf.scanPointBelowGroundMethod = "truncate";
                break;
            case "discard":
                conf.scanPointBelowGroundMethod = "discard";
                break;
            default:
                break;
        };
        this.props.updateConfig(conf);
    }

    private setPenetrationLoss = (x: PenetrationLossModel) => {
        const conf = { ...this.props.config };
        conf.buildingPenetrationLoss = x;
        this.props.updateConfig(conf);
    }

    private setPolarizationLoss = (x: PolarizationLossModel) => {
        const conf = { ...this.props.config };
        conf.polarizationMismatchLoss = x;
        this.props.updateConfig(conf);
    }

    private setBodyLoss = (x: BodyLossModel) => {
        const conf = { ...this.props.config };
        conf.bodyLoss = x;
        this.props.updateConfig(conf);
    }
    private setAntennaPattern = (x: AntennaPatternState) => {
        const conf = { ...this.props.config };
        conf.ulsDefaultAntennaType = x.defaultAntennaPattern;
        this.props.updateAntennaData(x);
        this.props.updateConfig(conf);
    }
    private setPropagationModel = (x: PropagationModel) => {
        if (x.kind === "Custom") {
            //rlanITMTxClutterMethod is set in the CustomPropagation but stored at the top level
            // so move it up if present
            const conf = { ...this.props.config };
            var model = x as CustomPropagation;
            var itmTxClutterMethod = model.rlanITMTxClutterMethod;
            delete model.rlanITMTxClutterMethod;
            if (!!itmTxClutterMethod) {
                conf.rlanITMTxClutterMethod = itmTxClutterMethod;
            } else {
                delete conf.rlanITMTxClutterMethod;
            }
            switch (x.winner2LOSOption) {
                case "BLDG_DATA_REQ_TX":
                    break;
                case "FORCE_LOS":
                    delete model.win2ConfidenceNLOS;
                    delete model.win2ConfidenceCombined;
                    break;
                case "FORCE_NLOS":
                    delete model.win2ConfidenceLOS;
                    delete model.win2ConfidenceCombined;
                    break;
                case "UNKNOWN":
                    break;
            }

            conf.propagationModel = model;
            this.props.updateConfig(conf);


        } else {
            const conf = { ...this.props.config };
            if (x.kind === "ITM with building data") {
                conf.rlanITMTxClutterMethod = "BLDG_DATA"
            } else {
                conf.rlanITMTxClutterMethod = "FORCE_TRUE"
            }
            if (x.kind === "FCC 6GHz Report & Order" && x.buildingSource === "B-Design3D") {
                delete x.win2ConfidenceLOS;
                delete x.win2ConfidenceNLOS;
            }
            if (x.kind === "FCC 6GHz Report & Order" && x.buildingSource === "None") {
                delete x.win2ConfidenceNLOS;
            }
            if (x.kind === "Brazilian Propagation Model" && x.buildingSource === "B-Design3D") {
                delete x.win2ConfidenceLOS;
                delete x.win2ConfidenceNLOS;
            }

            if (x.kind === "Brazilian Propagation Model" && x.buildingSource === "None") {
                delete x.win2ConfidenceNLOS;
            }
            if (x.kind !== "ISED DBS-06" ) {
                conf.cdsmDir = "";
            }
            if (x.kind === "ISED DBS-06") {
                if (x.surfaceDataSource === 'None') {
                    conf.cdsmDir = "";
                } else if (conf.cdsmDir === "") {
                    conf.cdsmDir = "rat_transfer/cdsm/3ov4_arcsec_wgs84";
                }
            }

            conf.propagationModel = x;
            this.props.updateConfig(conf);
        }
    }

    private setAPUncertainty = (x: APUncertainty) => {
        const conf = { ...this.props.config };
        conf.APUncertainty = x;
        this.props.updateConfig(conf);
    }

    private setITMParams = (x: ITMParameters) => {
        const conf = { ...this.props.config };
        conf.ITMParameters = x;
        this.props.updateConfig(conf);
    }

    private setUseClutter = (x: boolean) => {
        const conf = { ...this.props.config };
        //If changing to true, and was false, and there is no clutter model, use the defaults
        if (x && !conf.clutterAtFS && !conf.fsClutterModel) {
            conf.fsClutterModel = { maxFsAglHeight: 6, p2108Confidence: 5 };
        }
        conf.clutterAtFS = x;
        this.props.updateConfig(conf);
    }

    private setFsClutterConfidence = (n: number | string) => {
        const val = Number(n);
        const conf = this.props.config;
        const newParams: FSClutterModel = { ...conf.fsClutterModel! };
        newParams.p2108Confidence = val
        conf.fsClutterModel = newParams;
        this.props.updateConfig(conf);
    }
    private setFsClutterMaxHeight = (n: number | string) => {
        const val = Number(n);
        const conf = this.props.config;
        const newParams: FSClutterModel = { ...conf.fsClutterModel! };
        newParams.maxFsAglHeight = val;
        conf.fsClutterModel = newParams;
        this.props.updateConfig(conf);
    }



    private getPropagationModelForForm = () => {
        //rlanITMTxClutterMethod is stored at the top level but set in the form
        // so move it down if present
        if (this.props.config.propagationModel.kind !== "Custom") {
            return { ...this.props.config.propagationModel };
        } else {
            const customModel = { ...this.props.config.propagationModel } as CustomPropagation;
            customModel.rlanITMTxClutterMethod = this.props.config.rlanITMTxClutterMethod;
            return customModel;
        }

    }


    private setChannelResponseAlgorithm = (v: ChannelResponseAlgorithm) => {
        this.props.updateConfig(Object.assign(this.props.config, { channelResponseAlgorithm: v }));
    }

    private getLandCoverOptions = () => {
        let trimmed = this.trimmedRegionStr();
        switch (trimmed) {
           
            case "CA":
                return <>
                    <FormSelectOption key="2020 Land Cover " value="rat_transfer/nlcd/ca/landcover-2020-classification_resampled.tif" label="Land Cover of Canada" />
                </>
            case "BR":
                return <>
                    <FormSelectOption key="Brazilian Land Cover" value="rat_transfer/nlcd/br/landcover-for-brazil.tbd.tif" label="Land Cover of Brazil (TBD)" />
                </>
            case 'GB':
                return <>
                    <FormSelectOption key="Corine Land Cover" value="rat_transfer/nlcd/eu/U2018_CLC2012_V2020_20u1_resampled.tif" label="Corine Land Cover" />
                </>
            case "US":
            default:
                return <>
                    <FormSelectOption key="Production NLCD " value="rat_transfer/nlcd/nlcd_production" label="Production NLCD" />
                    <FormSelectOption key="WFA Test NLCD " value="rat_transfer/nlcd/nlcd_wfa" label="WFA Test NLCD" />
                </>
        }

       
    }

    private getDefaultLandCoverDatabase = () => {
        switch (this.trimmedRegionStr()) {
            case "CA":
                return 'rat_transfer/nlcd/ca/landcover-2020-classification_resampled.tif';
            case "BR":
                return 'rat_transfer/nlcd/br/landcover-for-brazil.tbd.tif'
            case 'GB':
                return 'rat_transfer/nlcd/eu/U2018_CLC2012_V2020_20u1_resampled.tif'
            case "US":
            default:
                return 'rat_transfer/nlcd/nlcd_production';
        }
    }

    render = () =>
        <>
            <GalleryItem>
                <FormGroup label={this.props.limit.indoorEnforce ? "AP Min. Indoor EIRP (Min: " + this.props.limit.indoorLimit + " dBm)" : "AP Min. Indoor EIRP"} fieldId="horizontal-form-min-eirp">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.minEIRPIndoor}
                            onChange={x => this.setMinEIRPIndoor(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-min-eirp"
                            name="horizontal-form-min-eirp"
                            isValid={this.props.limit.indoorEnforce ? this.props.config.minEIRPIndoor <= this.props.config.maxEIRP && this.props.config.minEIRPIndoor >= this.props.limit.indoorLimit : this.props.config.minEIRPIndoor <= this.props.config.maxEIRP}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm</InputGroupText>
                    </InputGroup>
                </FormGroup>
                <FormGroup label={this.props.limit.outdoorEnforce ? "AP Min. Outdoor EIRP (Min: " + this.props.limit.outdoorLimit + " dBm)" : "AP Min. Outdoor EIRP"} fieldId="horizontal-form-min-eirp">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.minEIRPOutdoor}
                            onChange={x => this.setMinOutdoorEIRP(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-min-eirp"
                            name="horizontal-form-min-eirp"
                            isValid={this.props.limit.outdoorEnforce ? this.props.config.minEIRPOutdoor <= this.props.config.maxEIRP && this.props.config.minEIRPOutdoor >= this.props.limit.outdoorLimit : this.props.config.minEIRPOutdoor <= this.props.config.maxEIRP}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm</InputGroupText>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="AP Max. EIRP" fieldId="horizontal-form-max-eirp">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.maxEIRP}
                            onChange={x => this.setMaxEIRP(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-max-eirp"
                            name="horizontal-form-max-eirp"
                            isValid={(this.props.limit.indoorEnforce || this.props.limit.outdoorEnforce) ? 
                                this.props.config.minEIRPIndoor <= this.props.config.maxEIRP && this.props.config.maxEIRP >= this.props.limit.indoorLimit 
                                && this.props.config.minEIRPOutdoor <= this.props.config.maxEIRP && this.props.config.maxEIRP >= this.props.limit.outdoorLimit 
                                 : this.props.config.minEIRPIndoor <= this.props.config.maxEIRP &&  this.props.config.minEIRPOutdoor <= this.props.config.maxEIRP }
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm</InputGroupText>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="AP Min. PSD" fieldId="horizontal-form-max-eirp">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.minPSD}
                            onChange={x => this.setMinPSD(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-max-eirp"
                            name="horizontal-form-max-eirp"
                            isValid={this.hasValue(this.props.config.minPSD)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm/MHz</InputGroupText>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>

            <GalleryItem>
                <BuildlingPenetrationLossForm
                    data={this.props.config.buildingPenetrationLoss}
                    onChange={this.setPenetrationLoss} />
            </GalleryItem>
            <GalleryItem>
                <PolarizationMismatchLossForm
                    data={this.props.config.polarizationMismatchLoss}
                    onChange={this.setPolarizationLoss} />
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="FS Receiver Feeder Loss" fieldId="horizontal-form-receiver-feeder-loss">
                    {" "}<Tooltip
                        position={TooltipPosition.top}
                        enableFlip={true}
                        className="fs-feeder-loss-tooltip"
                        maxWidth="40.0rem"
                        content={
                            <>
                                <p>Feederloss is set to: </p>
                                <ul>
                                    <li>
                                        the Feederloss in the FS Database (if present)
                                    </li>
                                    <li>Else, the applicable value below</li>
                                </ul>
                            </>
                        }
                    >
                        <OutlinedQuestionCircleIcon />
                    </Tooltip>
                    <InputGroup>
                        <TextInput
                            id="feeder-idu-label"
                            name="feeder-idu-label"
                            isReadOnly={true}
                            value="IDU"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.receiverFeederLoss.IDU}
                            onChange={x => this.setFeederLoss({ ...this.props.config.receiverFeederLoss, IDU: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-receiver-feeder-loss-idu"
                            name="horizontal-form-receiver-feeder-loss-idu"
                            isValid={this.hasValue(this.props.config.receiverFeederLoss.IDU)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dB</InputGroupText>
                    </InputGroup>
                    <InputGroup>
                        <TextInput
                            id="feeder-odu-label"
                            name="feeder-odu-label"
                            isReadOnly={true}
                            value="ODU"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.receiverFeederLoss.ODU}
                            onChange={x => this.setFeederLoss({ ...this.props.config.receiverFeederLoss, ODU: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-receiver-feeder-loss-7"
                            name="horizontal-form-receiver-feeder-loss-7"
                            isValid={this.hasValue(this.props.config.receiverFeederLoss.ODU)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dB</InputGroupText>
                    </InputGroup>
                    <InputGroup>
                        <TextInput
                            id="feeder-o-label"
                            name="feeder-o-label"
                            isReadOnly={true}
                            value="Unknown"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.receiverFeederLoss.UNKNOWN}
                            onChange={x => this.setFeederLoss({ ...this.props.config.receiverFeederLoss, UNKNOWN: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-receiver-feeder-loss-o"
                            name="horizontal-form-receiver-feeder-loss-o"
                            isValid={this.hasValue(this.props.config.receiverFeederLoss.UNKNOWN)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dB</InputGroupText>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="FS Receiver Noise Floor" fieldId="horizontal-form-receiver-noise">
                    {" "}<Tooltip
                        position={TooltipPosition.top}
                        enableFlip={true}
                        className="fs-feeder-loss-tooltip"
                        maxWidth="40.0rem"
                        content={
                            <>
                                <p>The FS receiver center frequency is compared against the value shown to choose the proper noise floor</p>
                            </>
                        }
                    >
                        <OutlinedQuestionCircleIcon />
                    </Tooltip>
                    <FormGroup label="FS Freq <= 6425 MHz" fieldId="horizontal-form-noiseFloor-0">
                        <InputGroup>
                            <TextInput
                                value={this.hasValueExists(() => this.props.config.fsReceiverNoise?.noiseFloorList[0]) ? this.props.config.fsReceiverNoise?.noiseFloorList[0] : -110}
                                type="number"
                                onChange={x => this.setReceiverNoise({ ...this.props.config.fsReceiverNoise, noiseFloorList: [Number(x), this.props.config.fsReceiverNoise.noiseFloorList[1]] })}
                                step="any"
                                id="horizontal-form-noiseFloor-0"
                                name="horizontal-form-noiseFloor-0"
                                isValid={this.hasValueExists(() => this.props.config.fsReceiverNoise?.noiseFloorList[0])}
                                style={{ textAlign: "right" }}
                            />
                            <InputGroupText>dBm/MHz</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                    <FormGroup label="FS Freq > 6425 MHz" fieldId="horizontal-form-noiseFloor-1">
                        <InputGroup>
                            <TextInput
                                value={this.hasValueExists(() => this.props.config.fsReceiverNoise?.noiseFloorList[1]) ? this.props.config.fsReceiverNoise?.noiseFloorList[1] : -109.5}
                                onChange={x => this.setReceiverNoise({ ...this.props.config.fsReceiverNoise, noiseFloorList: [this.props.config.fsReceiverNoise.noiseFloorList[0], Number(x)] })}
                                step="any"
                                type="number"
                                id="horizontal-form-noiseFloor-1"
                                name="horizontal-form-noiseFloor-1"
                                isValid={this.hasValueExists(() => this.props.config.fsReceiverNoise.noiseFloorList[1])}
                                style={{ textAlign: "right" }}
                            />
                            <InputGroupText size={1}>dBm/MHz</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <BodyLossForm
                    data={this.props.config.bodyLoss}
                    onChange={this.setBodyLoss} />
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="I/N Threshold" fieldId="horizontal-form-threshold">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.threshold}
                            onChange={x => this.setThreshold(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-threshold"
                            name="horizontal-form-threshold"
                            isValid={this.hasValue(this.props.config.threshold)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dB</InputGroupText>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <FormGroup
                    label="Max Link Distance to Consider"
                    fieldId="propogation-model-max-link-distance"
                >
                    <InputGroup><TextInput
                        value={this.props.config.maxLinkDistance}
                        type="number"
                        onChange={(x) => this.setMaxLinkDistance(Number(x))}
                        id="propogation-model-max-link-distance"
                        name="propogation-model-max-link-distance"
                        style={{ textAlign: "right" }}
                        isValid={this.props.config.maxLinkDistance >= 1} />
                        <InputGroupText>km</InputGroupText></InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <AntennaPatternForm
                    data={this.props.antennaPatterns}
                    onChange={this.setAntennaPattern}
                    region={this.trimmedRegionStr() ?? "US"} />
            </GalleryItem>
            <GalleryItem>
                <APUncertaintyForm
                    data={this.props.config.APUncertainty}
                    onChange={this.setAPUncertainty} />
            </GalleryItem>
            <GalleryItem>
                <PropogationModelForm
                    data={this.getPropagationModelForForm()}
                    onChange={this.setPropagationModel}
                    region={this.trimmedRegionStr() ?? "US"} />
            </GalleryItem>
            {(this.props.config.propagationModel.kind === "ITM with no building data"
                || this.props.config.propagationModel.kind == "FCC 6GHz Report & Order"
                || this.props.config.propagationModel.kind == "ISED DBS-06"
                || this.props.config.propagationModel.kind == "Brazilian Propagation Model"
                || this.props.config.propagationModel.kind == "Ofcom Propagation Model") &&
                <GalleryItem>
                    <FormGroup
                        label="AP Propagation Environment"
                        fieldId="propogation-env"
                    >
                        {" "}<Tooltip
                            position={TooltipPosition.top}
                            enableFlip={true}
                            //className="prop-env-tooltip"
                            maxWidth="40.0rem"
                            content={
                                <>
                                    <p>AP Propagation Environment:</p>
                                    <ul>
                                        <li>- "Land Cover Point" assigns propagation environment based on single Land Cover tile class the RLAN resides in: Urban if Land Cover tile = 23 or 24, Suburban if Land Cover tile = 22, Rural-D (Deciduous trees) if Land Cover tile = 41, 43 or 90, Rural-C (Coniferous trees) if Land Cover tile = 42, and Rural-V (Village Center) otherwise. The various Rural types correspond to the P.452 clutter category.</li>
                                        <li>- If “Land Cover Point” is not selected, Village center is assumed for the Rural clutter category.</li>
                                    </ul>
                                </>
                            }
                        >
                            <OutlinedQuestionCircleIcon />
                        </Tooltip>

                        <FormSelect
                            value={this.props.config.propagationEnv}
                            onChange={(x) => this.setPropogationEnv(x)}
                            id="propogation-env"
                            name="propogation-env"
                            style={{ textAlign: "right" }}
                            isValid={this.props.config.propagationEnv !== undefined}>
                            <FormSelectOption key="NLCD Point" value="NLCD Point" label="Land Cover Point" />
                            <FormSelectOption key="Population Density Map" value="Population Density Map" label="Population Density Map" />
                            <FormSelectOption key="Urban" value="Urban" label="Urban" />
                            <FormSelectOption key="Suburban" value="Suburban" label="Suburban" />
                            <FormSelectOption key="Rural" value="Rural" label="Rural" />
                        </FormSelect>
                        {this.props.config.propagationEnv == 'NLCD Point' ?
                            <FormGroup label="Land Cover Database" fieldId="nlcd-database">
                                <FormSelect
                                    value={this.props.config.nlcdFile ?? this.getDefaultLandCoverDatabase()}
                                    onChange={(x) => this.setNlcdFile(x)}
                                    id="nlcd-database"
                                    name="nlcd-database"
                                    style={{ textAlign: "right" }}
                                >
                                    {this.getLandCoverOptions()}
                                </FormSelect>
                            </FormGroup>
                            : <></>}
                    </FormGroup>
                </GalleryItem>}
            {this.props.config.propagationModel.kind != "FSPL" ?
                <GalleryItem>
                    <ITMParametersForm data={{...this.props.config.ITMParameters}} onChange={this.setITMParams} />
                </GalleryItem>
                : false}
            <GalleryItem>
                <Tooltip
                    position={TooltipPosition.top}
                    enableFlip={true}
                    //className="prop-env-tooltip"
                    maxWidth="40.0rem"
                    content={
                        <p>When distance &gt; 1km and FS Receiver (Rx) is in Urban/Suburban, P.2108 clutter loss is added at
                            FS Rx when FS Receiver AGL height &lt; <b>Max FS AGL Height</b></p>
                    }
                >
                    <FormGroup fieldId="horizontal-form-clutter">
                        <InputGroup label="">
                            <Checkbox
                                label="Add Clutter at FS Receiver"
                                isChecked={this.props.config.clutterAtFS}
                                onChange={this.setUseClutter}
                                id="horizontal-form-clutter"
                                name="horizontal-form-clutter"
                                style={{ textAlign: "right" }}
                            />

                            <OutlinedQuestionCircleIcon style={{ marginLeft: "5px" }} />
                        </InputGroup>
                    </FormGroup>
                </Tooltip>
                {this.props.config.clutterAtFS == true ?
                    <FormGroup fieldId="fsclutter-p2180-confidence" label="P.2108 Percentage of Locations" >

                        <InputGroup>
                            <TextInput
                                type="number"
                                id="fsclutter-p2180-confidence"
                                name="fsclutter-p2180-confidence"
                                isValid={true}
                                value={this.props.config.fsClutterModel!.p2108Confidence}
                                onChange={this.setFsClutterConfidence}
                                style={{ textAlign: "right" }} />
                            <InputGroupText>%</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                    : <></>}
                {this.props.config.clutterAtFS == true ?
                    <FormGroup fieldId="fsclutter-maxFsAglHeight"
                        label="Max FS AGL Height (m)"
                    >
                        <InputGroup>
                            <TextInput
                                type="number"
                                id="fsclutter-maxFsAglHeight"
                                name="fsclutter-maxFsAglHeight"
                                isValid={true}
                                value={this.props.config.fsClutterModel!.maxFsAglHeight}
                                onChange={this.setFsClutterMaxHeight}
                                style={{ textAlign: "right" }} />
                            <InputGroupText>m</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                    : <></>}
            </GalleryItem>
            <GalleryItem>
                <AllowedRangesDisplay data={this.props.config.freqBands} region={this.trimmedRegionStr() ?? "US"} />
            </GalleryItem>
            <GalleryItem>
                <FormGroup label="AP Height below Min Allowable AGL Height Behavior"
                    fieldId="horizontal-form-uls-scanBelowGround">
                    {" "}<Tooltip
                        position={TooltipPosition.top}
                        enableFlip={true}
                        maxWidth="40.0rem"
                        content={
                            <>
                                <p>Min allowable AGL height = 1.5 m</p>
                                <p>Note that this is meant to mainly prevent the portion of uncertainty region to be underground due to height uncertainty and terrain variation.</p>
                                <p>If the AGL height of all scan points is below ground (without height uncertainty), an error will be reported.</p>
                            </>
                        }
                    >
                        <OutlinedQuestionCircleIcon />
                    </Tooltip>

                    <FormSelect
                        value={this.props.config.scanPointBelowGroundMethod}
                        onChange={x => this.setScanBelowGround(x)}
                        id="horizontal-form-uls-scanBelowGround"
                        name="horizontal-form-uls-scanBelowGround"
                        isValid={!!this.props.config.scanPointBelowGroundMethod}
                        style={{ textAlign: "right" }}
                    >
                        <FormSelectOption key={"truncate"} value={"truncate"} label={"Truncate AP height to min allowable AGL height"} />
                        <FormSelectOption key={"discard"} value={"discard"} label={"Discard scan point"} />
                    </FormSelect>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                <Tooltip
                    position={TooltipPosition.top}
                    enableFlip={true}
                    //className="prop-env-tooltip"
                    maxWidth="40.0rem"
                    content={
                        <p>If enabled, the Virtual AP page will add map information via the Vendor extension and present a Google Map on the page</p>
                    }
                >
                    <FormGroup fieldId="horizontal-form-clutter">
                        <InputGroup label="">
                            <Checkbox
                                label="Enable Map in Virtual AP"
                                isChecked={this.props.config.enableMapInVirtualAp ?? false}
                                onChange={this.setEnableMapInVirtualAp}
                                id="horizontal-form-clutter"
                                name="horizontal-form-clutter"
                                style={{ textAlign: "right" }}
                            />

                            <OutlinedQuestionCircleIcon style={{ marginLeft: "5px" }} />
                        </InputGroup>
                    </FormGroup>
                </Tooltip>
            </GalleryItem>
            <GalleryItem>
                <Tooltip
                    position={TooltipPosition.top}
                    enableFlip={true}
                    //className="prop-env-tooltip"
                    maxWidth="40.0rem"
                    content={
                        <p>If enabled, the EIRP and PSD values will be rounded down to one decimal place</p>
                    }
                >
                    <FormGroup fieldId="horizontal-form-roundins">
                        <InputGroup label="">
                            <Checkbox
                                label="Round EIRP and PSD values"
                                isChecked={this.props.config.roundPSDEIRPFlag ?? false}
                                onChange={this.setRoundPSDEIRPFlag}
                                id="horizontal-form-clutter"
                                name="horizontal-form-clutter"
                                style={{ textAlign: "right" }}
                            />

                            <OutlinedQuestionCircleIcon style={{ marginLeft: "5px" }} />
                        </InputGroup>
                    </FormGroup>
                </Tooltip>
            </GalleryItem>
            <GalleryItem>
                <FormGroup fieldId="horizontal-form-channelResponse" label="Channel Response Algorithm">
                    <InputGroup id="">
                        <Radio
                            label="Total power within FS band"
                            isChecked={this.props.config.channelResponseAlgorithm === 'pwr'}
                            onChange={(b) => b && this.setChannelResponseAlgorithm('pwr')}
                            id="horizontal-form-channelResponse-pwr"
                            name="horizontal-form-channelResponse-pwr"
                            style={{ textAlign: "left" }}
                            value='pwr'

                        />
                    </InputGroup>
                    <InputGroup>
                        <Radio
                            label="Max PSD over FS band"
                            isChecked={this.props.config.channelResponseAlgorithm === 'psd'}
                            onChange={(b) => b && this.setChannelResponseAlgorithm('psd')}
                            id="horizontal-form-channelResponse-psd"
                            name="horizontal-form-channelResponse-psd"
                            style={{ textAlign: "left" }}
                            value='psd'
                        />

                    </InputGroup>
                </FormGroup>

            </GalleryItem>
            <GalleryItem>
                <FormGroup
                    label="Visiblilty Threshold"
                    fieldId="visiblity-threshold"
                >
                    <InputGroup><TextInput
                        value={this.props.config.visibilityThreshold}
                        type="number"
                        onChange={(x) => this.setVisiblityThreshold(Number(x))}
                        id="visiblity-threshold"
                        name="visiblity-threshold"
                        style={{ textAlign: "right" }}
                        isValid={!!this.props.config.visibilityThreshold || this.props.config.visibilityThreshold === 0} />
                        <InputGroupText>dB I/N</InputGroupText></InputGroup>
                </FormGroup>
            </GalleryItem>
            <GalleryItem>
                {/* <Tooltip
                    position={TooltipPosition.top}
                    enableFlip={true}
                    //className="prop-env-tooltip"
                    maxWidth="40.0rem"
                    content={
                        <p>If enabled, the Virtual AP page will add map information via the Vendor extension and present a Google Map on the page</p>
                    }
                > */}
                <FormGroup fieldId="horizontal-form-clutter">
                    <InputGroup label="">
                        <Checkbox
                            label="Nearfield Adjustment Factor"
                            isChecked={this.props.config.nearFieldAdjFlag ?? false}
                            onChange={this.setNearFieldAdjFlag}
                            isDisabled={true}
                            id="horizontal-form-clutter"
                            name="horizontal-form-clutter"
                            style={{ textAlign: "right" }}
                        />

                        {/* <OutlinedQuestionCircleIcon style={{ marginLeft: "5px" }} /> */}
                    </InputGroup>
                </FormGroup>
                {/* </Tooltip> */}
            </GalleryItem>
        </>

}