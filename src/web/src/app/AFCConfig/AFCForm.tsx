import * as React from "react";
import { FormGroup, InputGroup, TextInput, InputGroupText, FormSelect, FormSelectOption, ActionGroup, Checkbox, Button, AlertActionCloseButton, Alert, Gallery, GalleryItem, Card, CardBody, Modal, TextArea, ClipboardCopy, ClipboardCopyVariant, Tooltip, TooltipPosition  } from "@patternfly/react-core";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import BuildlingPenetrationLossForm from "./BuildingPenetrationLossForm";
import PolarizationMismatchLossForm from "./PolarizationMismatchLossForm";
import { ITMParametersForm } from "./ITMParametersForm";
import BodyLossForm from "./BodyLossForm";
import AntennaPatternForm from "./AntennaPatternForm";
import { APUncertaintyForm } from "./APUncertaintyForm"
import { PropogationModelForm } from "./PropogationModelForm";
import { AFCConfigFile, PenetrationLossModel, PolarizationLossModel, BodyLossModel, AntennaPattern, RatResponse, PropagationModel, APUncertainty, ITMParameters, FSReceiverFeederLoss, FSReceiverNoise } from "../Lib/RatApiTypes";
import { getDefaultAfcConf, guiConfig } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { Limit } from "../Lib/Admin";

/**
* AFCForm.tsx: form for generating afc configuration files to be used to update server
* author: Sam Smucny
*/

/**
 * Form component for AFC Config
 */
