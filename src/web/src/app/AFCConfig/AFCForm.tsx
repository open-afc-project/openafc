import * as React from "react";
import { FormGroup, InputGroup, TextInput, InputGroupText, FormSelect, FormSelectOption, ActionGroup, Checkbox, Button, AlertActionCloseButton, Alert, Gallery, GalleryItem, Card, CardBody, Modal, TextArea, ClipboardCopy, ClipboardCopyVariant, Tooltip, TooltipPosition, Radio, CardHead, PageSection } from "@patternfly/react-core";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import { AFCConfigFile, PenetrationLossModel, PolarizationLossModel, BodyLossModel, AntennaPatternState, DefaultAntennaType, UserAntennaPattern, RatResponse, PropagationModel, APUncertainty, ITMParameters, FSReceiverFeederLoss, FSReceiverNoise, FreqRange, CustomPropagation, ChannelResponseAlgorithm } from "../Lib/RatApiTypes";
import { getDefaultAfcConf, guiConfig, getAfcConfigFile, putAfcConfigFile, importCache, exportCache, getRegions } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { Limit } from "../Lib/Admin";
import { AllowedRangesDisplay, defaultRanges } from './AllowedRangesForm'
import DownloadContents from "../Components/DownloadContents";
import { AFCFormUSACanada } from "./AFCFormUSACanada";

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
        frequencyBands: FreqRange[],
        config: AFCConfigFile,
        ulsFiles: string[],
        antennaPatterns: string[],
        regions: string[],
        onSubmit: (x: AFCConfigFile) => Promise<RatResponse<string>>
    },
    {
        config: AFCConfigFile,
        isModalOpen: boolean,
        messageSuccess: string | undefined,
        messageError: string | undefined,
        antennaPatternData: AntennaPatternState
    }
