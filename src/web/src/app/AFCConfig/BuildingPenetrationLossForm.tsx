import React from 'react';
import {
  FormGroup,
  InputGroup,
  FormSelect,
  FormSelectOption,
  TextInput,
  Form,
  InputGroupText,
} from '@patternfly/react-core';
import { PenetrationLossModel, P2109 } from '../Lib/RatApiTypes';

/**
 * BuildingPenetrationLossForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Enumeration of penetration loss types
 */
const penetrationLossModels = ['ITU-R Rec. P.2109', 'Fixed Value'];

/**
 * Sub form for building penetration loss model
 */
export default class BuildlingPenetrationLossForm extends React.PureComponent<{
  data: PenetrationLossModel;
  onChange: (x: PenetrationLossModel) => void;
}> {
  change: (x: PenetrationLossModel) => void;

  constructor(props) {
    super(props);

    this.change = props.onChange;
  }

  private setKind(s: string) {
    switch (s) {
      case 'ITU-R Rec. P.2109':
        this.change({ kind: s, buildingType: 'Traditional', confidence: 50 });
        break;
      case 'Fixed Value':
        this.change({ kind: s, value: 0 });
        break;
      default:
      // Do nothing
    }
  }

  setBuildingType = (_event: any, s: string) => {
    if (s === 'Traditional' || s === 'Efficient') {
      this.change({ kind: 'ITU-R Rec. P.2109', buildingType: s, confidence: (this.props.data as P2109).confidence });
    }
  };

  setPercentile = (_event: any, s: string) => {
    this.change({
      kind: 'ITU-R Rec. P.2109',
      confidence: Number(s),
      buildingType: (this.props.data as P2109).buildingType,
    });
  };

  setFixedValue = (_event: any, s: string) => {
    this.change({ kind: 'Fixed Value', value: Number(s) });
  };

  getExpansion = () => {
    switch (this.props.data.kind) {
      case 'ITU-R Rec. P.2109':
        return (
          <>
            <FormGroup label="P.2109 Building Type" fieldId="penetration-loss-building-type">
              <FormSelect
                value={this.props.data.buildingType}
                onChange={this.setBuildingType}
                id="penetration-loss-building-type"
                name="penetration-loss-building-type"
                style={{ textAlign: 'right' }}
                validated={this.props.data.buildingType !== undefined ? 'default' : 'error'}
              >
                <FormSelectOption key="Traditional" value="Traditional" label="Traditional" />
                <FormSelectOption key="Efficient" value="Efficient" label="Thermally-Efficient" />
              </FormSelect>
            </FormGroup>
            <FormGroup label="P.2109 Confidence" fieldId="penetration-loss-building-confidence">
              <InputGroup>
                <TextInput
                  value={this.props.data.confidence}
                  type="number"
                  onChange={this.setPercentile}
                  id="penetration-loss-building-confidence"
                  name="penetration-loss-building-confidence"
                  style={{ textAlign: 'right' }}
                  validated={this.props.data.confidence >= 0 && this.props.data.confidence <= 100 ? 'default' : 'error'}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
          </>
        );

      case 'Fixed Value':
        return (
          <FormGroup label="Value" fieldId="penetration-loss-fixed-value">
            <InputGroup>
              <TextInput
                type="number"
                id="penetration-loss-fixed-value"
                name="penetration-loss-fixed-value"
                validated='default'
                value={this.props.data.value}
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
  };

  render() {
    return (
      <FormGroup label="Building Penetration Loss" fieldId="horizontal-form-penetration-loss">
        <InputGroup>
          <FormSelect
            value={this.props.data.kind}
            onChange={(_event, x) => this.setKind(x)}
            id="horzontal-form-penetration-loss"
            name="horizontal-form-penetration-loss"
            validated={this.props.data.kind !== undefined ? 'default' : 'error'}
            style={{ textAlign: 'right' }}
          >
            {penetrationLossModels.map((option) => (
              <FormSelectOption key={option} value={option} label={option} />
            ))}
          </FormSelect>
        </InputGroup>
        {this.getExpansion()}
      </FormGroup>
    );
  }
}
