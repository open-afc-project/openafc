import * as React from "react";
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
  GalleryItem
} from "@patternfly/react-core";
import { HeightType, IndoorOutdoorType, HeatMapRequest, RatResponse } from "../Lib/RatApiTypes";
import { getCacheItem, cacheItem } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { Timer } from "../Components/Timer";
import { letin } from "../Lib/Utils";
import { Limit } from "../Lib/Admin";

/**
* HeatMapForm.tsx: form for capturing paramaters for heat map and validating them
* author: Sam Smucny
*/

/**
 * Interface definition of form data
 */
export interface HeatMapFormData {
  bandwidth: number,
  centerFrequency: number,
  indoorOutdoor: {
    kind: IndoorOutdoorType.INDOOR | IndoorOutdoorType.OUTDOOR | IndoorOutdoorType.ANY,
    EIRP: number,
    height: number,
    heightType: HeightType,
    heightUncertainty: number
  } | {
    kind: IndoorOutdoorType.BUILDING,
    in: {
      EIRP: number,
      height: number,
      heightType: HeightType,
      heightUncertainty: number
    },
    out: {
      EIRP: number,
      height: number,
      heightType: HeightType,
      heightUncertainty: number
    }
  }
}

/**
 * Form component for filling in heat map parameters
 */
