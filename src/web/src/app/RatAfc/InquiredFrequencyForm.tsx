import React from 'react';
// import ReactTooltip from 'react-tooltip';
import { Table, Thead, Tr, Th, Tbody, Td, ActionsColumn } from '@patternfly/react-table';
import { GalleryItem, FormGroup, InputGroup, InputGroupText, TextInput, Button } from '@patternfly/react-core';
import { FrequencyRange } from '../Lib/RatAfcTypes';
/** InquiredFrequencyFormParams.tsx - Form component to display and create the list of frequency
 *
 * mgelman 2022-02-09
 */

export interface InquiredFrequencyFormParams {
  inquiredFrequencyRange: FrequencyRange[];
  onChange: (val: { inquiredFrequencyRange: FrequencyRange[] }) => void;
}

export interface InquiredFrequencyFormState {
  newLowFreq?: number;
  newHighFreq?: number;
  columns?: string[];
}

export class InquiredFrequencyForm extends React.PureComponent<
  InquiredFrequencyFormParams,
  InquiredFrequencyFormState
> {
  constructor(props: InquiredFrequencyFormParams) {
    super(props);
    this.state = {
      newLowFreq: undefined,
      newHighFreq: undefined,
      columns: ['#', 'Low (MHz)', 'High (MHz)'],
    };
  }

  // private setInclude(n: OperatingClassIncludeType) {
  //     this.props.onChange({
  //         include: n,
  //         channels: this.props.operatingClass.channels,
  //         num: this.props.operatingClass.num
  //     }
  //     );
  // }

  private removeFreqBand(index: number) {
    var newRanges = this.props.inquiredFrequencyRange.slice();
    newRanges.splice(index, 1);
    this.props.onChange({ inquiredFrequencyRange: newRanges });
  }

  private renderFrequencyTable = () => {
    return (
      <Table aria-label="freq-table" variant="compact">
        <Thead>
          <Tr>
            {this.state.columns?.map((col, idx) => (
              <Th key={idx}>{col}</Th>
            ))}
            <Th screenReaderText="Actions" />
          </Tr>
        </Thead>
        <Tbody>
          {this.props.inquiredFrequencyRange.map((band, index) => (
            <Tr key={index}>
              <Td dataLabel={this.state.columns?.[0]}>{index + 1}</Td>
              <Td dataLabel={this.state.columns?.[1]}>{band.lowFrequency}</Td>
              <Td dataLabel={this.state.columns?.[2]}>{band.highFrequency}</Td>
              <Td isActionCell>
                <ActionsColumn
                  items={[
                    {
                      title: 'Delete',
                      onClick: () => this.removeFreqBand(index),
                    },
                  ]}
                />
              </Td>
            </Tr>
          ))}
        </Tbody>
      </Table>
    );
  };

  private submitBand() {
    const low = this.state.newLowFreq;
    const high = this.state.newHighFreq;
    if (low && high && low < high) {
      const newRanges = this.props.inquiredFrequencyRange
        .slice()
        .concat({ highFrequency: high, lowFrequency: low })
        .sort((a, b) => a.lowFrequency - b.lowFrequency);
      this.props.onChange({ inquiredFrequencyRange: newRanges });
      this.setState({ newLowFreq: undefined, newHighFreq: undefined });
    }
  }

  render() {
    return (
      <GalleryItem>
        <fieldset>
          <legend>Inquired Frequencies</legend>
          {this.renderFrequencyTable()}
          <FormGroup label="Lower (MHz)" fieldId="band-lower-">
            <InputGroup>
              <TextInput
                className="lowerInline"
                placeholder="Lower"
                type="number"
                id={'band-lower-'}
                name={'band-lower-'}
                value={!this.state.newLowFreq ? '' : this.state.newLowFreq}
                style={{ textAlign: 'right' }}
                validated={!this.state.newLowFreq || this.state.newLowFreq > 0 ? 'default' : 'error'}
                onChange={(_event, data) => this.setState({ newLowFreq: Number(data) })}
              />
              <InputGroupText>–</InputGroupText>
              <TextInput
                aria-label="Upper frequency (MHz)"
                placeholder="Upper"
                type="number"
                id={'band-upper-'}
                name={'band-upper-'}
                value={!this.state.newHighFreq ? '' : this.state.newHighFreq}
                style={{ textAlign: 'right' }}
                // @ts-ignore
                validated={
                  !this.state.newHighFreq || this.state.newHighFreq > (this.state.newLowFreq ?? 0) ? 'default' : 'error'
                }
                onChange={(_event, data) => this.setState({ newHighFreq: Number(data) })}
                className="upperInline"
              />

              <Button className="btnInline" onClick={() => this.submitBand()}>
                +
              </Button>
            </InputGroup>
          </FormGroup>
        </fieldset>
      </GalleryItem>
    );
  }
}
