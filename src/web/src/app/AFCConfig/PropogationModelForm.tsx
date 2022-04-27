import * as React from "react";
import { FormGroup, FormSelect, FormSelectOption, TextInput, InputGroup, InputGroupText, Tooltip, TooltipPosition } from "@patternfly/react-core";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import { CustomPropagation, PropagationModel } from "../Lib/RatApiTypes";

/**
 * PropogationModelForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Enumeration of propogation models
 */
const propogationModels = [
    "FSPL",
    "ITM with building data",
    //"ITM with no building data",
    "FCC 6GHz Report & Order",
    "Ray Tracing",
    "Custom"
]


/**
 * Sub for for propogation model
 */
export class PropogationModelForm extends React.PureComponent<{ data: PropagationModel, onChange: (x: PropagationModel) => void }> {
    private setKind = (s: string) => {
        switch (s) {
            case "FSPL":
                this.props.onChange({ kind: s });
                break;
            case "ITM with building data":
                this.props.onChange({ kind: s, win2ProbLosThreshold: 10, win2Confidence: 50, itmConfidence: 50, p2108Confidence: 50, buildingSource: "LiDAR" });
                break;
            case "ITM with no building data":
                this.props.onChange({ kind: s, win2ProbLosThreshold: 10, win2Confidence: 50, itmConfidence: 50, p2108Confidence: 50, terrainSource: "SRTM (90m)" });
                break;
            case "FCC 6GHz Report & Order":
                this.props.onChange({ kind: s, win2Confidence: 50, itmConfidence: 50, p2108Confidence: 50, buildingSource: "LiDAR", terrainSource: "3DEP (30m)" });
                break;
            case "Ray Tracing":
                break; // do nothing
            case "Custom":
                this.props.onChange({
                    kind: s, winner2LOSOption: "UNKNOWN", win2Confidence: 50, itmConfidence: 50,
                    p2108Confidence: 50, rlanITMTxClutterMethod: "FORCE_TRUE", buildingSource: "None", terrainSource: "3DEP (30m)"
                })
                break;
        }
    }

    setWin2Confidence = (s: string) => {
        const n: number = Number(s);
        this.props.onChange(Object.assign(this.props.data, { win2Confidence: n }));
    }

    setItmConfidence = (s: string) => {
        const n: number = Number(s);
        this.props.onChange(Object.assign(this.props.data, { itmConfidence: n }));
    }
    setP2108Confidence = (s: string) => {
        const n: number = Number(s);
        this.props.onChange(Object.assign(this.props.data, { p2108Confidence: n }));
    }

    setProbLOS = (s: string) => {
        const n: number = Number(s);
        this.props.onChange(Object.assign(this.props.data, { win2ProbLosThreshold: n }));
    }

    setBuildingSource = (s: string) => {
        const model = this.props.data;
        if (model.hasOwnProperty('buildingSource')) {
            // Ensure that there is terrain source is set to default when there is building data
            if (model.buildingSource === "None" && s !== "None") {
                this.setTerrainSource("3DEP (30m)");
            }
            this.props.onChange(Object.assign(this.props.data, { buildingSource: s }));
        }
    }

    setTerrainSource = (s: string) => {
        this.props.onChange(Object.assign(this.props.data, { terrainSource: s }));
    }

    setLosOption = (s: string) => {
        const model = this.props.data as CustomPropagation;

        if (model.winner2LOSOption === "BLDG_DATA_REQ_TX" && s !== "BLDG_DATA_REQ_TX") {
            this.props.onChange(Object.assign(this.props.data, {
                winner2LOSOption: s,
                buildingSource: "None",
                terrainSource: "3DEP (30m)"
            }));
        } else {
            this.props.onChange(Object.assign(this.props.data, { winner2LOSOption: s }));
        }
    }

    setItmClutterMethod = (s: string) => {
        const model = this.props.data as CustomPropagation;
        if (model.rlanITMTxClutterMethod === "BLDG_DATA" && s !== "BLDG_DATA") {
            this.props.onChange(Object.assign(this.props.data, {
                rlanITMTxClutterMethod: s,
                buildingSource: "None",
                terrainSource: "3DEP (30m)"
            }));
        } else {
            this.props.onChange(Object.assign(this.props.data, { rlanITMTxClutterMethod: s }));
        }
    }


