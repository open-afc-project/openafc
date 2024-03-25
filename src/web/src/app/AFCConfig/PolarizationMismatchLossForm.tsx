import * as React from 'react';
import { FormGroup, TextInput, InputGroup, FormSelect, FormSelectOption, InputGroupText } from '@patternfly/react-core';
import { PolarizationLossModel } from '../Lib/RatApiTypes';

/**
 * PolarizationMismatchLossForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Enumeration of polarization loss models
 */
const polarizationLossModels = ['Fixed Value'];

/**
 * Sub form for Polarization mismatch model
 */
export default class PolarizationMismatchLossForm extends React.PureComponent<{
  data: PolarizationLossModel;
  onChange: (x: PolarizationLossModel) => void;
}> {
  private change: (x: PolarizationLossModel) => void;

  constructor(props: Readonly<{ data: PolarizationLossModel; onChange: (x: PolarizationLossModel) => void }>) {
    super(props);

    this.change = props.onChange;
  }

  private setKind(s: string) {
    switch (s) {
      case 'EU':
        this.change({ kind: s });
        break;
      case 'Fixed Value':
        this.change({ kind: s, value: 0 });
        break;
      default:
      // Do nothing
    }
  }

  setFixedValue = (s: string) => {
    this.change({ kind: 'Fixed Value', value: Number(s) });
  };

  private getExpansion(model: PolarizationLossModel) {
    switch (model.kind) {
      case 'EU':
        return <></>;

      case 'Fixed Value':
        return (
          <FormGroup
            // label="Value"
            fieldId="polarization-loss-fixed-value"
          >
            <InputGroup>
              <TextInput
                type="number"
                id="polarization-loss-fixed-value"
                name="polarization-loss-fixed-value"
                isValid={true}
                value={model.value}
                onChange={this.setFixedValue}
                style={{ textAlign: 'right' }}
              />
              <InputGroupText>dB</InputGroupText>
            </InputGroup>
          </FormGroup>
        );

      default:
        return <></>;
    }
  }

  render() {
    const model = this.props.data;

    return (
      <FormGroup label="Polarization Mismatch Loss" fieldId="horizontal-form-polarization-loss">
        {/* <InputGroup>
                    <FormSelect
                        value={model.kind}
                        onChange={x => this.setKind(x)}
                        id="horzontal-form-polarization-loss"
                        name="horizontal-form-polarization-loss"
                        isValid={model.kind !== undefined}
                        style={{ textAlign: "right" }}
                    >
                        {polarizationLossModels.map((option) => (
                            <FormSelectOption key={option} value={option} label={option} />
                        ))}
                    </FormSelect>
                </InputGroup> */}
        {this.getExpansion(model)}
      </FormGroup>
    );
  }
}