> {

    constructor(props: Readonly<{ limit: Limit; frequencyBands: FreqRange[]; config: AFCConfigFile; ulsFiles: string[]; antennaPatterns: string[]; regions: string[]; onSubmit: (x: AFCConfigFile) => Promise<RatResponse<string>>; }>) {
        super(props);
        let config = props.config as AFCConfigFile
        if (props.frequencyBands.length > 0) {
            config.freqBands = props.frequencyBands;
        } else {
            config.freqBands = defaultRanges[config.regionStr ?? "USA"];
        }

        this.state = {
            config: config,
            messageError: undefined,
            messageSuccess: undefined,
            isModalOpen: false,
            antennaPatternData: {
                defaultAntennaPattern: config.ulsDefaultAntennaType,
            }

        }
    }

    private setUlsDatabase = (n: string) => this.setState({ config: Object.assign(this.state.config, { ulsDatabase: n }) });
    private setUlsRegion = (n: string) => {
        this.setState({ config: Object.assign(this.state.config, { regionStr: n }) });
        // region changed by user, reload the coresponding configuration for that region
        getAfcConfigFile(n).then(
            res => {
                if (res.kind === "Success") {
                    this.updateEntireConfigState(res.result);
                    document.cookie = `afc-config-last-region=${n};max-age=2592000;path='/';SameSite=strict`
                } else {
                    if (res.errorCode == 404) {
                        this.updateEntireConfigState(getDefaultAfcConf(n));
                        this.setState({ messageSuccess: "No config found for this region, using region default" });
                    }
                    this.setState({ messageError: res.description, messageSuccess: undefined });
                }
            })
    }

    private isValid = () => {

        const err = (s?: string) => ({ isError: true, message: s || "One or more inputs are invalid" });
        const model = this.state.config;

        if (model.APUncertainty.horizontal <= 0 || model.APUncertainty.height <= 0) return err();
        if (model.ITMParameters.minSpacing < 1 || model.ITMParameters.minSpacing > 30) return err('Path Min Spacing must be between 1 and 30')
        if (model.ITMParameters.maxPoints < 100 || model.ITMParameters.maxPoints > 10000) return err('Path Max Points must be between 100 and 10000')


        if (!model.ulsDatabase) return err();
        if (!model.propagationEnv) return err();

        if (!(model.minEIRP <= model.maxEIRP)) return err();

        if (this.props.limit.enforce && model.minEIRP < this.props.limit.limit || model.maxEIRP < this.props.limit.limit) {
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


        const propModel = model.propagationModel;
        switch (propModel.kind) {
            case "ITM with no building data":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.itmReliability < 0 || propModel.itmReliability > 100) return err();
                if (propModel.win2Confidence < 0 || propModel.win2Confidence > 100) return err();
                if (propModel.win2ProbLosThreshold < 0 || propModel.win2ProbLosThreshold > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.terrainSource != "SRTM (90m)" && propModel.terrainSource != "3DEP (30m)") return err();
                break;
            case "ITM with building data":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.itmReliability < 0 || propModel.itmReliability > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D") return err();
                break;
            case "FCC 6GHz Report & Order":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.itmReliability < 0 || propModel.itmReliability > 100) return err();
                if (propModel.win2ConfidenceCombined < 0 || propModel.win2ConfidenceCombined > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D" && propModel.buildingSource != "None") return err();
                if (propModel.terrainSource != "3DEP (30m)") return err("Invalid terrain source.");
                break;
            case "FSPL":
                break;
            case "Ray Tracing":
                break;
            case "Custom":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.itmReliability < 0 || propModel.itmReliability > 100) return err();
                if (propModel.win2ConfidenceCombined! < 0 || propModel.win2ConfidenceCombined! > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D" && propModel.buildingSource != "None") return err();
                if (propModel.buildingSource !== "None" && propModel.terrainSource != "3DEP (30m)") return err("Invalid terrain source.");
                break;
            case "ISED DBS-06":
                if (propModel.itmConfidence < 0 || propModel.itmConfidence > 100) return err();
                if (propModel.itmReliability < 0 || propModel.itmReliability > 100) return err();
                if (propModel.win2ConfidenceCombined! < 0 || propModel.win2ConfidenceCombined! > 100) return err();
                if (propModel.p2108Confidence < 0 || propModel.p2108Confidence > 100) return err();
                if (propModel.buildingSource != "LiDAR" && propModel.buildingSource != "B-Design3D" && propModel.buildingSource != "None") return err();
                if (propModel.buildingSource !== "None" && propModel.terrainSource != "3DEP (30m)") return err("Invalid terrain source.");
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
     * Use this method when updating the entire configuration as it will also update the antenna pattern subcomponent.  When updating just
     * another item (like just updating the PenatrationLossModel) use setState as normal
     * @param config new configuation file
     */
    private updateEntireConfigState(config: AFCConfigFile) {
        this.setState({
            config: config,
            antennaPatternData: {
                defaultAntennaPattern: config.ulsDefaultAntennaType,
            }
        }
        );
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
        let config = getDefaultAfcConf(this.state.config.regionStr);
        // TODO: work out how to handle allowed frequency band per country or access point
        // if (this.props.frequencyBands.length > 0) {
        //     config.freqBands = this.props.frequencyBands;
        // } else {
        //     config.freqBands = defaultRanges[config.regionStr ?? "USA"];
        // }
        config.freqBands = defaultRanges[config.regionStr ?? "USA"];

        this.updateEntireConfigState(config);
    }

    private setConfig = (newConf: string) => {
        try {
            var escaped = newConf.replace(/\s+/g, ' ')
            const parsed = JSON.parse(escaped) as AFCConfigFile;
            if (parsed.version == guiConfig.version) {
                this.updateEntireConfigState(Object.assign(this.state.config, parsed));
            } else {
                this.setState({ messageError: "The imported configuration (version: " + parsed.version + ") is not compatible with the current version (" + guiConfig.version + ")." })
            }
        } catch (e) {
            logger.error("Pasted value was not valid JSON");
        }
    }

    private getConfig = () => JSON.stringify(this.state.config);

    private export = () =>
        new Blob([JSON.stringify(Object.assign(exportCache(), { afcConfig: this.state.config }))], {
            type: "application/json"
        });


    private import(ev) {
        // @ts-ignore
        const file = ev.target.files[0];
        const reader = new FileReader();
        try {
            reader.onload = async () => {
                try {
                    const value: any = JSON.parse(reader.result as string);
                    if (value.afcConfig) {
                        if (value.afcConfig.version !== guiConfig.version) {
                            const warning: string = "The imported file is from a different version. It has version '"
                                + value.afcConfig.version
                                + "', and you are currently running '"
                                + guiConfig.version
                                + "'.";
                            logger.warn(warning);
                        }
                        const putResp = await putAfcConfigFile(value.afcConfig);
                        if (putResp.kind === "Error") {
                            this.setState({ messageError: putResp.description, messageSuccess: undefined });
                            return;
                        } else {
                            this.updateEntireConfigState(value.afcConfig);
                        }
                    }

                    value.afcConfig = undefined;

                    importCache(value);

                    this.setState({ messageError: undefined, messageSuccess: "Import successful!" });
                } catch (e) {
                    this.setState({ messageError: "Unable to import file", messageSuccess: undefined })
                }
            }

            reader.readAsText(file);

        } catch (e) {
            logger.error("Failed to import application state", e);
            this.setState({ messageError: "Failed to import application state", messageSuccess: undefined });
        }
    }

    updateConfigFromComponent(newState: AFCConfigFile) {
        this.setState({ config: newState });
    }

    updateAntennaDataFromComponent(newState: AntennaPatternState) {
        this.setState({ antennaPatternData: newState });
    }

    render() {

        return (
            <Card>
                <Modal
                    title="Copy/Paste"
                    isLarge={true}
                    isOpen={this.state.isModalOpen}
                    onClose={() => this.setState({ isModalOpen: false })}
                    actions={[<Button key="update" variant="primary" onClick={() => this.setState({ isModalOpen: false })}>Close</Button>]}>
                    <ClipboardCopy variant={ClipboardCopyVariant.expansion} isExpanded onChange={(v: string | number) => this.setConfig(String(v).trim())} aria-label="text area">{this.getConfig()}</ClipboardCopy>
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
                                    {this.props.regions.map((option: string) => (
                                        <FormSelectOption key={option} value={option} label={option} />
                                    ))}
                                </FormSelect>
                            </FormGroup>
                        </GalleryItem>
                        <GalleryItem>
                            <FormGroup label="FS Database" fieldId="horizontal-form-uls-db">
                                {" "}<Tooltip
                                    position={TooltipPosition.top}
                                    enableFlip={true}
                                    className="fs-feeder-loss-tooltip"
                                    maxWidth="40.0rem"
                                    content={
                                        <>
                                            <p>CONUS_ULS_LATEST.sqlite3 refers to the latest stable CONUS FS database .</p>
                                        </>
                                    }
                                >
                                    <OutlinedQuestionCircleIcon />
                                </Tooltip>
                                <FormSelect
                                    value={this.state.config.ulsDatabase}
                                    onChange={x => this.setUlsDatabase(x)}
                                    id="horizontal-form-uls-db"
                                    name="horizontal-form-uls-db"
                                    isValid={!!this.state.config.ulsDatabase}
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select an FS Database" />
                                    {this.props.ulsFiles.map((option: string) => (
                                        <FormSelectOption key={option} value={option} label={option} />
                                    ))}
                                </FormSelect>
                            </FormGroup>
                        </GalleryItem>
                        {(this.state.config.regionStr == "CANADA" || this.state.config.regionStr == "USA") &&
                            <AFCFormUSACanada
                                config={this.state.config}
                                antennaPatterns={this.state.antennaPatternData}
                                frequencyBands={this.props.frequencyBands}
                                limit={this.props.limit}
                                updateConfig={(x) => this.updateConfigFromComponent(x)}
                                updateAntennaData={(x) => this.updateAntennaDataFromComponent(x)} />
                        }
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
                    <br />
                    <br />
                    <FormGroup
                        label="Import Afc Config"
                        fieldId="import-afc-form"
                    >

                        <input
                            // @ts-ignore
                            type="file"
                            name="import afc file"
                            // @ts-ignore
                            onChange={(ev) => this.import(ev)}
                        />
                    </FormGroup>
                    <br />
                    <DownloadContents fileName="Download AFC Config" contents={() => this.export()} />
                    <br />

                </CardBody>
            </Card >);
    }
}
