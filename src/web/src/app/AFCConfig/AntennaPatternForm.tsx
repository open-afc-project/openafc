import * as React from "react";
import { FormGroup, FormSelect, FormSelectOption, TextInput } from "@patternfly/react-core";
import { AntennaPattern } from "../Lib/RatApiTypes";

/**
 * AntennaPatternForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Enumeration of antenna pattern types
 */
const antennaPatterns = [
    "F.1245",
    "User Upload"
];

/**
 * Sub form component for Antenna patterns
 */
export default class AntennaPatternForm extends React.PureComponent<{ data: AntennaPattern, antennaPatternFiles: string[], onChange: (x: AntennaPattern) => void }> {

    private setKind = (s: string) => {
        switch (s) {
            case "F.1245":
                this.props.onChange({ kind: s });
                break;
            case "User Upload":
                this.props.onChange({ kind: s, value: "" });
                break;
            default:
            // Do nothing
        }
    }

    setUserUpload = (s: string) => this.props.onChange({ kind: "User Upload", value: s });

    getExpansion = () => {
        switch (this.props.data.kind) {
            case "F.1245":
                return (<></>);

            case "User Upload":
                return (<FormGroup
                    label="Antenna Pattern File Name"
                    fieldId="form-user-upload">
                    <FormSelect
                        id="form-user-upload"
                        name="form-user-upload"
                        value={this.props.data.value}
                        isValid={!!this.props.data.value}
                        onChange={this.setUserUpload}
                        style={{ textAlign: "right" }}>
                        <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select an Antenna Pattern" />
                        {this.props.antennaPatternFiles.map((option) => (
                            <FormSelectOption key={option} value={option} label={option} />
                        ))}
                    </FormSelect>
                </FormGroup>)

            default:
                return <></>;
        }
    };

    render = () =>
        <FormGroup label="FS Receiver Antenna Pattern" fieldId="horizontal-form-antenna">
            <FormSelect
                value={this.props.data.kind}
                onChange={x => this.setKind(x)}
                id="horzontal-form-antenna"
                name="horizontal-form-antenna"
                isValid={this.props.data.kind !== undefined}
                style={{ textAlign: "right" }}
            >
                {antennaPatterns.map((option) => (
                    <FormSelectOption key={option} value={option} label={option} />
                ))}
            </FormSelect>
            {this.getExpansion()}
        </FormGroup>
}