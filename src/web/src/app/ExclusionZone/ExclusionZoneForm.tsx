import React from 'react';
import {
  FormGroup,
  TextInput,
  FormSelectOption,
  FormSelect,
  ActionGroup,
  Button,
  Alert,
  AlertActionCloseButton,
  InputGroupText,
  InputGroup,
  Gallery,
  GalleryItem,
} from '@patternfly/react-core';
import { HeightType, IndoorOutdoorType, ExclusionZoneRequest, RatResponse } from '../Lib/RatApiTypes';
import { cacheItem, getCacheItem } from '../Lib/RatApi';
import { logger } from '../Lib/Logger';
import { Limit } from '../Lib/Admin';

/**
 * ExclusionZoneForm.tsx: form for capturing paramaters for exclusion zone and validating them
 * author: Sam Smucny
 */

/**
 * Form component for entering parameters for Exclusion Zone
 * @param onSubmit callback to parent when a valid object of parameters is ready to be submitted
 */
export class ExclusionZoneForm extends React.Component<
  {
    limit: Limit;
    onSubmit: (a: ExclusionZoneRequest) => Promise<any>;
    onCopyPaste?: (formData: ExclusionZoneRequest, updateCallback: (v: ExclusionZoneRequest) => void) => void;
    /** Extra actions in the same button row as Send Request / Copy-Paste (e.g. Cancel, LiDAR, RAS) */
    footerActions?: React.ReactNode;
  },
  {
    height?: number;
    heightType: HeightType;
    heightCert?: number;
    insideOutside: IndoorOutdoorType;
    eirp?: number;
    eirpStr: string;
    bandwidth?: number;
    centerFreq?: number;
    fsid?: number;
    mesgType?: 'danger' | 'info' | 'success';
    mesgTitle?: string;
    mesgBody?: string;
  }