export class AFCForm extends React.Component<
    {
        limit: Limit,
        config: AFCConfigFile,
        ulsFiles: string[],
        antennaPatterns: string[],
        onSubmit: (x: AFCConfigFile) => Promise<RatResponse<string>>
    },
    {
        config: AFCConfigFile,
        isModalOpen: boolean,
        messageSuccess: string | undefined,
        messageError: string | undefined
    }
    > {

    constructor(props: Readonly<{ limit: Limit, config: AFCConfigFile; ulsFiles: string[]; antennaPatterns: string[]; onSubmit: (x: AFCConfigFile) => Promise<RatResponse<string>>; }>) {
        super(props);
        this.state = { config: props.config as AFCConfigFile, messageError: undefined, messageSuccess: undefined, isModalOpen: false }
    }

    private hasValue = (val: any) => val !== undefined && val !== null;

    private setMinEIRP = (n: number) => this.setState({ config: Object.assign(this.state.config, { minEIRP: n }) });
    private setMaxEIRP = (n: number) => this.setState({ config: Object.assign(this.state.config, { maxEIRP: n }) });
    private setFeederLoss = (f: FSReceiverFeederLoss) => this.setState({ config: Object.assign(this.state.config, { receiverFeederLoss: f }) });
    private setReceiverNoise = (f: FSReceiverNoise) => this.setState({ config: Object.assign(this.state.config, { fsReceiverNoise: f }) });
    private setThreshold = (n: number) => this.setState({ config: Object.assign(this.state.config, { threshold: n }) });
    private setMaxLinkDistance = (n: number) => this.setState({ config: Object.assign(this.state.config, { maxLinkDistance: n }) });
    private setUlsDatabase = (n: string) => this.setState({ config: Object.assign(this.state.config, { ulsDatabase: n }) });
    private setUlsRegion = (n: string) => this.setState({ config: Object.assign(this.state.config, { regionStr: n , ulsDatabase: ""}) });
    private setPropogationEnv = (n: string) => this.setState({ config: Object.assign(this.state.config, { propagationEnv: n }) });


    private isValid = () => {

        const err = (s?: string) => ({ isError: true, message: s || "One or more inputs are invalid" });
        const model = this.state.config;

        if(model.APUncertainty.horizontal <= 0 || model.APUncertainty.height <= 0) return err();
        if(model.ITMParameters.minSpacing <1 || model.ITMParameters.minSpacing > 30) return err('Path Min Spacing must be between 1 and 30')        
        if(model.ITMParameters.maxPoints <100 || model.ITMParameters.maxPoints > 10000) return err('Path Max Points must be between 100 and 10000')        


        if (!model.ulsDatabase) return err();
        if (!model.propagationEnv) return err();

        if (!(model.minEIRP <= model.maxEIRP)) return err();

        if(this.props.limit.enforce && model.minEIRP < this.props.limit.limit || model.maxEIRP < this.props.limit.limit) {
            return err("EIRP value must be at least " + this.props.limit.limit + " dBm")
        }

        if (!(model.maxLinkDistance >= 1)) return err();

        switch (model.buildingPenetrationLoss.kind) {
            case "ITU-R Rec. P.2109":
                if (!model.buildingPenetrationLoss.buildingType) return err();
                if (model.buildingPenetrationLoss.confidence > 100 || model.buildingPenetrationLoss.confidence < 0) return err();
                break;
            case "Fixed Value":
                if (model.buildingPenetrationLoss.value === undefined) return err();
                break;
            default:
                return err();
        }

        switch (model.antennaPattern.kind) {
            case "User Upload":
                if (!model.antennaPattern.value) return err();
                break;
            case "F.1245":
                break;
            default:
                return err();
        }

        const propModel = model.propagationModel;
        switch (propModel.kind) {
            case "ITM with no building data":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.win2Confidence < 0 || propModel.win2Confidence > 100) return err();
                if (propModel.win2ProbLosThreshold < 0 || propModel.win2ProbLosThreshold > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.terrainSource != "SRTM (90m)" && propModel.terrainSource != "3DEP (30m)") return err();
                break;
            case "ITM with building data":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D") return err();
                break;
            case "FCC 6GHz Report & Order":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.win2Confidence < 0 || propModel.win2Confidence > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D" && propModel.buildingSource != "None") return err();
                if (propModel.terrainSource != "3DEP (30m)") return err("Invalid terrain source.");
                break;
            case "FSPL":
                break;
            case "Ray Tracing":
                break;
            default:
                return err();
        }

        return {
            isError: false,
            message: undefined
        }
    }

    /**
     * Push completed form to parent if valid
     */
    private submit = () => {
        const isValid = this.isValid();
        if (isValid.isError) {
            this.setState({ messageError: isValid.message, messageSuccess: undefined });
            return;
        }
        this.props.onSubmit(this.state.config).then(
            res => {
                if (res.kind === "Success") {
                    this.setState({ messageSuccess: res.result, messageError: undefined });
                } else {
                    this.setState({ messageError: res.description, messageSuccess: undefined });
                }
            }
        )
    }

    private reset = () => {
        this.setState({ config: getDefaultAfcConf() });
    }

    private setConfig = (newConf: string) => {
        try {
            const parsed = JSON.parse(newConf) as AFCConfigFile;
            if (parsed.version == guiConfig.version) {
                this.setState({ config: Object.assign(this.state.config, parsed) });
            } else {
                this.setState({ messageError: "The imported configuration (version: " + parsed.version + ") is not compatible with the current version (" + guiConfig.version +")." })
            }
        } catch (e) {
            logger.error("Pasted value was not valid JSON");
        }
    }

    private getConfig = () => JSON.stringify(this.state.config);

    render() {

        const setPenetrationLoss = (x: PenetrationLossModel) => {
            const conf = this.state.config;
            conf.buildingPenetrationLoss = x;
            this.setState({ config: conf });
        }

        const setPolarizationLoss = (x: PolarizationLossModel) => {
            const conf = this.state.config;
            conf.polarizationMismatchLoss = x;
            this.setState({ config: conf });
        }

        const setBodyLoss = (x: BodyLossModel) => {
            const conf = this.state.config;
            conf.bodyLoss = x;
            this.setState({ config: conf });
        }

        const setAntennaPattern = (x: AntennaPattern) => {
            const conf = this.state.config;
            conf.antennaPattern = x;
            this.setState({ config: conf });
        }

        const setPropogationModel = (x: PropagationModel) => {
            const conf = this.state.config;
            conf.propagationModel = x;
            this.setState({ config: conf });
        }

        const setAPUncertainty = (x: APUncertainty) => {
            const conf = this.state.config;
            conf.APUncertainty = x;
            this.setState({ config: conf });
        }

        const setITMParams = (x: ITMParameters) => {
            const conf = this.state.config;
            conf.ITMParameters = x;
            this.setState({ config: conf });
        }

        const setUseClutter = (x: Boolean) => {
            const conf = this.state.config;
            conf.clutterAtFS = x;
            this.setState({ config: conf });
        }

        return (
            <Card>
                <Modal
                    title="Copy/Paste"
                    isLarge={true}
                    isOpen={this.state.isModalOpen}
                    onClose={() => this.setState({ isModalOpen: false })}
                    actions={[<Button key="update" variant="primary" onClick={() => this.setState({ isModalOpen: false })}>Close</Button>]}>
                    <ClipboardCopy variant={ClipboardCopyVariant.expansion} onChange={(v: string) => this.setConfig(v)} aria-label="text area">{this.getConfig()}</ClipboardCopy>
                </Modal>
                <CardBody>
                    <Gallery gutter="sm">
                        <GalleryItem>
                            <FormGroup label="Country" fieldId="horizontal-form-uls-region">
                                <FormSelect
                                    value={this.state.config.regionStr}
                                    onChange={x => this.setUlsRegion(x)}
                                    id="horizontal-form-uls-region"
                                    name="horizontal-form-uls-region"
                                    isValid={!!this.state.config.regionStr}
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption key={undefined} value={undefined} label="Select a Country" />
                                        <FormSelectOption key={"CONUS"} value={"CONUS"} label={"CONUS"} />
                                        <FormSelectOption key={"Canada"} value={"Canada"} label={"Canada"} />
                                    </FormSelect>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <FormGroup label="ULS Database" fieldId="horizontal-form-uls-db">
                                <FormSelect
                                    value={this.state.config.ulsDatabase}
                                    onChange={x => this.setUlsDatabase(x)}
                                    id="horizontal-form-uls-db"
                                    name="horizontal-form-uls-db"
                                    isValid={!!this.state.config.ulsDatabase}
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a ULS Database" />
                                    {this.props.ulsFiles.filter((file : string) => file.includes(this.state.config.regionStr)).map((option: string) => (
                                        <FormSelectOption key={option} value={option} label={option} />
                                    ))}
                                </FormSelect>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <FormGroup label={this.props.limit.enforce ? "AP/Client Min. EIRP (Min: " + this.props.limit.limit + " dBm)" : "AP/Client Min. EIRP"} fieldId="horizontal-form-min-eirp">
                                <InputGroup>
                                    <TextInput
                                        value={this.state.config.minEIRP}
                                        onChange={x => this.setMinEIRP(Number(x))}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-min-eirp"
                                        name="horizontal-form-min-eirp"
                                        isValid={this.props.limit.enforce ? this.state.config.minEIRP <= this.state.config.maxEIRP && this.state.config.minEIRP >= this.props.limit.limit : this.state.config.minEIRP <= this.state.config.maxEIRP}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dBm</InputGroupText>
                                </InputGroup>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <FormGroup label="AP/Client Max. EIRP" fieldId="horizontal-form-max-eirp">
                                <InputGroup>
                                    <TextInput
                                        value={this.state.config.maxEIRP}
                                        onChange={x => this.setMaxEIRP(Number(x))}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-max-eirp"
                                        name="horizontal-form-max-eirp"
                                        isValid={this.props.limit.enforce ? this.state.config.minEIRP <= this.state.config.maxEIRP && this.state.config.maxEIRP >= this.props.limit.limit : this.state.config.minEIRP <= this.state.config.maxEIRP}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dBm</InputGroupText>
                                </InputGroup>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <BuildlingPenetrationLossForm
                                data={this.state.config.buildingPenetrationLoss}
                                onChange={setPenetrationLoss} />
                        </GalleryItem>
                        <GalleryItem>
                            <PolarizationMismatchLossForm
                                data={this.state.config.polarizationMismatchLoss}
                                onChange={setPolarizationLoss} />
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
                                        id="feeder-5-label"
                                        name="feeder-5-label"
                                        isReadOnly={true}
                                        value="UNII-5"
                                        style={{ textAlign: "left", minWidth: "35%" }}
                                    /> 
                                    <TextInput
                                        value={this.state.config.receiverFeederLoss.UNII5}
                                        onChange={x => this.setFeederLoss({...this.state.config.receiverFeederLoss, UNII5: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-receiver-feeder-loss-5"
                                        name="horizontal-form-receiver-feeder-loss-5"
                                        isValid={this.hasValue(this.state.config.receiverFeederLoss.UNII5)}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dB</InputGroupText>
                                </InputGroup>
                                <InputGroup>
                                    <TextInput
                                        id="feeder-7-label"
                                        name="feeder-7-label"
                                        isReadOnly={true}
                                        value="UNII-7"
                                        style={{ textAlign: "left", minWidth: "35%" }}
                                    /> 
                                    <TextInput
                                        value={this.state.config.receiverFeederLoss.UNII7}
                                        onChange={x => this.setFeederLoss({...this.state.config.receiverFeederLoss, UNII7: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-receiver-feeder-loss-7"
                                        name="horizontal-form-receiver-feeder-loss-7"
                                        isValid={this.hasValue(this.state.config.receiverFeederLoss.UNII7)}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dB</InputGroupText>
                                </InputGroup>    
                                <InputGroup>
                                    <TextInput
                                        id="feeder-o-label"
                                        name="feeder-o-label"
                                        isReadOnly={true}
                                        value="Other"
                                        style={{ textAlign: "left", minWidth: "35%" }}
                                    /> 
                                    <TextInput
                                        value={this.state.config.receiverFeederLoss.other}
                                        onChange={x => this.setFeederLoss({...this.state.config.receiverFeederLoss, other: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-receiver-feeder-loss-o"
                                        name="horizontal-form-receiver-feeder-loss-o"
                                        isValid={this.hasValue(this.state.config.receiverFeederLoss.other)}
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
                                        value={this.state.config.fsReceiverNoise.UNII5}
                                        onChange={x => this.setReceiverNoise({...this.state.config.fsReceiverNoise, UNII5: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-noise-5"
                                        name="horizontal-form-noise-5"
                                        isValid={this.hasValue(this.state.config.fsReceiverNoise.UNII5)}
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
                                        value={this.state.config.fsReceiverNoise.UNII7}
                                        onChange={x => this.setReceiverNoise({...this.state.config.fsReceiverNoise, UNII7: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-noise-7"
                                        name="horizontal-form-noise-7"
                                        isValid={this.hasValue(this.state.config.fsReceiverNoise.UNII7)}
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
                                        value={this.state.config.fsReceiverNoise.other}
                                        onChange={x => this.setReceiverNoise({...this.state.config.fsReceiverNoise, other: Number(x)})}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-receiver-feeder-loss-o"
                                        name="horizontal-form-receiver-feeder-loss-o"
                                        isValid={this.hasValue(this.state.config.fsReceiverNoise.other)}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dBm/MHz</InputGroupText>
                                </InputGroup>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <BodyLossForm
                                data={this.state.config.bodyLoss}
                                onChange={setBodyLoss} />
                        </GalleryItem>
                        <GalleryItem>
                            <FormGroup label="I/N Threshold" fieldId="horizontal-form-threshold">
                                <InputGroup>
                                    <TextInput
                                        value={this.state.config.threshold}
                                        onChange={x => this.setThreshold(Number(x))}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-threshold"
                                        name="horizontal-form-threshold"
                                        isValid={this.hasValue(this.state.config.threshold)}
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
                                    value={this.state.config.maxLinkDistance}
                                    type="number"
                                    onChange={(x) => this.setMaxLinkDistance(Number(x))}
                                    id="propogation-model-max-link-distance"
                                    name="propogation-model-max-link-distance"
                                    style={{ textAlign: "right" }}
                                    isValid={this.state.config.maxLinkDistance >= 1} />
                                    <InputGroupText>km</InputGroupText></InputGroup>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <AntennaPatternForm
                                data={this.state.config.antennaPattern}
                                antennaPatternFiles={this.props.antennaPatterns}
                                onChange={setAntennaPattern} />
                        </GalleryItem>
                        <GalleryItem>
                            <APUncertaintyForm
                                data={this.state.config.APUncertainty}
                                onChange={setAPUncertainty} />
                        </GalleryItem>
                        <GalleryItem>
                            <PropogationModelForm
                                data={this.state.config.propagationModel}
                                onChange={setPropogationModel} />
                        </GalleryItem>
                        {(this.state.config.propagationModel.kind === "ITM with no building data" || this.state.config.propagationModel.kind == "FCC 6GHz Report & Order") &&
                            <GalleryItem>
                                <FormGroup
                                    label="AP/Client Propagation Environment"
                                    fieldId="propogation-env"
                                >
                                    {" "}<Tooltip
                                    position={TooltipPosition.top}
                                    enableFlip={true}
                                    //className="prop-env-tooltip"
                                    maxWidth="40.0rem"
                                    content={
                                        <>
                                            <p>AP/Client Propagation Environment:</p>
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
                                    value={this.state.config.propagationEnv}
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
                                </FormGroup>
                            </GalleryItem>}
                        {this.state.config.propagationModel.kind != "FSPL" ?  
                            <GalleryItem>
                                <ITMParametersForm data={this.state.config.ITMParameters} onChange={setITMParams}/>
                            </GalleryItem>
                        : false}
                        <GalleryItem>
                            <Tooltip
                                position={TooltipPosition.top}
                                enableFlip={true}
                                //className="prop-env-tooltip"
                                maxWidth="40.0rem"
                                content={
                                <p>When distance &gt; 1km, clutter loss is added at FS Receiver when FS Receiver AGL height &lt;10m. Clutter models are the same as those used for APs.​</p>
                                    
                                }
                            >
                                <InputGroup label=""> 
                                    <Checkbox
                                            label="Add Clutter at FS Receiver"
                                            isChecked={this.state.config.clutterAtFS}
                                            onChange={setUseClutter}
                                            id="horizontal-form-clutter"
                                            name="horizontal-form-clutter"
                                            style={{ textAlign: "right" }}
                                    /> 
                                   
                                    <OutlinedQuestionCircleIcon style={{marginLeft: "5px"}}/>
                                    
                                </InputGroup>
                            </Tooltip>                       
                        </GalleryItem>
                    </Gallery>
                    <br />
                    <>
                        {this.state.messageError !== undefined && (
                            <Alert
                                variant="danger"
                                title="Error"
                                action={<AlertActionCloseButton onClose={() => this.setState({ messageError: undefined })} />}
                            >{this.state.messageError}</Alert>
                        )}
                    </>
                    <>
                        {this.state.messageSuccess !== undefined && (
                            <Alert
                                variant="success"
                                title="Success"
                                action={<AlertActionCloseButton onClose={() => this.setState({ messageSuccess: undefined })} />}
                            >{this.state.messageSuccess}</Alert>
                        )}
                    </>
                    <br />
                    <>
                        <Button variant="primary" onClick={this.submit}>Update Configuration File</Button>{" "}
                        <Button variant="secondary" onClick={this.reset}>Reset Form to Default</Button>{" "}
                        <Button key="open-modal" variant="secondary" onClick={() => this.setState({ isModalOpen: true })}>Copy/Paste</Button>
                    </>
                </CardBody>
            </Card>);
    }
}