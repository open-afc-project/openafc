import * as React from "react";
import { FormGroup, InputGroup, TextInput, InputGroupText, FormSelect, FormSelectOption, ActionGroup, Button, AlertActionCloseButton, Alert, Gallery, GalleryItem, Card, CardBody, Modal, TextArea, ClipboardCopy, ClipboardCopyVariant } from "@patternfly/react-core";
import BuildlingPenetrationLossForm from "./BuildingPenetrationLossForm";
import PolarizationMismatchLossForm from "./PolarizationMismatchLossForm";
import BodyLossForm from "./BodyLossForm";
import AntennaPatternForm from "./AntennaPatternForm";
import { PropogationModelForm } from "./PropogationModelForm";
import { AFCConfigFile, PenetrationLossModel, PolarizationLossModel, BodyLossModel, AntennaPattern, RatResponse, PropagationModel } from "../Lib/RatApiTypes";
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
    private setFeederLoss = (n: number) => this.setState({ config: Object.assign(this.state.config, { receiverFeederLoss: n }) });
    private setThreshold = (n: number) => this.setState({ config: Object.assign(this.state.config, { threshold: n }) });
    private setMaxLinkDistance = (n: number) => this.setState({ config: Object.assign(this.state.config, { maxLinkDistance: n }) });
    private setUlsDatabase = (n: string) => this.setState({ config: Object.assign(this.state.config, { ulsDatabase: n }) });
    private setUlsRegion = (n: string) => this.setState({ config: Object.assign(this.state.config, { regionStr: n , ulsDatabase: ""}) });
    private setPropogationEnv = (n: string) => this.setState({ config: Object.assign(this.state.config, { propagationEnv: n }) });

    private isValid = () => {

        const err = (s?: string) => ({ isError: true, message: s || "One or more inputs are invalid" });
        const model = this.state.config;

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
                            <FormGroup label="ULS Region" fieldId="horizontal-form-uls-region">
                                <FormSelect
                                    value={this.state.config.regionStr}
                                    onChange={x => this.setUlsRegion(x)}
                                    id="horizontal-form-uls-region"
                                    name="horizontal-form-uls-region"
                                    isValid={!!this.state.config.regionStr}
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption key={undefined} value={undefined} label="Select a ULS Region" />
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
                                <InputGroup>
                                    <TextInput
                                        value={this.state.config.receiverFeederLoss}
                                        onChange={x => this.setFeederLoss(Number(x))}
                                        type="number"
                                        step="any"
                                        id="horizontal-form-receiver-feeder-loss"
                                        name="horizontal-form-receiver-feeder-loss"
                                        isValid={this.hasValue(this.state.config.receiverFeederLoss)}
                                        style={{ textAlign: "right" }}
                                    />
                                    <InputGroupText>dB</InputGroupText>
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
                            <PropogationModelForm
                                data={this.state.config.propagationModel}
                                onChange={setPropogationModel} />
                        </GalleryItem>
                        {(this.state.config.propagationModel.kind === "ITM with no building data" || this.state.config.propagationModel.kind == "FCC 6GHz Report & Order") &&
                            <GalleryItem>
                                <FormGroup
                                    label="AP/Client Propogation Environment"
                                    fieldId="propogation-env"
                                ><FormSelect
                                    value={this.state.config.propagationEnv}
                                    onChange={(x) => this.setPropogationEnv(x)}
                                    id="propogation-env"
                                    name="propogation-env"
                                    style={{ textAlign: "right" }}
                                    isValid={this.props.config.propagationEnv !== undefined}>
                                        <FormSelectOption key="Population Density Map" value="Population Density Map" label="Population Density Map" />
                                        <FormSelectOption key="Urban" value="Urban" label="Urban" />
                                        <FormSelectOption key="Suburban" value="Suburban" label="Suburban" />
                                        <FormSelectOption key="Rural" value="Rural" label="Rural" />
                                    </FormSelect>
                                </FormGroup>
                            </GalleryItem>}
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