export class HeatMapForm extends React.Component<{ limit : Limit, onSubmit: (a: HeatMapFormData) => Promise<RatResponse<string>>, onCopyPaste?: (formData: HeatMapRequest, updateCallback: (v: HeatMapRequest) => void) => void },
  {
    height: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    heightType: { kind: "s", val?: HeightType } | { kind: "b", in?: HeightType, out?: HeightType },
    heightCert: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    insideOutside: IndoorOutdoorType,
    eirp: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    bandwidth?: number,
    centerFrequency?: number,
    mesgType?: "info" | "danger",
    mesgTitle?: string,
    mesgBody?: string
  }> {

  private static heightTypes: string[] = [HeightType.AGL.toString(), HeightType.AMSL.toString()];
  private static inOutDoors: [string , string][] = [
    ["INDOOR", IndoorOutdoorType.INDOOR.toString()], 
    ["OUTDOOR", IndoorOutdoorType.OUTDOOR.toString()], 
    ["BUILDING", IndoorOutdoorType.BUILDING.toString()]
  ];
  private static bandwidths: number[] = [20, 40, 80, 160];
  private static startingFreq = 5945;
  private static centerFrequencies: Map<number, number[]> = new Map([
    [20, Array(59).fill(0).map((_, i) => i * 20 + HeatMapForm.startingFreq + 10)],
    [40, Array(29).fill(0).map((_, i) => i * 40 + HeatMapForm.startingFreq + 20)],
    [80, Array(14).fill(0).map((_, i) => i * 80 + HeatMapForm.startingFreq + 40)],
    [160, Array(7).fill(0).map((_, i) => i * 160 + HeatMapForm.startingFreq + 80)]
  ]);

  constructor(props: Readonly<{limit : Limit, onSubmit: (a: HeatMapFormData) => Promise<RatResponse<string>>; }>) {
    super(props);
    this.state = {
      height: { kind: "s", val: undefined },
      heightType: { kind: "s", val: HeightType.AMSL },
      heightCert: { kind: "s", val: undefined },
      insideOutside: IndoorOutdoorType.INDOOR,
      eirp: { kind: "s", val: undefined },
      bandwidth: undefined,
      centerFrequency: undefined,
      mesgType: undefined
    };
  }

  componentDidMount() {
    const st = getCacheItem("heatMapForm");
    if (st !== undefined)
      this.setState(st);
  }

  componentWillUnmount() {
    const state: any = this.state;
    state.mesgType = undefined;
    cacheItem("heatMapForm", state);
  }

  private validHeight = (s: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number }) =>
    (this.state.insideOutside === IndoorOutdoorType.BUILDING ?
      s.kind === "b" && [Number.isFinite(s.in!), Number.isFinite(s.out!)]
      : s.kind === "s" && [Number.isFinite(s.val!)]);
  private validHeightType = (s: { kind: "s", val?: HeightType } | { kind: "b", in?: HeightType, out?: HeightType }) =>
    (this.state.insideOutside === IndoorOutdoorType.BUILDING ?
      s.kind === "b" && [!!s.in, !!s.out]
      : s.kind === "s" && [!!s.val]);
  private validHeightCert = (s: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number }) => {
    if(this.state.insideOutside === IndoorOutdoorType.BUILDING) {
      return s.kind === "b" && [Number.isFinite(s.in!) && s.in! >= 0, Number.isFinite(s.out!) && s.out! >= 0];
    }
    else { 
      return s.kind === "s" && Number.isFinite(s.val!) && [s.val! >= 0];
    }
  
  }
  private validInOut = (s?: string) => !!s;
  private validEirp = (s: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number }) => {
    if(this.state.insideOutside === IndoorOutdoorType.BUILDING) {
      if(this.props.limit.enforce) {
        return s.kind === "b" && [Number.isFinite(s.in!) && s.in! >= this.props.limit.limit, Number.isFinite(s.out!) && s.out! >= this.props.limit.limit];
      } else {
        return s.kind === "b" && [Number.isFinite(s.in!), Number.isFinite(s.out!)];
      }
    } else {
        if(this.props.limit.enforce) {
          return s.kind === "s" && Number.isFinite(s.val!) && [s.val! >= this.props.limit.limit];
        } else {
            return s.kind === "s" && [Number.isFinite(s.val!)]; 
        }
    }
  }
  private validBandwidth = (s?: number) => s !== undefined && Number.isFinite(s);
  private validCenterFreq = (s?: number) => s !== undefined && Number.isFinite(s);

  private submit = () => {
    const allValid = [true].concat(
      this.validHeight(this.state.height),
      this.validHeightType(this.state.heightType),
      this.validHeightCert(this.state.heightCert),
      [this.validInOut(this.state.insideOutside)],
      this.validEirp(this.state.eirp),
      [this.validBandwidth(this.state.bandwidth)],
      [this.validCenterFreq(this.state.centerFrequency)]
    ).reduce((p, c) => p && c);

    if (!allValid) {
      logger.error("Invalid inputs when submitting HeatMapForm");
      this.setState({ mesgType: "danger", mesgTitle: "Invalid Inputs", mesgBody: "One or more inputs are invalid" });
      return;
    }

    logger.info("running heat map with form state: ", this.state);
    const inout: any = this.state.insideOutside === IndoorOutdoorType.BUILDING ?
      {
        kind: this.state.insideOutside,
        in: {
          // @ts-ignore
          EIRP: this.state.eirp.in,
          // @ts-ignore
          height: this.state.height.in,
          // @ts-ignore
          heightType: this.state.heightType.in,
          // @ts-ignore
          heightUncertainty: this.state.heightCert.in
        },
        out: {
          // @ts-ignore
          EIRP: this.state.eirp.out,
          // @ts-ignore
          height: this.state.height.out,
          // @ts-ignore
          heightType: this.state.heightType.out,
          // @ts-ignore
          heightUncertainty: this.state.heightCert.out
        }
      } :
      {
        kind: this.state.insideOutside,
        // @ts-ignore
        EIRP: this.state.eirp.val,
        // @ts-ignore
        height: this.state.height.val,
        // @ts-ignore
        heightType: this.state.heightType.val,
        // @ts-ignore
        heightUncertainty: this.state.heightCert.val
      };
    this.setState({ mesgType: "info" });
    this.props.onSubmit({
      bandwidth: this.state.bandwidth!,
      centerFrequency: this.state.centerFrequency!,
      indoorOutdoor: inout
    } as unknown as any)
      .then(res => {
        this.setState({ mesgType: undefined });
      })
  }

  private defaultInOut(s: string) {
    // @ts-ignore
    const inOut: IndoorOutdoorType = s;
    if (inOut === IndoorOutdoorType.BUILDING) {
      return {
        insideOutside: inOut,
        height: { kind: "b", in: 0, out: 0 },
        heightType: { kind: "b", in: HeightType.AMSL, out: HeightType.AMSL },
        heightCert: { kind: "b", in: 0, out: 0 },
        eirp: { kind: "b", in: 0, out: 0 }
      }
    } else {
      return {
        insideOutside: inOut,
        height: { kind: "s", val: 0 },
        heightType: { kind: "s", val: HeightType.AMSL },
        heightCert: { kind: "s", val: 0 },
        eirp: { kind: "s", val: 0 }
      }
    }
  }

  private copyPasteClick = () => this.props.onCopyPaste && this.props.onCopyPaste(letin(this.state.insideOutside === IndoorOutdoorType.BUILDING ?
    {
      kind: this.state.insideOutside,
      in: {
        // @ts-ignore
        EIRP: this.state.eirp.in,
        // @ts-ignore
        height: this.state.height.in,
        // @ts-ignore
        heightType: this.state.heightType.in,
        // @ts-ignore
        heightUncertainty: this.state.heightCert.in
      },
      out: {
        // @ts-ignore
        EIRP: this.state.eirp.out,
        // @ts-ignore
        height: this.state.height.out,
        // @ts-ignore
        heightType: this.state.heightType.out,
        // @ts-ignore
        heightUncertainty: this.state.heightCert.out
      }
    } :
    {
      kind: this.state.insideOutside,
      // @ts-ignore
      EIRP: this.state.eirp.val,
      // @ts-ignore
      height: this.state.height.val,
      // @ts-ignore
      heightType: this.state.heightType.val,
      // @ts-ignore
      heightUncertainty: this.state.heightCert.val
    }, inout => ({
    bandwidth: this.state.bandwidth!,
    centerFrequency: this.state.centerFrequency!,
    indoorOutdoor: inout
  })) as HeatMapRequest, this.setParams);

  private setParams = (v: HeatMapRequest) => {
    this.setState({
      bandwidth: v.bandwidth,
      centerFrequency: v.centerFrequency
    });

    if (v.indoorOutdoor.kind === IndoorOutdoorType.BUILDING) {
      this.setState({
        insideOutside: v.indoorOutdoor.kind,
        height: { kind: "b", in: v.indoorOutdoor.in.height, out: v.indoorOutdoor.out.height },
        heightType: { kind: "b", in: v.indoorOutdoor.in.heightType, out: v.indoorOutdoor.out.heightType },
        heightCert: { kind: "b", in: v.indoorOutdoor.in.heightUncertainty, out: v.indoorOutdoor.out.heightUncertainty },
        eirp: { kind: "b", in: v.indoorOutdoor.in.EIRP, out: v.indoorOutdoor.out.EIRP }
      });
    } else {
      this.setState({
        insideOutside: v.indoorOutdoor.kind,
        height: { kind: "s", val: v.indoorOutdoor.height },
        heightType: { kind: "s", val: v.indoorOutdoor.heightType },
        heightCert: { kind: "s", val: v.indoorOutdoor.heightUncertainty },
        eirp: { kind: "s", val: v.indoorOutdoor.EIRP }
      })
    }
  }

  render() {

    return (<>
      <Gallery gutter="sm">
        <GalleryItem>
          <FormGroup label="Indoor/Outdoor" fieldId="horizontal-form-indoor-outdoor">
            <InputGroup>
              <FormSelect
                value={this.state.insideOutside}
                // @ts-ignore
                onChange={x => this.setState(this.defaultInOut(x))}
                id="horzontal-form-indoor-outdoor"
                name="horizontal-form-indoor-outdoor"
                isValid={this.validInOut(this.state.insideOutside)}
                style={{ textAlign: "right" }}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select device environment" />
                {HeatMapForm.inOutDoors.map(([option, label]) => (
                  <FormSelectOption isDisabled={false} key={label} value={label} label={label} />
                ))}
              </FormSelect>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          {this.state.height.kind === "s" ?
            <FormGroup label="RLAN Height" fieldId="horizontal-form-height">
              <InputGroup>
                <TextInput
                  value={this.state.height.val}
                  onChange={x => this.setState({ height: { kind: "s", val: Number(x) } })}
                  type="number"
                  step="any"
                  id="horizontal-form-height"
                  name="horizontal-form-height"
                  isValid={this.validHeight(this.state.height)[0]}
                  style={{ textAlign: "right" }}
                />
                <InputGroupText>meters</InputGroupText>
              </InputGroup>
            </FormGroup> : <>
              <FormGroup label="Indoor RLAN Height" fieldId="horizontal-form-height-in">
                <InputGroup>
                  <TextInput
                    value={this.state.height.in}
                    // @ts-ignore
                    onChange={x => this.setState(st => ({ height: { kind: "b", out: st.height.out, in: Number(x) } }))}
                    type="number"
                    step="any"
                    id="horizontal-form-height-in"
                    name="horizontal-form-height-in"
                    isValid={this.validHeight(this.state.height)[0]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>meters</InputGroupText>
                </InputGroup>
              </FormGroup>
              <FormGroup label="Outdoor RLAN Height" fieldId="horizontal-form-height-out">
                <InputGroup>
                  <TextInput
                    value={this.state.height.out}
                    // @ts-ignore
                    onChange={x => this.setState(st => ({ height: { kind: "b", in: st.height.in, out: Number(x) } }))}
                    type="number"
                    step="any"
                    id="horizontal-form-height-out"
                    name="horizontal-form-height-out"
                    isValid={this.validHeight(this.state.height)[1]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>meters</InputGroupText>
                </InputGroup>
              </FormGroup></>
          }
        </GalleryItem>
        <GalleryItem>
          {this.state.heightType.kind === "s" ?
            <FormGroup label="RLAN Height Type" fieldId="horizontal-form-height-type">
              <InputGroup>
                <FormSelect
                  value={this.state.heightType.val}
                  // @ts-ignore
                  onChange={x => this.setState({ heightType: { kind: "s", val: HeightType[x] } })}
                  id="horzontal-form-height-type"
                  name="horizontal-form-height-type"
                  isValid={this.validHeightType(this.state.heightType)[0]}
                  style={{ textAlign: "right" }}
                >
                  <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                  {HeatMapForm.heightTypes.map((option) => (
                    <FormSelectOption key={option} value={option} label={option} />
                  ))}
                </FormSelect>
              </InputGroup>
            </FormGroup> : <>
              <FormGroup label="Indoor RLAN Height Type" fieldId="horizontal-form-height-type-in">
                <InputGroup>
                  <FormSelect
                    value={this.state.heightType.in}
                    // @ts-ignore
                    onChange={x => this.setState({ heightType: { kind: "b", in: HeightType[x], out: this.state.heightType.out } })}
                    id="horzontal-form-height-type-in"
                    name="horizontal-form-height-type-in"
                    isValid={this.validHeightType(this.state.heightType)[0]}
                    style={{ textAlign: "right" }}
                  >
                    <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                    {HeatMapForm.heightTypes.map((option) => (
                      <FormSelectOption key={option} value={option} label={option} />
                    ))}
                  </FormSelect>
                </InputGroup>
              </FormGroup>
              <FormGroup label="Outdoor RLAN Height Type" fieldId="horizontal-form-height-type-out">
                <InputGroup>
                  <FormSelect
                    value={this.state.heightType.out}
                    // @ts-ignore
                    onChange={x => this.setState({ heightType: { kind: "b", out: HeightType[x], in: this.state.heightType.in } })}
                    id="horzontal-form-height-type-out"
                    name="horizontal-form-height-type-out"
                    isValid={this.validHeightType(this.state.heightType)[1]}
                    style={{ textAlign: "right" }}
                  >
                    <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                    {HeatMapForm.heightTypes.map((option) => (
                      <FormSelectOption key={option} value={option} label={option} />
                    ))}
                  </FormSelect>
                </InputGroup>
              </FormGroup></>
          }
        </GalleryItem>
        <GalleryItem>
          {this.state.heightCert.kind === "s" ?
            <FormGroup label="RLAN Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert">
              <InputGroup>
                <TextInput
                  value={this.state.heightCert.val}
                  onChange={x => this.setState({ heightCert: { kind: "s", val: Number(x) } })}
                  type="number"
                  step="any"
                  id="horizontal-form-height-cert"
                  name="horizontal-form-height-cert"
                  isValid={this.validHeightCert(this.state.heightCert)[0]}
                  style={{ textAlign: "right" }}
                />
                <InputGroupText>meters</InputGroupText>
              </InputGroup>
            </FormGroup> : <>
              <FormGroup label="Indoor RLAN Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert-in">
                <InputGroup>
                  <TextInput
                    value={this.state.heightCert.in}
                    // @ts-ignore
                    onChange={x => this.setState({ heightCert: { kind: "b", in: Number(x), out: this.state.heightCert.out } })}
                    type="number"
                    step="any"
                    id="horizontal-form-height-cert-in"
                    name="horizontal-form-height-cert-in"
                    isValid={this.validHeightCert(this.state.heightCert)[0]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>meters</InputGroupText>
                </InputGroup>
              </FormGroup>
              <FormGroup label="Outdoor RLAN Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert-out">
                <InputGroup>
                  <TextInput
                    value={this.state.heightCert.out}
                    // @ts-ignore
                    onChange={x => this.setState({ heightCert: { kind: "b", out: Number(x), in: this.state.heightCert.in } })}
                    type="number"
                    step="any"
                    id="horizontal-form-height-cert-out"
                    name="horizontal-form-height-cert-out"
                    isValid={this.validHeightCert(this.state.heightCert)[1]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>meters</InputGroupText>
                </InputGroup>
              </FormGroup> </>
          }
        </GalleryItem>
        <GalleryItem>
          {this.state.eirp.kind === "s" ?
            <FormGroup label={this.props.limit.enforce ? "RLAN EIRP (Min: " + this.props.limit.limit  + " dBm)": "RLAN EIRP"} fieldId="horizontal-form-eirp">
              <InputGroup>
                <TextInput
                  value={this.state.eirp.val}
                  onChange={x => this.setState({ eirp: { kind: "s", val: Number(x) } })}
                  type="number"
                  step="any"
                  id="horizontal-form-eirp"
                  name="horizontal-form-eirp"
                  isValid={this.validEirp(this.state.eirp)[0]}
                  style={{ textAlign: "right" }}
                />
                <InputGroupText>dBm</InputGroupText>
              </InputGroup>
            </FormGroup> : <>
              <FormGroup label={this.props.limit.enforce ? "Indoor RLAN EIRP (Min: " + this.props.limit.limit  + " dBm)": "Indoor RLAN EIRP"} fieldId="horizontal-form-eirp-in">
                <InputGroup>
                  <TextInput
                    value={this.state.eirp.in}
                    // @ts-ignore
                    onChange={x => this.setState({ eirp: { kind: "b", in: Number(x), out: this.state.eirp.out } })}
                    type="number"
                    step="any"
                    id="horizontal-form-eirp-in"
                    name="horizontal-form-eirp-in"
                    isValid={this.validEirp(this.state.eirp)[0]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>dBm</InputGroupText>
                </InputGroup>
              </FormGroup>
              <FormGroup label={this.props.limit.enforce ? "Outdoor RLAN EIRP (Min: " + this.props.limit.limit  + " dBm)": "Outdoor RLAN EIRP"} fieldId="horizontal-form-eirp-out">
                <InputGroup>
                  <TextInput
                    value={this.state.eirp.out}
                    // @ts-ignore
                    onChange={x => this.setState({ eirp: { kind: "b", out: Number(x), in: this.state.eirp.in } })}
                    type="number"
                    step="any"
                    id="horizontal-form-eirp-out"
                    name="horizontal-form-eirp-out"
                    isValid={this.validEirp(this.state.eirp)[1]}
                    style={{ textAlign: "right" }}
                  />
                  <InputGroupText>dBm</InputGroupText>
                </InputGroup>
              </FormGroup>
            </>
          }
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="RLAN Bandwidth" fieldId="horizontal-form-bandwidth">
            <InputGroup>
              <FormSelect
                value={this.state.bandwidth}
                // @ts-ignore
                onChange={x => this.setState({ bandwidth: Number.parseInt(x), centerFrequency: undefined })}
                id="horzontal-form-bandwidth"
                name="horizontal-form-bandwidth"
                isValid={this.validBandwidth(this.state.bandwidth)}
                style={{ textAlign: "right" }}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select bandwidth" />
                {HeatMapForm.bandwidths.map((option) => (
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
                value={this.state.centerFrequency}
                // @ts-ignore
                onChange={x => this.setState({ centerFrequency: Number.parseInt(x) })}
                id="horzontal-form-centfreq"
                name="horizontal-form-centfreq"
                isValid={this.validCenterFreq(this.state.centerFrequency)}
                style={{ textAlign: "right" }}
                isDisabled={!this.state.bandwidth}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select center frequency" />
                {(HeatMapForm.centerFrequencies.get(this.state.bandwidth || 0) || []).map((option) => (
                  <FormSelectOption isDisabled={false} key={option} value={option} label={String(option)} />
                ))}
              </FormSelect>
              <InputGroupText>MHz</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
      </Gallery>
      <br />
      <React.Fragment>
        {this.state.mesgType && this.state.mesgType !== "info" && (
          <Alert
            variant={this.state.mesgType}
            title={this.state.mesgTitle || ""}
            action={<AlertActionCloseButton onClose={() => this.setState({ mesgType: undefined })} />}
          >
            {this.state.mesgBody}
          </Alert>
        )}
      </React.Fragment>
      <br />
      <>
        <Button variant="primary" isDisabled={this.state.mesgType === "info"} onClick={this.submit}>Send Request</Button>{" "}
        <Button key="open-modal" variant="secondary" onClick={() => this.copyPasteClick()}>Copy/Paste</Button>
      </>
    </>);
  }
}
