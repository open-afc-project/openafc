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
  Gallery,
  Button,
  Label,
  ClipboardCopy,
  ClipboardCopyVariant,
} from '@patternfly/react-core';
import { VendorExtension } from '../Lib/RatAfcTypes';
/** VendorExtensionForm.tsx - Form component to display and create the VendorExtension object
 *
 * mgelman 2022-01-04
 */

export interface VendorExtensionFormParams {
  VendorExtensions: VendorExtension[];
  onChange: (val: VendorExtension[]) => void;
}

export interface VendorExtensionFormState {
  VendorExtensions: VendorExtension[];
  parseValidationMessages: string[];
}

export class VendorExtensionForm extends React.PureComponent<VendorExtensionFormParams, VendorExtensionFormState> {
  constructor(props: VendorExtensionFormParams) {
    super(props);
    let len = this.props.VendorExtensions.length;
    this.state = {
      VendorExtensions: this.props.VendorExtensions,
      parseValidationMessages: Array(len).fill(''),
    };
  }

  setExtensionId(x: string, idx: number): void {
    var newVeList = this.state.VendorExtensions.slice();
    newVeList[idx].extensionId = x;
    this.setState({ VendorExtensions: newVeList }, () => {
      this.props.onChange(this.state.VendorExtensions);
    });
  }

  setParameters(x: string, idx: number): void {
    try {
      let cleanedString = x.replace(/\s+/g, ' ').trim();
      let newParamObj = JSON.parse(cleanedString);
      var newVeList = this.state.VendorExtensions.slice();
      newVeList[idx].parameters = newParamObj;
      let msgArray = this.state.parseValidationMessages.slice();
      msgArray[idx] = '';
      this.setState({ VendorExtensions: newVeList, parseValidationMessages: msgArray }, () => {
        this.props.onChange(this.state.VendorExtensions);
      });
    } catch (err) {
      if (err instanceof SyntaxError) {
        let msgArray = this.state.parseValidationMessages.slice();
        msgArray[idx] = 'Parameter format not correct: ' + err.message;
        this.setState({ parseValidationMessages: msgArray });
      }
    }
  }

  deleteVendorExtension(idx: number): void {
    var newVeList = this.state.VendorExtensions.slice();
    let msgArray = this.state.parseValidationMessages.slice();
    newVeList.splice(idx, 1);
    msgArray.splice(idx, 1);
    this.setState({ VendorExtensions: newVeList, parseValidationMessages: msgArray }, () => {
      this.props.onChange(this.state.VendorExtensions);
    });
  }

  addVendorExtension(): void {
    let newVe = { extensionId: '', parameters: {} };
    var newVeList = this.state.VendorExtensions.slice();
    let msgArray = this.state.parseValidationMessages.slice();
    newVeList.push(newVe);
    msgArray.push('');
    this.setState({ VendorExtensions: newVeList, parseValidationMessages: msgArray }, () => {
      this.props.onChange(this.state.VendorExtensions);
    });
  }

  render(): React.ReactNode {
    return (
      <Gallery width={'1200px'}>
        <GalleryItem width={'50%'}>
          {this.state.VendorExtensions.map((ve, idx) => (
            <FormGroup fieldId={'ve-form-group-' + idx}>
              <FormGroup fieldId={'vendor-extension-id-group-' + idx} label={'Extension Id ' + (idx + 1)}>
                <TextInput
                  value={ve.extensionId}
                  type="text"
                  onChange={(x) => this.setExtensionId(x, idx)}
                  id={'vendor-extension-id-' + idx}
                  name={'vendor-extension-id-' + idx}
                  style={{ textAlign: 'left' }}
                  isValid={!!ve.extensionId}
                />
              </FormGroup>
              <FormGroup
                fieldId={'ve-form-group-' + idx}
                label="Parameters"
                className="vendorParamsEntry"
                helperTextInvalid={this.state.parseValidationMessages[idx]}
                validated={this.state.parseValidationMessages[idx] == '' ? 'success' : 'error'}
              >
                <ClipboardCopy
                  id={'vendor-params-' + idx}
                  variant={ClipboardCopyVariant.expansion}
                  onChange={(x) => this.setParameters(x as string, idx)}
                >
                  {JSON.stringify(ve.parameters, null, ' ')}
                </ClipboardCopy>
              </FormGroup>
              <Button id={'delete-ve-' + idx} onClick={() => this.deleteVendorExtension(idx)} variant="tertiary">
                Delete this Extension
              </Button>
            </FormGroup>
          ))}
          <Button id="add-new-ve" onClick={() => this.addVendorExtension()} variant="tertiary">
            Add New Vendor Extension
          </Button>
        </GalleryItem>
      </Gallery>
    );
  }
}
