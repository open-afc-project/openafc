import * as React from "react";
import { FormGroup, FormSelect, FormSelectOption, TextInput, InputGroup, InputGroupText, Tooltip, TooltipPosition, TextArea} from "@patternfly/react-core";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import { ITMParameters, GroundType} from "../Lib/RatApiTypes";

/**
 * ITMParametersForm.tsx: sub form of afc config form
 */


/**
 * Sub form for itm parameters 
 */
export class ITMParametersForm extends React.PureComponent<{ data: ITMParameters, onChange: (x: ITMParameters) => void }> {

    constructor(props) {
        super(props)
    }

    private lookUpConstants = (ground: GroundType) => {
        let dielectric = -1;
        let conductivity = -1;
        switch(ground) {
            case "Average Ground":
                dielectric = 15;
                conductivity = .005; 
                break;
            case "Poor Ground":
                dielectric = 4;
                conductivity = .001; 
                break;
            case "Good Ground":
                dielectric = 25;
                conductivity = .02; 
                break;
            case "Sea Water":
                dielectric = 81;
                conductivity = 5.0; 
                break;
            case "Fresh Water":
                dielectric = 81;
                conductivity = .01; 
                break;
        }
        return {dielectricConst: dielectric, conductivity: conductivity}
    }

    private setPolarization = (s: string) =>
        // @ts-ignore
        this.props.onChange(Object.assign(this.props.data, { polarization: s }));
    
    private setGroundType = (s: string) => {
        let constants = this.lookUpConstants(s as GroundType); 
        // @ts-ignore
        this.props.onChange(Object.assign(this.props.data, {ground: s, dielectricConst: constants.dielectricConst, conductivity: constants.conductivity}));
    }

    private setMinSpacing = (s: string) => {
        // @ts-ignore
        this.props.onChange(Object.assign(this.props.data, {minSpacing: Number(s)}));
    }

    private setMaxPoints = (s: string) => {
        // @ts-ignore
        this.props.onChange(Object.assign(this.props.data, {maxPoints: Number(s)}));
    }

    render = () =>
        <>
        <FormGroup label="ITM Parameters" fieldId="ITM-parameters-form">
        {" "}<Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="prop-model-tooltip"
                maxWidth="40.0rem"
                content={
                    <ul> 
                        <p>
                        ITM Path Profile “Min Spacing” and “Max Points” are used to define the points and their uniform spacing within the RLAN to FS RX path. The spacing is at the minimum equal to “Min Spacing” (for shorter distances) and for larger distances when “Max Points” is reached, it will be larger.
                        </p>
                        <p>Constraints: </p>
                        <li>- "Path Profile Min Spacing" must be between 1m and 30m.​ </li>
                        <li>- "Path Profile Max Points" must be between 100 and 10,000.</li>
                    </ul>
                    
                }
            >
                <OutlinedQuestionCircleIcon />
            </Tooltip>
            <InputGroup>
                <TextInput
                    id="polarization-label"
                    name="polarization-label"
                    isReadOnly={true}
                    value="Polarization"
                    style={{ textAlign: "left", minWidth: "50%"}}
                /> 
            <FormSelect
                value={this.props.data.polarization}
                onChange={(x) => this.setPolarization(x)}
                id="polarization-drop"
                name="polarization-drop"
                style={{ textAlign: "right" }}
                >
                    <FormSelectOption key="Vertical" value="Vertical" label="Vertical" />
                    <FormSelectOption key="Horizontal" value="Horizontal" label="Horizontal" />
                </FormSelect>
            </InputGroup>
            <InputGroup>
                <TextInput
                    id="ground-type-label"
                    name="ground-type-label"
                    isReadOnly={true}
                    value="Ground Type"
                    style={{ textAlign: "left", minWidth: "50%" }}
                /> 
                <FormSelect
                value={this.props.data.ground}
                onChange={(x : string) => this.setGroundType(x)}
                id="ground-type-drop"
                name="ground-type-drop"
                style={{ textAlign: "right" }}
                >
                    <FormSelectOption key="Good Ground" value="Good Ground" label="Good Ground" />
                    <FormSelectOption key="Average Ground" value="Average Ground" label="Average Ground" />
                    <FormSelectOption key="Poor Ground" value="Poor Ground" label="Poor Ground" />
                    <FormSelectOption key="Fresh Water" value="Fresh Water" label="Fresh Water" />
                    <FormSelectOption key="Sea Water" value="Sea Water" label="Sea Water" />
                    
                </FormSelect>
            </InputGroup>
            <InputGroup>
                <TextInput
                    id="dielectric-label"
                    name="dielectric-label"
                    isReadOnly={true}
                    value="Dielectric Constant"
                    style={{ textAlign: "left", minWidth: "65%" }}
                /> 
                <TextInput
                    id="dielectric-display"
                    name="dielectric-display"
                    isReadOnly={true}
                    value={this.props.data.dielectricConst}
                    style={{ textAlign: "right"}}
                />
            </InputGroup>
            <InputGroup>
                <TextInput
                    id="conductivity-label"
                    name="conductivity-label"
                    isReadOnly={true}
                    value="Conductivity"
                    style={{ textAlign: "left", minWidth: "60%" }}
                /> 
                <TextInput
                    id="conductivity-data"
                    name="conductivity-data"
                    isReadOnly={true}
                    value={this.props.data.conductivity}
                    style={{ textAlign: "right"}}
                /> 
                <InputGroupText>S/m</InputGroupText>
            </InputGroup>
            <InputGroup>
                <TextInput
                    id="min-spacing-label"
                    name="min-spacing-label"
                    isReadOnly={true}
                    value="Path Min Spacing"
                    style={{ textAlign: "left", minWidth: "55%" }}
                /> 
                <TextInput
                    id="min-spacing-data"
                    name="min-spacing-data"
                    onChange={(x : string) => this.setMinSpacing(x)}
                    step="any"
                    type="number"
                    isValid={this.props.data.minSpacing >= 1 && this.props.data.minSpacing <= 30}
                    value={this.props.data.minSpacing}
                    style={{ textAlign: "right"}}
                /> 
                <InputGroupText>m</InputGroupText>
            </InputGroup>
            <InputGroup>
                <TextInput
                    id="max-points-label"
                    name="max-points-label"
                    isReadOnly={true}
                    value="Path Max Points"
                    style={{ textAlign: "left", minWidth: "55%" }}
                /> 
                <TextInput
                    id="max-points-data"
                    name="max-points-data"
                    onChange={(x : string) => this.setMaxPoints(x)}
                    step="any"
                    type="number"
                    isValid={this.props.data.maxPoints >= 100 && this.props.data.maxPoints <= 10000}
                    value={this.props.data.maxPoints}
                    style={{ textAlign: "right"}}
                /> 
            </InputGroup>
        </FormGroup>
        </>
}