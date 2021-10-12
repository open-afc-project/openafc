import * as React from "react";
import { FormGroup, InputGroup, FormSelect, FormSelectOption, TextInput, InputGroupText } from "@patternfly/react-core";
import { BodyLossModel } from "../Lib/RatApiTypes";

/**
 * BodyLossForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Enumeration of penetration loss models
 */
const penetrationLossModels = [
    "Fixed Value"
];

/**
 * Sub form component for body loss model
 */
export default class BodyLossForm extends React.PureComponent<{ data: BodyLossModel, onChange: (x: BodyLossModel) => void }> {

    private setKind = (s: string) => {
        switch (s) {
            case "EU":
                this.props.onChange({ kind: s });
                break;
            case "Fixed Value":
                this.props.onChange({ kind: s, valueIndoor: 0, valueOutdoor: 0 });
                break;
            default:
            // Do nothing
        }
    }

    private setFixedValueIndoor = (s: string) =>
        // @ts-ignore
        this.props.onChange({ kind: "Fixed Value", valueIndoor: Number(s), valueOutdoor: this.props.data.valueOutdoor });

    private setFixedValueOutdoor = (s: string) =>
            // @ts-ignore
        this.props.onChange({ kind: "Fixed Value", valueIndoor: this.props.data.valueIndoor, valueOutdoor: Number(s) });

    private getExpansion = () => {
        switch (this.props.data.kind) {
            case "EU":
                return (<></>);

            case "Fixed Value":
                return (<>
                <FormGroup
                    label="RLAN Indoor Body Loss"
                    fieldId="body-loss-fixed-value-in">
                    <InputGroup>
                        <TextInput
                            type="number"
                            id="body-loss-fixed-value-in"
                            name="body-loss-fixed-value-in"
                            isValid={this.props.data.valueIndoor <= 0 || this.props.data.valueIndoor > 0}
                            value={this.props.data.valueIndoor}
                            onChange={this.setFixedValueIndoor}
                            style={{ textAlign: "right" }} /><InputGroupText>dB</InputGroupText>
                    </InputGroup>
                </FormGroup>
                <FormGroup 
                    label="RLAN Outdoor Body Loss"
                    fieldId="body-loss-fixed-value-out">
                    <InputGroup>
                        <TextInput
                            type="number"
                            id="body-loss-fixed-value-out"
                            name="body-loss-fixed-value-out"
                            isValid={this.props.data.valueOutdoor <= 0 || this.props.data.valueOutdoor > 0}
                            value={this.props.data.valueOutdoor}
                            onChange={this.setFixedValueOutdoor}
                            style={{ textAlign: "right" }} /><InputGroupText>dB</InputGroupText>
                    </InputGroup>
                </FormGroup>
                </>)

            default:
                return <></>;
        }
    }

    render() {
        return (
            <FormGroup fieldId="horizontal-form-body-loss">
                {/* <InputGroup>
                    <FormSelect
                        value={this.props.data.kind}
                        onChange={x => this.setKind(x)}
                        id="horzontal-form-body-loss"
                        name="horizontal-form-body-loss"
                        isValid={this.props.data.kind !== undefined}
                        style={{ textAlign: "right" }}
                    >
                        {penetrationLossModels.map((option) => (
                            <FormSelectOption key={option} value={option} label={option} />
                        ))}
                    </FormSelect>
                </InputGroup> */}
                {this.getExpansion()}
            </FormGroup>
        )
    }
}