    getExpansion = () => {
        const model = this.props.data;
        switch (model.kind) {
            case "FSPL":
                return <></>;
            case "ITM with building data":
                return <>
                    <FormGroup
                        label="ITM Confidence"
                        fieldId="propogation-model-itm-confidence"
                    >
                        <InputGroup><TextInput
                            value={model.itmConfidence}
                            type="number"
                            onChange={this.setItmConfidence}
                            id="propogation-model-itm-confidence"
                            name="propogation-model-itm-confidence"
                            style={{ textAlign: "right" }}
                            isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Building Data Source"
                        fieldId="propogation-model-data-source"
                    >
                        <FormSelect
                            value={model.buildingSource}
                            onChange={this.setBuildingSource}
                            id="propogation-model-data-source"
                            name="propogation-model-data-source"
                            style={{ textAlign: "right" }}
                            isValid={model.buildingSource === "LiDAR" || model.buildingSource === "B-Design3D"}>
                            <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                            <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
                        </FormSelect>
                    </FormGroup>
                </>
            /* case "ITM with no building data":
                return <>
                    <FormGroup
                        label="Winner II Probability Line of Sight Threshold"
                        fieldId="prop-los-threshold"
                    ><InputGroup><TextInput
                        value={model.win2ProbLosThreshold}
                        type="number"
                        onChange={this.setProbLOS}
                        id="prop-los-threshold"
                        name="prop-los-threshold"
                        style={{ textAlign: "right" }}
                        isValid={model.win2ProbLosThreshold >= 0 && model.win2ProbLosThreshold <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Winner II Confidence"
                        fieldId="propogation-model-win-confidence"
                    ><InputGroup><TextInput
                        value={model.win2Confidence}
                        type="number"
                        onChange={this.setWin2Confidence}
                        id="propogation-model-win-confidence"
                        name="propogation-model-win-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.win2Confidence >= 0 && model.win2Confidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="ITM Confidence"
                        fieldId="propogation-model-itm-confidence"
                    ><InputGroup><TextInput
                        value={model.itmConfidence}
                        type="number"
                        onChange={this.setItmConfidence}
                        id="propogation-model-itm-confidence"
                        name="propogation-model-itm-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="P.2108 Percentage of Locations"
                        fieldId="propogation-model-p2108-confidence"
                    ><InputGroup><TextInput
                        value={model.p2108Confidence}
                        type="number"
                        onChange={this.setP2108Confidence}
                        id="propogation-model-p2108-confidence"
                        name="propogation-model-p2108-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Terrain Source"
                        fieldId="terrain-source"
                    >
                        <FormSelect
                            value={model.terrainSource}
                            onChange={this.setTerrainSource}
                            id="terrain-source"
                            name="terrain-source"
                            style={{ textAlign: "right" }}
                            isValid={model.terrainSource === "SRTM (90m)" || model.terrainSource === "3DEP (30m)"}>
                            <FormSelectOption key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
                            <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                        </FormSelect>
                    </FormGroup>
                </> */
            case "FCC 6GHz Report & Order":
                return <>
                    <FormGroup
                        label="Winner II Confidence"
                        fieldId="propogation-model-win-confidence"
                    ><InputGroup><TextInput
                        value={model.win2Confidence}
                        type="number"
                        onChange={this.setWin2Confidence}
                        id="propogation-model-win-confidence"
                        name="propogation-model-win-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.win2Confidence >= 0 && model.win2Confidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="ITM Confidence"
                        fieldId="propogation-model-itm-confidence"
                    ><InputGroup><TextInput
                        value={model.itmConfidence}
                        type="number"
                        onChange={this.setItmConfidence}
                        id="propogation-model-itm-confidence"
                        name="propogation-model-itm-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="P.2108 Percentage of Locations"
                        fieldId="propogation-model-p2108-confidence"
                    ><InputGroup><TextInput
                        value={model.p2108Confidence}
                        type="number"
                        onChange={this.setP2108Confidence}
                        id="propogation-model-p2108-confidence"
                        name="propogation-model-p2108-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Building Data Source"
                        fieldId="propogation-model-data-source"
                    >
                        <FormSelect
                            value={model.buildingSource}
                            onChange={this.setBuildingSource}
                            id="propogation-model-data-source"
                            name="propogation-model-data-source"
                            style={{ textAlign: "right" }}
                            isValid={model.buildingSource === "LiDAR" || model.buildingSource === "B-Design3D" || model.buildingSource === "None"}>
                            <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                            <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
                            <FormSelectOption key="None" value="None" label="None" />
                        </FormSelect>

                    </FormGroup>

                    {model.buildingSource === "None" ?
                        <FormGroup
                            label="Terrain Source"
                            fieldId="terrain-source"
                        >
                            <FormSelect
                                value={model.terrainSource}
                                onChange={this.setTerrainSource}
                                id="terrain-source"
                                name="terrain-source"
                                style={{ textAlign: "right" }}
                                isValid={model.terrainSource === "3DEP (30m)"}>
                                <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                                <FormSelectOption isDisabled={true} key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
                            </FormSelect>
                        </FormGroup>
                        :
                        false}

                </>
            case "Ray Tracing":
                return <></>;
            case "Custom":
                return <>
                    <FormGroup label="WinnerII LoS Option" fieldId="propogation-model-win-los-option">
                        {" "}<Tooltip
                            position={TooltipPosition.top}
                            enableFlip={true}
                            className="propogation-model-win-los-option-tooltip"
                            maxWidth="40.0rem"
                            content={
                                <>
                                    <p>If <strong>LoS/NLoS per building data</strong> is selected, and if the RLAN is in a region where there is building database,
                                        then terrain plus building database is used to determine whether the path is LoS or not.
                                        Otherwise, the Combined LoS/NLoS model is used. By the same logic, if the <strong>Building Data Source</strong> is None,
                                        the Combined LoS/NLoS model is always used. </p>
                                </>
                            }
                        >
                            <OutlinedQuestionCircleIcon />
                        </Tooltip>
                        <FormSelect value={model.winner2LOSOption} onChange={this.setLosOption} id="propogation-model-win-los-option"
                            name="propogation-model-win-los-option" style={{ textAlign: "right" }}
                        >
                            <FormSelectOption key="UNKNOWN" value="UNKNOWN" label="Combined LoS/NLoS" />
                            <FormSelectOption key="FORCE_LOS" value="FORCE_LOS" label="LoS" />
                            <FormSelectOption key="FORCE_NLOS" value="FORCE_NLOS" label="NLoS" />
                            <FormSelectOption key="BLDG_DATA_REQ_TX" value="BLDG_DATA_REQ_TX" label="Los/NLoS per building data" />
                        </FormSelect>
                    </FormGroup>
                    <FormGroup label="WinnerII Confidence" fieldId="propogation-model-win-confidence">
                        <InputGroup>
                            <TextInput value={model.win2Confidence} type="number" onChange={this.setWin2Confidence}
                                id="propogation-model-win-confidence"
                                name="propogation-model-win-confidence"
                                style={{
                                    textAlign: "right"
                                }}
                                isValid={model.win2Confidence >= 0 && model.win2Confidence <= 100} />
                            <InputGroupText>%</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                    <FormGroup label="ITM with Tx Clutter Method" fieldId="propogation-model-itm-tx-clutter">
                        {" "}<Tooltip
                            position={TooltipPosition.top}
                            enableFlip={true}
                            className="propogation-model-itm-tx-clutter-tooltip"
                            maxWidth="40.0rem"
                            content={
                                <>
                                    <p>If <strong>Clutter/No clutter per building</strong> is selected, the path is determined to be LoS or NLoS based on terrain and/or building database (if available).
                                        By the same logic, if the <strong>Building Data Source</strong> is None, blockage is determined based solely on terrain database.</p>
                                </>
                            }
                        >
                            <OutlinedQuestionCircleIcon />
                        </Tooltip>
                        <FormSelect value={model.rlanITMTxClutterMethod} onChange={this.setItmClutterMethod} id="propogation-model-itm-tx-clutter"
                            name="propogation-model-itm-tx-clutter" style={{ textAlign: "right" }}
                        >
                            <FormSelectOption key="FORCE_TRUE" value="FORCE_TRUE" label="Always add clutter" />
                            <FormSelectOption key="FORCE_FALSE" value="FORCE_FALSE" label="Never add clutter" />
                            <FormSelectOption key="BLDG_DATA" value="BLDG_DATA" label="Clutter/No clutter per building data" />
                        </FormSelect>
                    </FormGroup>
                    <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
                        <InputGroup>
                            <TextInput value={model.itmConfidence} type="number" onChange={this.setItmConfidence}
                                id="propogation-model-itm-confidence" name="propogation-model-itm-confidence" style={{
                                    textAlign: "right"
                                }} isValid={model.itmConfidence >= 0 && model.itmConfidence
                                    <= 100} />
                            <InputGroupText>%</InputGroupText>
                        </InputGroup>
                    </FormGroup>
                    <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
                        <InputGroup>
                            <TextInput value={model.p2108Confidence} type="number" onChange={this.setP2108Confidence}
                                id="propogation-model-p2108-confidence" name="propogation-model-p2108-confidence" style={{
                                    textAlign: "right"
                                }} isValid={model.p2108Confidence >= 0 && model.p2108Confidence
                                    <= 100} />
                            <InputGroupText>%</InputGroupText>
                        </InputGroup>
                    </FormGroup>

                    {model.rlanITMTxClutterMethod === "BLDG_DATA" || model.winner2LOSOption === "BLDG_DATA_REQ_TX" ?
                        <>

                            <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
                                <FormSelect value={model.buildingSource} onChange={this.setBuildingSource} id="propogation-model-data-source"
                                    name="propogation-model-data-source" style={{ textAlign: "right" }} isValid={model.buildingSource === "LiDAR"
                                        || model.buildingSource === "B-Design3D" || model.buildingSource === "None"}>
                                    <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                                    <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
                                    <FormSelectOption key="None" value="None" label="None" />
                                </FormSelect>
                            </FormGroup>

                            <FormGroup label="Terrain Source" fieldId="terrain-source">
                                {" "}<Tooltip
                                    position={TooltipPosition.top}
                                    enableFlip={true}
                                    className="fs-feeder-loss-tooltip"
                                    maxWidth="40.0rem"
                                    content={
                                        <>
                                            <p>The higher terrain height resolution (that goes with the building database)
                                                is used instead within the first 1 km (when WinnerII building data is chosen)
                                                and greater than 1 km (when ITM with building data is chosen).</p>
                                        </>
                                    }
                                >
                                    <OutlinedQuestionCircleIcon />
                                </Tooltip>
                                <FormSelect value={model.terrainSource} onChange={this.setTerrainSource} id="terrain-source"
                                    name="terrain-source" style={{ textAlign: "right" }} >
                                    <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                                    <FormSelectOption isDisabled={true} key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />

                                </FormSelect>
                            </FormGroup>
                        </>
                        :
                        false}

                </>
            default: throw "Invalid propogation model";
        }
    }