> {
  private static heightTypes: string[] = [HeightType.AGL.toString(), HeightType.AMSL.toString()];
  private static inOutDoors: [string, string][] = [
    ['INDOOR', IndoorOutdoorType.INDOOR.toString()],
    ['OUTDOOR', IndoorOutdoorType.OUTDOOR.toString()],
    ['ANY', IndoorOutdoorType.ANY.toString()],
  ];
  private static bandwidths: number[] = [20, 40, 80, 160];
  private static startingFreq = 5945;
  private static centerFrequencies: Map<number, number[]> = new Map([
    [
      20,
      Array(59)
        .fill(0)
        .map((_, i) => i * 20 + ExclusionZoneForm.startingFreq + 10),
    ],
    [
      40,
      Array(29)
        .fill(0)
        .map((_, i) => i * 40 + ExclusionZoneForm.startingFreq + 20),
    ],
    [
      80,
      Array(14)
        .fill(0)
        .map((_, i) => i * 80 + ExclusionZoneForm.startingFreq + 40),
    ],
    [
      160,
      Array(7)
        .fill(0)
        .map((_, i) => i * 160 + ExclusionZoneForm.startingFreq + 80),
    ],
  ]);

  constructor(props: Readonly<{ limit: Limit; onSubmit: (a: ExclusionZoneRequest) => Promise<RatResponse<string>> }>) {
    super(props);
    this.state = {
      height: undefined,
      heightType: HeightType.AMSL,
      heightCert: undefined,
      insideOutside: IndoorOutdoorType.INDOOR,
      eirp: undefined,
      eirpStr: '',
      bandwidth: undefined,
      centerFreq: undefined,
      fsid: undefined,
      mesgType: undefined,
    };
  }

  componentDidMount() {
    const st = getCacheItem('exclusionZoneForm');
    if (st !== undefined) {
      if (st.eirp !== undefined && st.eirpStr === undefined) {
        st.eirpStr = String(st.eirp);
      } else if (st.eirpStr === undefined) {
        st.eirpStr = '';
      }
      this.setState(st);
    }
  }

  componentWillUnmount() {
    const state: any = this.state;
    state.mesgType = undefined;
    cacheItem('exclusionZoneForm', state);
  }

  private validHeight = (s?: number) => s !== undefined && Number.isFinite(s);
  private validHeightType = (s?: string) => !!s;
  private validHeightCert = (s?: number) => s !== undefined && Number.isFinite(s) && s >= 0;
  private validInOut = (s?: string) => !!s;
  private validEirp = (s?: number) => {
    if (this.props.limit.enforce) {
      return s !== undefined && Number.isFinite(s) && s >= this.props.limit.limit;
    }
    return s !== undefined && Number.isFinite(s);
  };
  private validBandwidth = (s?: number) => s !== undefined && Number.isFinite(s);
  private validCenterFreq = (s?: number) => s !== undefined && Number.isFinite(s);
  private validFsid = (s?: number) => !!s && Number.isInteger(s);

  private submit = () => {
    const allValid = [
      this.validHeight(this.state.height),
      this.validHeightType(this.state.heightType),
      this.validHeightCert(this.state.heightCert),
      this.validInOut(this.state.insideOutside),
      this.validEirp(this.state.eirp),
      this.validBandwidth(this.state.bandwidth),
      this.validCenterFreq(this.state.centerFreq),
      this.validFsid(this.state.fsid),
    ].reduce((p, c) => p && c);

    if (!allValid) {
      logger.error('Invalid inputs when submitting ExlusionZoneForm');
      this.setState({ mesgType: 'danger', mesgTitle: 'Invalid Inputs', mesgBody: 'One or more inputs are invalid' });
      return;
    }

    logger.info('running exclusion zone with form state: ', this.state);
    this.setState({ mesgType: 'info' });
    this.props
      .onSubmit({
        height: this.state.height!,
        heightType: this.state.heightType,
        heightUncertainty: this.state.heightCert!,
        indoorOutdoor: this.state.insideOutside,
        EIRP: this.state.eirp!,
        bandwidth: this.state.bandwidth!,
        centerFrequency: this.state.centerFreq!,
        FSID: this.state.fsid!,
      })
      .then(() => {
        this.setState({ mesgType: undefined });
      })
      .catch(() => {
        this.setState({ mesgType: undefined });
      });
  };

  private copyPasteClick = () =>
    this.props.onCopyPaste &&
    this.props.onCopyPaste(
      {
        height: this.state.height!,
        heightType: this.state.heightType,
        heightUncertainty: this.state.heightCert!,
        indoorOutdoor: this.state.insideOutside,
        EIRP: this.state.eirp!,
        bandwidth: this.state.bandwidth!,
        centerFrequency: this.state.centerFreq!,
        FSID: this.state.fsid!,
      } as ExclusionZoneRequest,
      this.setParams,
    );

  private setParams = (v: ExclusionZoneRequest) =>
    this.setState({
      height: v.height,
      heightType: v.heightType,
      heightCert: v.heightUncertainty,
      insideOutside: v.indoorOutdoor,
      eirp: v.EIRP,
      eirpStr: String(v.EIRP),
      bandwidth: v.bandwidth,
      centerFreq: v.centerFrequency,
      fsid: v.FSID,
    });

  render() {
    return (
      <>
        <Gallery hasGutter>
          <GalleryItem>
            <FormGroup label="RLAN Height" fieldId="horizontal-form-height">
              <InputGroup>
                <TextInput
                  value={this.state.height}
                  onChange={(_event, x) => this.setState({ height: Number(x) })}
                  type="number"
                  step="any"
                  id="horizontal-form-height"
                  name="horizontal-form-height"
                  validated={this.validHeight(this.state.height) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                />
                <InputGroupText>meters</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="RLAN Height Type" fieldId="horizontal-form-height-type">
              <InputGroup>
                <FormSelect
                  value={this.state.heightType}
                  // @ts-ignore
                  onChange={(_event, x) => this.setState({ heightType: HeightType[x] })}
                  id="horzontal-form-height-type"
                  name="horizontal-form-height-type"
                  validated={this.validHeightType(this.state.heightType) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                >
                  <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                  {ExclusionZoneForm.heightTypes.map((option) => (
                    <FormSelectOption key={option} value={option} label={option} />
                  ))}
                </FormSelect>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="RLAN Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert">
              <InputGroup>
                <TextInput
                  value={this.state.heightCert}
                  onChange={(_event, x) => this.setState({ heightCert: Number(x) })}
                  type="number"
                  step="any"
                  id="horizontal-form-height-cert"
                  name="horizontal-form-height-cert"
                  validated={this.validHeightCert(this.state.heightCert) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                />
                <InputGroupText>meters</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="Indoor/Outdoor" fieldId="horizontal-form-indoor-outdoor">
              <InputGroup>
                <FormSelect
                  value={this.state.insideOutside}
                  // @ts-ignore
                  onChange={(_event, x) => this.setState({ insideOutside: x })}
                  id="horzontal-form-indoor-outdoor"
                  name="horizontal-form-indoor-outdoor"
                  validated={this.validInOut(this.state.insideOutside) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                >
                  <FormSelectOption
                    isDisabled={true}
                    key={undefined}
                    value={undefined}
                    label="Select device environment"
                  />
                  {ExclusionZoneForm.inOutDoors.map(([option, label]) => (
                    <FormSelectOption isDisabled={false} key={label} value={label} label={label} />
                  ))}
                </FormSelect>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup
              label={this.props.limit.enforce ? 'RLAN EIRP (Min: ' + this.props.limit.limit + ' dBm)' : 'RLAN EIRP'}
              fieldId="horizontal-form-eirp"
            >
              <InputGroup>
                <TextInput
                  value={this.state.eirpStr}
                  onChange={(_event, x) =>
                    this.setState({ eirpStr: x, eirp: x === '' ? undefined : parseFloat(x) })
                  }
                  type="text"
                  inputMode="decimal"
                  id="horizontal-form-eirp"
                  name="horizontal-form-eirp"
                  validated={this.validEirp(this.state.eirp) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                />
                <InputGroupText>dBm</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="RLAN Bandwidth" fieldId="horizontal-form-bandwidth">
              <InputGroup>
                <FormSelect
                  value={this.state.bandwidth}
                  // @ts-ignore
                  onChange={(_event, x) => this.setState({ bandwidth: Number.parseInt(x), centerFreq: undefined })}
                  id="horzontal-form-bandwidth"
                  name="horizontal-form-bandwidth"
                  validated={this.validBandwidth(this.state.bandwidth) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                >
                  <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select bandwidth" />
                  {ExclusionZoneForm.bandwidths.map((option) => (
                    <FormSelectOption isDisabled={false} key={option} value={option} label={String(option)} />
                  ))}
                </FormSelect>
                <InputGroupText>MHz</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="RLAN Center Frequency" fieldId="horizontal-form-centfreq">
              <InputGroup>
                <FormSelect
                  value={this.state.centerFreq}
                  // @ts-ignore
                  onChange={(_event, x) => this.setState({ centerFreq: Number.parseInt(x) })}
                  id="horzontal-form-centfreq"
                  name="horizontal-form-centfreq"
                  validated={this.validCenterFreq(this.state.centerFreq) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                  isDisabled={!this.state.bandwidth}
                >
                  <FormSelectOption
                    isDisabled={true}
                    key={undefined}
                    value={undefined}
                    label="Select center frequency"
                  />
                  {(ExclusionZoneForm.centerFrequencies.get(this.state.bandwidth || 0) || []).map((option) => (
                    <FormSelectOption isDisabled={false} key={option} value={option} label={String(option)} />
                  ))}
                </FormSelect>
                <InputGroupText>MHz</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup label="FSID" fieldId="horizontal-form-fsid">
              <InputGroup>
                <TextInput
                  value={this.state.fsid}
                  onChange={(_event, x) => this.setState({ fsid: Number(x) })}
                  type="number"
                  step="any"
                  id="horizontal-form-fsid"
                  name="horizontal-form-fsid"
                  validated={this.validFsid(this.state.fsid) ? 'default' : 'error'}
                  style={{ textAlign: 'right' }}
                />
              </InputGroup>
            </FormGroup>
          </GalleryItem>
        </Gallery>
        <br />
        <React.Fragment>
          {this.state.mesgType && this.state.mesgType !== 'info' && (
            <Alert
              variant={this.state.mesgType}
              title={this.state.mesgTitle || ''}
              actionClose={<AlertActionCloseButton onClose={() => this.setState({ mesgType: undefined })} />}
            >
              {this.state.mesgBody}
            </Alert>
          )}
        </React.Fragment>
        <ActionGroup className="afc-button-group afc-button-group--after-form">
          <Button variant="primary" isDisabled={this.state.mesgType === 'info'} onClick={this.submit}>
            Send Request
          </Button>
          <Button key="open-modal" variant="secondary" onClick={() => this.copyPasteClick()}>
            Copy/Paste
          </Button>
          {this.props.footerActions}
        </ActionGroup>
      </>
    );
  }
}
