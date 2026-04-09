import React from 'react';
import { Table, Thead, Tbody, Tr, Th, Td } from '@patternfly/react-table';
import { TextInput, Button, InputGroup, FormGroup } from '@patternfly/react-core';
import { ifNum } from '../Lib/Utils';
import { Tooltip } from '@patternfly/react-core';
import { OutlinedQuestionCircleIcon } from '@patternfly/react-icons';

const createHeaderWithTooltip = (text: string, tooltip: string) => {
  return (
    <>
      {text} {createTooltip(tooltip)}
    </>
  );
};

const createTooltip = (tooltip: string) => {
  return (
    <Tooltip
      enableFlip={true}
      className="prop-model-tooltip"
      maxWidth="40.0rem"
      content={
        <>
          <p>{tooltip}</p>
        </>
      }
    >
      <OutlinedQuestionCircleIcon />
    </Tooltip>
  );
};

/**
 * PairList.tsx: component for interacting with a list with 2 number columns
 * author: Sam Smucny
 */

/**
 * params for pair list component
 */
interface PairListParams {
  tableName: string;
  firstLabel: string;
  secondLabel: string;
  firstHelperText: string;
  secondHelperText: string;
  values: [number, number][];
  fstValid: (x: number) => boolean;
  sndValid: (x: number) => boolean;
  onChange: (values: [number, number][]) => void;
}

interface PairListState {
  first?: number;
  second?: number;
}

/**
 * pair list component for interacting with a list with two number columns
 */
export class PairList extends React.Component<PairListParams, PairListState> {
  private rowClickHandler: (event: any, row: { index: number; cells: string[] }) => void;

  constructor(props: PairListParams) {
    super(props);

    this.state = {};

    /**
     * rows are removed when clicked
     */
    this.rowClickHandler = (_, row) => {
      console.log(row);
      const valueCopy = this.props.values;
      valueCopy.splice(row.index, 1);
      this.props.onChange(valueCopy);
    };
  }

  /**
   * add a row
   */
  private addPoint() {
    const valueCopy = this.props.values;
    if (
      this.state.first !== undefined &&
      this.props.fstValid(this.state.first) &&
      this.state.second !== undefined &&
      this.props.sndValid(this.state.second)
    ) {
      valueCopy.push([this.state.first, this.state.second]);
      this.props.onChange(valueCopy);
    }
  }

  render() {
    const { firstLabel, firstHelperText, secondLabel, secondHelperText } = this.props;

    // Render either the plain column names, or with a tooltips if specified
    const columns = [
      firstHelperText
        ? { title: createHeaderWithTooltip(firstLabel, firstHelperText), text: firstLabel }
        : { title: firstLabel, text: firstLabel },
      secondHelperText
        ? { title: createHeaderWithTooltip(secondLabel, secondHelperText), text: secondLabel }
        : { title: secondLabel, text: secondLabel },
    ];

    const rows = this.props.values.map((row, i) => ({ index: i, cells: row }));
    return (
      <>
        <Table aria-label={this.props.tableName}>
          <caption>{this.props.tableName}</caption>
          <Thead>
            <Tr>
              {columns.map((col, ci) => (
                <Th key={ci} aria-label={col.text} screenReaderText={col.text}>
                  {col.title}
                </Th>
              ))}
            </Tr>
          </Thead>
          <Tbody>
            {rows.map((row) => (
              <Tr
                key={row.index}
                onRowClick={(event) => this.rowClickHandler(event, { index: row.index, cells: row.cells.map(String) })}
              >
                {row.cells.map((cell, ci) => (
                  <Td key={ci}>{cell}</Td>
                ))}
              </Tr>
            ))}
          </Tbody>
        </Table>
        <InputGroup>
          <TextInput
            value={this.state.first ?? ''}
            onChange={(_event, x) => ifNum(x, (n) => this.setState({ first: n }))}
            type="number"
            step="any"
            id="horizontal-form-lat-point"
            name="horizontal-form-lat-point"
            validated={this.state.first && this.props.fstValid(this.state.first) ? 'default' : 'error'}
            style={{ textAlign: 'right' }}
          />
          <TextInput
            value={this.state.second ?? ''}
            onChange={(_event, x) => ifNum(x, (n) => this.setState({ second: n }))}
            type="number"
            step="any"
            id="horizontal-form-lon-point"
            name="horizontal-form-lon-point"
            validated={this.state.second && this.props.sndValid(this.state.second) ? 'default' : 'error'}
            style={{ textAlign: 'right' }}
          />
          <Button key="add-point" variant="tertiary" onClick={() => this.addPoint()}>
            +
          </Button>
        </InputGroup>
      </>
    );
  }
}
