import * as React from 'react';
import {
  Card,
  CardHead,
  CardHeader,
  CardBody,
  PageSection,
  FormGroup,
  TextInput,
  Title,
  ActionGroup,
  Button,
  Alert,
  AlertActionCloseButton,
  InputGroupText,
  InputGroup,
  Checkbox,
  FormSelect,
  FormSelectOption,
} from '@patternfly/react-core';
import { FreqRange } from '../Lib/RatApiTypes';
import { mapRegionCodeToName } from '../Lib/Utils';

// FrequencyRangeInput.tsx: Page where frequency bands can be modified

interface FrequencyRangeProps {
  frequencyRange: FreqRange;
  regions: string[];
  onSuccess: (freq: FreqRange) => void;
}

interface FrequencyRangeState {
  frequencyRange: FreqRange;
}

export class FrequencyRangeInput extends React.Component<FrequencyRangeProps, FrequencyRangeState> {
  constructor(props: FrequencyRangeProps) {
    super(props);

    this.state = {
      frequencyRange: { ...props.frequencyRange },
    };
  }

  private updateField = (newData: string, fieldToUpdate: string) => {
    let freq = { ...this.state.frequencyRange };
    if (fieldToUpdate == 'name' || fieldToUpdate == 'region') {
      freq[fieldToUpdate] = newData;
    } else {
      // the only other fields are all numbers
      freq[fieldToUpdate] = Number(newData);
    }

    this.setState({ frequencyRange: freq });
  };

  private submitBand = () => {
    const isValid =
      this.state.frequencyRange.name.length > 0 &&
      this.state.frequencyRange.startFreqMHz > 0 &&
      this.state.frequencyRange.stopFreqMHz > this.state.frequencyRange.startFreqMHz;
    if (isValid) {
      this.props.onSuccess(this.state.frequencyRange);
    } else {
      alert('One or more fields invalid.');
    }
  };

  render() {
    return (
      <Card>
        <CardBody>
          <FormGroup label="Update Frequency Range" fieldId="band-name-label">
            <InputGroup>
              <TextInput
                id={'band-region-label'}
                name={'band-region-label'}
                value={'region'}
                style={{ textAlign: 'left', minWidth: '50%' }}
                isReadOnly
              />
              <FormSelect
                id={'band-region-'}
                name={'band-region-'}
                value={this.state.frequencyRange.region}
                style={{ textAlign: 'right' }}
                isValid={!!this.state.frequencyRange.region && this.state.frequencyRange.region!.length > 0}
                onChange={(data) => this.updateField(data, 'region')}
              >
                {this.props.regions.map((option: string) => (
                  <FormSelectOption key={option} value={option} label={mapRegionCodeToName(option)} />
                ))}
              </FormSelect>
            </InputGroup>

            <InputGroup>
              <TextInput
                id={'band-name-label'}
                name={'band-name-label'}
                value={'Name'}
                style={{ textAlign: 'left', minWidth: '50%' }}
                isReadOnly
              />
              <TextInput
                id={'band-name-'}
                name={'band-name-'}
                value={this.state.frequencyRange.name}
                style={{ textAlign: 'right' }}
                isValid={this.state.frequencyRange.name.length > 0}
                onChange={(data) => this.updateField(data, 'name')}
              />
            </InputGroup>
            <InputGroup>
              <TextInput
                id={'band-lower-label'}
                name={'band-lower-label'}
                value={'Low Frequency'}
                style={{ textAlign: 'left', minWidth: '50%' }}
                isReadOnly
              />
              <TextInput
                type="number"
                id={'band-lower-'}
                name={'band-lower-'}
                value={this.state.frequencyRange.startFreqMHz}
                style={{ textAlign: 'right' }}
                isValid={this.state.frequencyRange.startFreqMHz > 0}
                onChange={(data) => this.updateField(data, 'startFreqMHz')}
              />
              <InputGroupText>MHz</InputGroupText>
            </InputGroup>
            <InputGroup>
              <TextInput
                id={'band-upper-label'}
                name={'band-upper-label'}
                value={'High Frequency'}
                style={{ textAlign: 'left', minWidth: '50%' }}
                isReadOnly
              />
              <TextInput
                type="number"
                id={'band-upper-'}
                name={'band-upper-'}
                value={this.state.frequencyRange.stopFreqMHz}
                style={{ textAlign: 'right' }}
                isValid={this.state.frequencyRange.stopFreqMHz > this.state.frequencyRange.startFreqMHz}
                onChange={(data) => this.updateField(data, 'stopFreqMHz')}
              />
              <InputGroupText>MHz</InputGroupText>
            </InputGroup>
            <Button onClick={this.submitBand}>Submit</Button>
          </FormGroup>
        </CardBody>
      </Card>
    );
  }
}
