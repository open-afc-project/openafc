import * as React from "react";
import { FormGroup, FormSelect, FormSelectOption, TextInput } from "@patternfly/react-core";
import { AntennaPatternState } from "../Lib/RatApiTypes";



/**
 * AntennaPatternForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */


/**
 * Enumeration of antenna pattern types
 */
const antennaPatterns = [
    'WINNF-AIP-07',
    "F.699",
    "F.1245"
];

/**
 * Sub form component for Antenna patterns
 */
export default class AntennaPatternForm extends React.PureComponent<{ data: AntennaPatternState, antennaPatternFiles: string[], onChange: (x: AntennaPatternState) => void }> {

    private setKind = (s: string) => {
        switch (s) {
            case "F.1245":
                this.props.onChange({ defaultAntennaPattern: 'F.1245', userUpload: this.props.data.userUpload });
                break;
            case "F.699":
                this.props.onChange({ defaultAntennaPattern: "F.699", userUpload: this.props.data.userUpload });
                break;
            case 'WINNF-AIP-07':
                this.props.onChange({ defaultAntennaPattern: 'WINNF-AIP-07', userUpload: this.props.data.userUpload });
                break;

            default:
            // Do nothing
        }
    }

    setUserUpload = (s: string) => this.props.onChange({
        defaultAntennaPattern: this.props.data.defaultAntennaPattern,
        userUpload: s != 'None' ? { kind: "User Upload", value: s } : { kind: s, value: '' }
    });


    render = () =>
        <FormGroup label="Default FS Receiver Antenna Pattern" fieldId="horizontal-form-antenna">
            <FormSelect
                value={this.props.data.defaultAntennaPattern}
                onChange={x => this.setKind(x)}
                id="horzontal-form-antenna"
                name="horizontal-form-antenna"
                isValid={this.props.data.defaultAntennaPattern !== undefined}
                style={{ textAlign: "right" }}
            >
                {antennaPatterns.map((option) => (
                    <FormSelectOption key={option} value={option} label={option} />
                ))}
            </FormSelect>
            <FormGroup
                label="User Upload Antenna Pattern File Name"
                fieldId="form-user-upload">
                <FormSelect
                    id="form-user-upload"
                    name="form-user-upload"
                    value={this.props.data.userUpload?.value}
                    isValid={!!this.props.data.userUpload}
                    onChange={this.setUserUpload}
                    style={{ textAlign: "right" }}>
                    <FormSelectOption isDisabled={false} key='None' value='None' label="Use Default Pattern" />
                    {this.props.antennaPatternFiles.map((option) => (
                        <FormSelectOption key={option} value={option} label={option} />
                    ))}
                </FormSelect>
            </FormGroup>
        </FormGroup>
}