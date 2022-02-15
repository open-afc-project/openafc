import * as React from "react";
// import ReactTooltip from 'react-tooltip';
import { Table, TableHeader, TableBody, TableVariant, TableProps } from "@patternfly/react-table";
import { GalleryItem, FormGroup, InputGroup, Radio, TextInput, InputGroupText, Button } from "@patternfly/react-core";
import { FrequencyRange } from "../Lib/RatAfcTypes";
/** InquiredFrequencyFormParams.tsx - Form component to display and create the list of frequency
 * 
 * mgelman 2022-02-09
 */

export interface InquiredFrequencyFormParams {
    inquiredFrequencyRange: FrequencyRange[],
    onChange: (val: {
        inquiredFrequencyRange: FrequencyRange[]
    }) => void
}

export interface InquiredFrequencyFormState {
    newLowFreq?: number,
    newHighFreq?: number,
    columns?: string[]
}


export class InquiredFrequencyForm extends React.PureComponent<InquiredFrequencyFormParams, InquiredFrequencyFormState> {
    constructor(props: InquiredFrequencyFormParams) {
        super(props);
        this.state = {
            newLowFreq: undefined,
            newHighFreq: undefined,
            columns: ["#", "Low (MHz)", "High (MHz)"]
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

    actionResolver(data: any, extraData: any) {

        return [
            {
                title: "Delete",
                onClick: (event: any, rowId: any, rowData: any, extra: any) => this.removeFreqBand(rowId)
            }
        ];
    }

    //   private updateTableEntry = (freq :FrequencyRange) => {
    //     const {frequencyEditIndex, frequencyBands} = this.state;
    //     frequencyBands[frequencyEditIndex] = freq;
    //     this.setState({frequencyBands: frequencyBands})
    //   }

    private freqBandToRow(band: FrequencyRange, index: number) {

        return [String(index + 1), band.lowFrequency, band.highFrequency]

    }

    private removeFreqBand(index: number) {
        var newRanges = this.props.inquiredFrequencyRange.slice();
        newRanges.splice(index, 1);
        this.props.onChange({ inquiredFrequencyRange: newRanges })
    }


    private renderFrequencyTable = () => {
        return (
            <Table
                aria-label="freq-table"
                actionResolver={(a, b) => this.actionResolver(a, b)} variant={TableVariant.compact}
                cells={this.state.columns}
                rows={this.props.inquiredFrequencyRange.map((band, index) => this.freqBandToRow(band, index))} >
                <TableHeader />
                <TableBody />

            </Table>)
    }

    private submitBand() {
        const low = this.state.newLowFreq;
        const high = this.state.newHighFreq;
        if (low && high && low < high) {
            const newRanges = this.props.inquiredFrequencyRange.slice().concat({ highFrequency: high, lowFrequency: low }).sort((a, b) => a.lowFrequency - b.lowFrequency)
            this.props.onChange({ inquiredFrequencyRange: newRanges })
            this.setState({ newLowFreq: undefined, newHighFreq: undefined });
        }
    }

    render() {
        return (<GalleryItem >
            <FormGroup label="Inquired Frequencies" fieldId="horizontal-form-includedFreq" >
                {this.renderFrequencyTable()}
                <FormGroup label="Add Frequency Range (MHz)" fieldId="horizontal-form-addFreq" >
                        <InputGroup >
                        
                            <TextInput className="lowerInline"
                                placeholder="Lower"
                                type="number"
                                id={"band-lower-"}
                                name={"band-lower-"}
                                value={!this.state.newLowFreq ? "" : this.state.newLowFreq}
                                style={{ textAlign: "right"}}
                                isValid={!this.state.newLowFreq || this.state.newLowFreq > 0}
                                onChange={(data) => this.setState({ newLowFreq: Number(data) })}
                            />
                      
                       
                            <TextInput
                                placeholder="Upper"
                                type="number"
                                id={"band-upper-"}
                                name={"band-upper-"}
                                value={!this.state.newHighFreq ? "" : this.state.newHighFreq}
                                style={{ textAlign: "right" }}
                                isValid={!this.state.newHighFreq || this.state.newHighFreq > this.state.newLowFreq}
                                onChange={(data) => this.setState({ newHighFreq: Number(data) })}
                                className="upperInline"
                            />
                          
                       
                        <Button className="btnInline" onClick={() => this.submitBand()}>+</Button>
                        </InputGroup>
                </FormGroup>
            </FormGroup>
        </GalleryItem >
        );
    }
}

