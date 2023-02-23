import * as React from "react";
import { FormGroup, InputGroup, TextInput, InputGroupText, FormSelect, FormSelectOption, ActionGroup, Checkbox, Button, AlertActionCloseButton, Alert, Gallery, GalleryItem, Card, CardBody, Modal, TextArea, ClipboardCopy, ClipboardCopyVariant, Tooltip, TooltipPosition, Radio, CardHead, PageSection } from "@patternfly/react-core";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import BuildlingPenetrationLossForm from "./BuildingPenetrationLossForm";
import PolarizationMismatchLossForm from "./PolarizationMismatchLossForm";
import { ITMParametersForm } from "./ITMParametersForm";
import BodyLossForm from "./BodyLossForm";
import { APUncertaintyForm } from "./APUncertaintyForm"
import { PropogationModelForm } from "./PropogationModelForm";
import { AFCConfigFile, PenetrationLossModel, PolarizationLossModel, BodyLossModel, AntennaPatternState, DefaultAntennaType, UserAntennaPattern, RatResponse, PropagationModel, APUncertainty, ITMParameters, FSReceiverFeederLoss, FSReceiverNoise, FreqRange, CustomPropagation, ChannelResponseAlgorithm, FSClutterModel } from "../Lib/RatApiTypes";
import { Limit } from "../Lib/Admin";
import { AllowedRangesDisplay, defaultRanges } from './AllowedRangesForm'
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



    private hasValue = (val: any) => val !== undefined && val !== null;

    private setMinEIRP = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { minEIRP: n }));
    private setMaxEIRP = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { maxEIRP: n }));
    private setMinPSD = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { minPSD: n }));
    private setFeederLoss = (f: FSReceiverFeederLoss) => this.props.updateConfig(Object.assign(this.props.config, { receiverFeederLoss: f }));
    private setReceiverNoise = (f: FSReceiverNoise) => this.props.updateConfig(Object.assign(this.props.config, { fsReceiverNoise: f }));
    private setThreshold = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { threshold: n }));
    private setMaxLinkDistance = (n: number) => this.props.updateConfig(Object.assign(this.props.config, { maxLinkDistance: n }));
    private setEnableMapInVirtualAp = (n: boolean) => this.props.updateConfig(Object.assign(this.props.config, { enableMapInVirtualAp: n }));
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
        const conf = this.props.config;
        conf.buildingPenetrationLoss = x;
        this.props.updateConfig(conf);
    }

    private setPolarizationLoss = (x: PolarizationLossModel) => {
        const conf = this.props.config;
        conf.polarizationMismatchLoss = x;
        this.props.updateConfig(conf);
    }

    private setBodyLoss = (x: BodyLossModel) => {
        const conf = this.props.config;
        conf.bodyLoss = x;
        this.props.updateConfig(conf);
    }
    private setAntennaPattern = (x: AntennaPatternState) => {
        const conf = this.props.config;
        conf.ulsDefaultAntennaType = x.defaultAntennaPattern;
        this.props.updateConfig(conf);
    }
    private setPropogationModel = (x: PropagationModel) => {
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
            const conf = this.props.config;
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

            conf.propagationModel = x;
            this.props.updateConfig(conf);
        }
    }

    private setAPUncertainty = (x: APUncertainty) => {
        const conf = this.props.config;
        conf.APUncertainty = x;
        this.props.updateConfig(conf);
    }

    private setITMParams = (x: ITMParameters) => {
        const conf = this.props.config;
        conf.ITMParameters = x;
        this.props.updateConfig(conf);
    }

    private setUseClutter = (x: boolean) => {
        const conf = this.props.config;
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
        const newParams:FSClutterModel = { ...conf.fsClutterModel! };
        newParams.p2108Confidence = val
        conf.fsClutterModel = newParams;
        this.props.updateConfig(conf);
    }
    private setFsClutterMaxHeight = (n: number | string) => {
        const val = Number(n);
        const conf = this.props.config;
        const newParams:FSClutterModel = { ...conf.fsClutterModel! };
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

    render = () =>
        <>
            <GalleryItem>
                <FormGroup label={this.props.limit.enforce ? "AP Min. EIRP (Min: " + this.props.limit.limit + " dBm)" : "AP Min. EIRP"} fieldId="horizontal-form-min-eirp">
                    <InputGroup>
                        <TextInput
                            value={this.props.config.minEIRP}
                            onChange={x => this.setMinEIRP(Number(x))}
                            type="number"
                            step="any"
                            id="horizontal-form-min-eirp"
                            name="horizontal-form-min-eirp"
                            isValid={this.props.limit.enforce ? this.props.config.minEIRP <= this.props.config.maxEIRP && this.props.config.minEIRP >= this.props.limit.limit : this.props.config.minEIRP <= this.props.config.maxEIRP}
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
                            isValid={this.props.limit.enforce ? this.props.config.minEIRP <= this.props.config.maxEIRP && this.props.config.maxEIRP >= this.props.limit.limit : this.props.config.minEIRP <= this.props.config.maxEIRP}
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
                                <p>UNII Range:</p>
                                <ul>
                                    <li> UNII-5: 5,925-6,425 MHz</li>
                                    <li> UNII-7: 6,525-6,875 MHz</li>
                                </ul>
                            </>
                        }
                    >
                        <OutlinedQuestionCircleIcon />
                    </Tooltip>
                    <InputGroup>
                        <TextInput
                            id="fs-noise-5-label"
                            name="fs-noise-5-label"
                            isReadOnly={true}
                            value="UNII-5"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.fsReceiverNoise.UNII5}
                            onChange={x => this.setReceiverNoise({ ...this.props.config.fsReceiverNoise, UNII5: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-noise-5"
                            name="horizontal-form-noise-5"
                            isValid={this.hasValue(this.props.config.fsReceiverNoise.UNII5)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm/MHz</InputGroupText>
                    </InputGroup>
                    <InputGroup>
                        <TextInput
                            id="fs-noise-7-label"
                            name="fs-noise-7-label"
                            isReadOnly={true}
                            value="UNII-7"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.fsReceiverNoise.UNII7}
                            onChange={x => this.setReceiverNoise({ ...this.props.config.fsReceiverNoise, UNII7: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-noise-7"
                            name="horizontal-form-noise-7"
                            isValid={this.hasValue(this.props.config.fsReceiverNoise.UNII7)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm/MHz</InputGroupText>
                    </InputGroup>
                    <InputGroup>
                        <TextInput
                            id="fs-noise-o-label"
                            name="fs-noise-o-label"
                            isReadOnly={true}
                            value="Other"
                            style={{ textAlign: "left", minWidth: "35%" }}
                        />
                        <TextInput
                            value={this.props.config.fsReceiverNoise.other}
                            onChange={x => this.setReceiverNoise({ ...this.props.config.fsReceiverNoise, other: Number(x) })}
                            type="number"
                            step="any"
                            id="horizontal-form-receiver-feeder-loss-o"
                            name="horizontal-form-receiver-feeder-loss-o"
                            isValid={this.hasValue(this.props.config.fsReceiverNoise.other)}
                            style={{ textAlign: "right" }}
                        />
                        <InputGroupText>dBm/MHz</InputGroupText>
                    </InputGroup>
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
                    onChange={this.setAntennaPattern} />
            </GalleryItem>
            <GalleryItem>
                <APUncertaintyForm
                    data={this.props.config.APUncertainty}
                    onChange={this.setAPUncertainty} />
            </GalleryItem>
            <GalleryItem>
                <PropogationModelForm
                    data={this.getPropagationModelForForm()}
                    onChange={this.setPropogationModel} />
            </GalleryItem>
            {(this.props.config.propagationModel.kind === "ITM with no building data" || this.props.config.propagationModel.kind == "FCC 6GHz Report & Order") &&
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
                                        <li>- "NLCD Point" assigns propagation environment based on single NLCD tile class the RLAN resides in: Urban if NLCD tile = 23 or 24, Suburban if NLCD tile = 22, Rural-D (Deciduous trees) if NLCD tile = 41, 43 or 90, Rural-C (Coniferous trees) if NLCD tile = 42, and Rural-V (Village Center) otherwise. The various Rural types correspond to the P.452 clutter category.</li>
                                        <li>- If “NLCD Point” is not selected, Village center is assumed for the Rural clutter category.</li>
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
                            <FormSelectOption key="NLCD Point" value="NLCD Point" label="NLCD Point" />
                            <FormSelectOption key="Population Density Map" value="Population Density Map" label="Population Density Map" />
                            <FormSelectOption key="Urban" value="Urban" label="Urban" />
                            <FormSelectOption key="Suburban" value="Suburban" label="Suburban" />
                            <FormSelectOption key="Rural" value="Rural" label="Rural" />
                        </FormSelect>
                        {this.props.config.propagationEnv == 'NLCD Point' ?
                            <FormGroup label="NLCD Database" fieldId="nlcd-database">
                                <FormSelect
                                    value={this.props.config.nlcdFile ?? "nlcd_production"}
                                    onChange={(x) => this.setNlcdFile(x)}
                                    id="nlcd-database"
                                    name="nlcd-database"
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption key="Production NLCD " value="nlcd_production" label="Production NLCD" />
                                    <FormSelectOption key="WFA Test NLCD " value="nlcd_wfa" label="WFA Test NLCD" />
                                </FormSelect>
                            </FormGroup>
                            : <></>}
                    </FormGroup>
                </GalleryItem>}
            {this.props.config.propagationModel.kind != "FSPL" ?
                <GalleryItem>
                    <ITMParametersForm data={this.props.config.ITMParameters} onChange={this.setITMParams} />
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
                <AllowedRangesDisplay data={this.props.frequencyBands} />
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
        </>

}