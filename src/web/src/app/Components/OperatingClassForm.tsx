import * as React from 'react';
// import ReactTooltip from 'react-tooltip';
import {
  GalleryItem,
  FormGroup,
  InputGroup,
  Radio,
  TextInput,
  InputGroupText,
  Select,
  SelectOption,
  SelectVariant,
  CheckboxSelectGroup,
  Checkbox,
} from '@patternfly/react-core';
import { OperatingClass, OperatingClassIncludeType } from '../Lib/RatAfcTypes';
/** OperatingClassForm.tsx - Form component to display and create the OperatingClass object
 *
 * mgelman 2022-01-04
 */

export interface OperatingClassFormParams {
  operatingClass: {
    num: number;
    include: OperatingClassIncludeType;
    channels?: number[];
  };
  onChange: (val: { num: number; include: OperatingClassIncludeType; channels?: number[] }) => void;
  allowOnlyOneChannel?: boolean;
}

export interface OperatingClassFormState {
  num?: number;
  include?: OperatingClassIncludeType;
  channels?: number[];
  allChannels: number[];
  allowOnlyOneChannel: boolean;
}

export class OperatingClassForm extends React.PureComponent<OperatingClassFormParams, OperatingClassFormState> {
  constructor(props: OperatingClassFormParams) {
    super(props);
    this.state = {
      allChannels: this.generateChannelIndicesForOperatingClassNum(props.operatingClass.num),
      num: this.props.operatingClass.num,
      allowOnlyOneChannel: this.props.allowOnlyOneChannel ?? false,
    };
  }

  readonly humanNames = {
    131: 'Operating Class 131 (20 MHz)',
    132: 'Operating Class 132 (40 MHz)',
    133: 'Operating Class 133 (80 MHz)',
    134: 'Operating Class 134 (160 MHz)',
    136: 'Operating Class 136 (20 MHz)',
    137: 'Operating Class 137 (320 MHz)',
  };

  private setInclude(n: OperatingClassIncludeType) {
    this.props.onChange({
      include: n,
      channels: this.props.operatingClass.channels,
      num: this.props.operatingClass.num,
    });
  }

  private setChannels(n: number[]) {
    this.props.onChange({
      include: this.props.operatingClass.include,
      channels: n,
      num: this.props.operatingClass.num,
    });
  }

  private AddIdToString(s: string) {
    return s + '_' + this.props.operatingClass.num;
  }

  private generateChannelIndicesForOperatingClassNum(oc: number) {
    var start = 1;
    var interval = 1;
    var count = 1;

    switch (oc) {
      case 131:
        interval = 4;
        count = 59;
        break;
      case 132:
        start = 3;
        interval = 8;
        count = 29;
        break;
      case 133:
        start = 7;
        interval = 16;
        count = 14;
        break;
      case 134:
        start = 15;
        interval = 32;
        count = 7;
        break;
      case 136:
        start = 2;
        interval = 4;
        count = 1;
        break;
      case 137:
        start = 31;
        interval = 32;
        count = 6;
        break;

      default:
        return [];
    }

    var n = start;
    var result = [];
    for (let index = 0; index < count; index++) {
      result.push(n);
      n += interval;
    }

    return result;
  }

  private onChannelSelected(isChecked, selection) {
    var selectedChannel = Number(selection);

    if (this.state.allowOnlyOneChannel) {
      this.setChannels([selectedChannel]);
    } else {
      if (isChecked && !this.props.operatingClass.channels) {
        this.setChannels([selectedChannel]);
      } else {
        var prevSelections = this.props.operatingClass.channels!.slice();

        if (isChecked) {
          if (prevSelections.includes(selectedChannel)) {
            return; // already there do nothing
          } else {
            prevSelections.push(selectedChannel);
            this.setChannels(prevSelections.sort((a, b) => a - b));
          }
        } else {
          //remove if present
          if (prevSelections.includes(selectedChannel)) {
            this.setChannels(prevSelections.filter((x) => x != selection));
          }
        }
      }
    }
  }

  private isChannelSelected(c: number) {
    if (this.props.operatingClass.channels) {
      return this.props.operatingClass.channels.includes(c);
    } else return false;
  }

  render() {
    return (
      <>
        <GalleryItem key={this.props.operatingClass.num}>
          <FormGroup
            label={this.humanNames[this.props.operatingClass.num]}
            fieldId={'horizontal-form-height-opClass' + this.props.operatingClass.num}
          >
            {!this.state.allowOnlyOneChannel && (
              <FormGroup fieldId={'horizontal-form-height-opClass-Rbs' + this.props.operatingClass.num}>
                <Radio
                  id={this.AddIdToString('rbIncludeNone')}
                  name={this.AddIdToString('includeGrp')}
                  label="None"
                  isChecked={this.props.operatingClass.include == OperatingClassIncludeType.None}
                  onChange={(isChecked) => isChecked && this.setInclude(OperatingClassIncludeType.None)}
                />
                <Radio
                  id={this.AddIdToString('rbIncludeSome')}
                  name={this.AddIdToString('includeGrp')}
                  label="Some"
                  isChecked={this.props.operatingClass.include == OperatingClassIncludeType.Some}
                  onChange={(isChecked) => isChecked && this.setInclude(OperatingClassIncludeType.Some)}
                />
                <Radio
                  id={this.AddIdToString('rbIncludeAll')}
                  name={this.AddIdToString('includeGrp')}
                  label="All"
                  isChecked={this.props.operatingClass.include == OperatingClassIncludeType.All}
                  onChange={(isChecked) => isChecked && this.setInclude(OperatingClassIncludeType.All)}
                />
              </FormGroup>
            )}
            <FormGroup
              fieldId={'horizontal-form-height-opClass-channelList' + this.props.operatingClass.num}
              id={'cbList' + this.props.operatingClass.num}
            >
              {this.props.operatingClass.include == OperatingClassIncludeType.Some &&
                this.state.allChannels.map((v, _) => {
                  return !this.state.allowOnlyOneChannel ? (
                    <Checkbox
                      id={this.AddIdToString('cbChannel') + String(v)}
                      key={this.AddIdToString('cbChannel') + String(v)}
                      value={String(v)}
                      label={String(v)}
                      isChecked={this.isChannelSelected(v)}
                      onChange={(isChecked, e) => this.onChannelSelected(isChecked, v)}
                    />
                  ) : (
                    <Radio
                      id={this.AddIdToString('cbChannel') + String(v)}
                      key={this.AddIdToString('cbChannel') + String(v)}
                      name={'single-channel-select'}
                      value={String(v)}
                      label={String(v)}
                      isChecked={this.isChannelSelected(v)}
                      onChange={(isChecked, e) => this.onChannelSelected(isChecked, v)}
                    />
                  );
                })}
            </FormGroup>
          </FormGroup>
        </GalleryItem>
      </>
    );
  }
}
