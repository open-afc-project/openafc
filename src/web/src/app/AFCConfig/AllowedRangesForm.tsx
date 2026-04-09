import React from 'react';
import {
  Alert,
  FormGroup,
  InputGroup,
  TextInput,
  InputGroupText,
  AlertActionCloseButton,
} from '@patternfly/react-core';
import { Table, Thead, Tr, Th, Tbody, Td } from '@patternfly/react-table';
import { FreqRange } from '../Lib/RatApiTypes';

/**
 * AllowedRangesForm.tsx: Displays admin defined allowed frequency ranges as readonly values
 */

const cols = ['Name', 'Low Frequency', 'High Frequency'];

export const getDefaultRangesByRegion = (region: string) => {
  if (region.endsWith('CA')) {
    return [
      {
        region: 'CA',
        name: 'Canada',
        startFreqMHz: 5925,
        stopFreqMHz: 6875,
      },
    ];
  } else if (region.endsWith('BR')) {
    return [
      {
        region: 'BR',
        name: 'Brazil',
        startFreqMHz: 5925,
        stopFreqMHz: 6875,
      },
    ];
  } else if (region.endsWith('GB')) {
    return [
      {
        region: 'GB',
        name: 'United Kingdom',
        startFreqMHz: 5925,
        stopFreqMHz: 7125,
      },
    ];
  } else {
    return [
      {
        region: 'US',
        name: 'UNII-5',
        startFreqMHz: 5925,
        stopFreqMHz: 6425,
      },
      {
        region: 'US',
        name: 'UNII-7',
        startFreqMHz: 6525,
        stopFreqMHz: 6875,
      },
    ];
  }
};

/**
 * Sub form component for allowed freq ranges
 */
export class AllowedRangesDisplay extends React.PureComponent<
  { data: FreqRange[]; region: string },
  { showWarn: boolean }
> {
  // @ts-ignore
  constructor(props) {
    super(props);
    this.state = {
      showWarn: !this.props.data || this.props.data.length === 0,
    };
  }

  private renderTable = (datasource: FreqRange[]) => {
    return (
      <Table aria-label="freq-table" variant="compact">
        <Thead>
          <Tr>
            {cols.map((col, idx) => (
              <Th key={idx}>{col}</Th>
            ))}
          </Tr>
        </Thead>
        <Tbody>
          {datasource.map((f, index) => (
            <Tr key={index}>
              <Td dataLabel={cols[0]}>{f.name}</Td>
              <Td dataLabel={cols[1]}>{f.startFreqMHz}</Td>
              <Td dataLabel={cols[2]}>{f.stopFreqMHz}</Td>
            </Tr>
          ))}
        </Tbody>
      </Table>
    );
  };

  render() {
    const dataSource =
      !this.props.data || this.props.data.length === 0 ? getDefaultRangesByRegion(this.props.region) : this.props.data;
    return (
      <>
        {this.state.showWarn ? (
          <Alert
            title={'Error Fetching Allowed Frequency Ranges'}
            variant="warning"
            actionClose={<AlertActionCloseButton onClose={() => this.setState({ showWarn: false })} />}
          >
            <pre>Falling back to default UNII-5 and UNII-7 ranges</pre>
          </Alert>
        ) : (
          false
        )}
        <fieldset style={{ border: 'none', padding: 0, margin: 0 }}>
          <legend>Allowed Frequency Ranges</legend>
          {this.renderTable(dataSource)}
        </fieldset>
      </>
    );
  }
}