    render = () =>
        <FormGroup label="Propagation Model" fieldId="horizontal-form-propogation-model">
            {" "}<Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="prop-model-tooltip"
                maxWidth="40.0rem"
                content={
                    <>
                        <p>For "ITM with no building data," the following model is used: </p>
                        <p>Urban/Suburban RLAN:</p>
                        <ul>
                            <li>- Distance from FS &lt; 1 km: Winner II</li>
                            <li>- Distance from FS &gt; 1 km: ITM + P.2108 Clutter</li>
                        </ul>
                        <p>Rural/Barren RLAN: ITM + P.456 Clutter</p>
                        <br />
                        <p>For "FCC 6 GHz Report &amp; Order", the following model is used:</p>
                        <ul>
                            <li>- Path loss &lt; FSPL is clamped to FSPL</li>
                            <li>- Distance &lt; 30m: FSPL</li>
                            <li>- 30m &le; Distance &lt; 1km: Combined Winner II Urban/Suburban/Rural</li>
                            <li>- Distance &gt;= 1km: ITM + [P.2108 Clutter (Urban/Suburban) or P.452 Clutter (Rural)]</li>
                            <li>- For the terrain source, the highest resolution selected terrain source is used (1m LiDAR -&gt; 30m 3DEP -&gt; 90m SRTM -&gt; 1km Globe)</li>
                        </ul>
                    </>
                }
            >
                <OutlinedQuestionCircleIcon />
            </Tooltip>
            <FormSelect
                value={this.props.data.kind}
                onChange={x => this.setKind(x)}
                id="horzontal-form-propogation-model"
                name="horizontal-form-propogation-model"
                isValid={this.props.data.kind !== undefined}
                style={{ textAlign: "right" }}
            >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select Propogation Model" />
                {propogationModels.map((option) => (
                    <FormSelectOption isDisabled={option === "Ray Tracing"} key={option} value={option} label={option} />
                ))}
            </FormSelect>
            {this.getExpansion()}
        </FormGroup>
}
