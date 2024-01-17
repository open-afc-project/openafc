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
  GalleryItem,
  Tooltip,
  Chip,
  ChipGroup,
  TooltipPosition,
  Radio
} from "@patternfly/react-core";
import { HeightType, IndoorOutdoorType, HeatMapRequest, RatResponse, HeatMapAnalysisType, HeatMapFsIdType } from "../Lib/RatApiTypes";
import { getCacheItem, cacheItem } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { Timer } from "../Components/Timer";
import { letin } from "../Lib/Utils";
import { Limit } from "../Lib/Admin";
import { OutlinedQuestionCircleIcon } from "@patternfly/react-icons";
import { CertificationId, InquiredChannel, OperatingClass, OperatingClassIncludeType } from "../Lib/RatAfcTypes";
import { OperatingClassForm } from "../Components/OperatingClassForm";

/**
* HeatMapForm.tsx: form for capturing paramaters for heat map and validating them
* author: Sam Smucny
*/

/**
 * Interface definition of form data
 */
export interface HeatMapFormData {
  serialNumber: string,
  certificationId: CertificationId[],
  inquiredChannel: InquiredChannel,
  analysis: HeatMapAnalysisType,
  fsIdType: HeatMapFsIdType,
  fsId?: number
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
export class HeatMapForm extends React.Component<{
  limit: Limit, rulesetIds: string[],
  onSubmit: (a: HeatMapFormData) => Promise<RatResponse<string>>, onCopyPaste?: (formData: HeatMapFormData, updateCallback: (v: HeatMapFormData) => void) => void
},
  {
    height: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    heightType: { kind: "s", val?: HeightType } | { kind: "b", in?: HeightType, out?: HeightType },
    heightCert: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    insideOutside: IndoorOutdoorType,
    eirp: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number },
    mesgType?: "info" | "danger",
    mesgTitle?: string,
    mesgBody?: string,
    serialNumber?: string,
    certificationId: CertificationId[],
    newCertificationId?: string,
    newCertificationRulesetId?: string,
    rulesetIds: string[];
    operatingClasses: OperatingClass[],
    inquiredChannel?: InquiredChannel,
    analysisType: HeatMapAnalysisType,
    fsIdType: HeatMapFsIdType,
    fsId?: number

  }> {

  private static heightTypes: string[] = [HeightType.AGL.toString(), HeightType.AMSL.toString()];
  private static bandwidths: number[] = [20, 40, 80, 160];
  private static startingFreq = 5945;
  private static centerFrequencies: Map<number, number[]> = new Map([
    [20, Array(59).fill(0).map((_, i) => i * 20 + HeatMapForm.startingFreq + 10)],
    [40, Array(29).fill(0).map((_, i) => i * 40 + HeatMapForm.startingFreq + 20)],
    [80, Array(14).fill(0).map((_, i) => i * 80 + HeatMapForm.startingFreq + 40)],
    [160, Array(7).fill(0).map((_, i) => i * 160 + HeatMapForm.startingFreq + 80)]
  ]);

  constructor(props: Readonly<{ limit: Limit, onSubmit: (a: HeatMapFormData) => Promise<RatResponse<string>>; rulesetIds: string[] }>) {
    super(props);
    this.state = {
      height: { kind: "s", val: undefined },
      heightType: { kind: "s", val: HeightType.AMSL },
      heightCert: { kind: "s", val: undefined },
      insideOutside: IndoorOutdoorType.INDOOR,
      eirp: { kind: "s", val: undefined },
      mesgType: undefined,
      serialNumber: "HeatMapSerialNumber",
      certificationId: [{ id: "HeatMapCertificationId", rulesetId: "US_47_CFR_PART_15_SUBPART_E" }],
      rulesetIds: this.props.rulesetIds,
      operatingClasses: [
        {
          num: 131,
          include: OperatingClassIncludeType.Some
        },
        {
          num: 132,
          include: OperatingClassIncludeType.Some
        },
        {
          num: 133,
          include: OperatingClassIncludeType.Some
        },
        {
          num: 134,
          include: OperatingClassIncludeType.Some
        },
        {
          num: 136,
          include: OperatingClassIncludeType.Some
        },
        {
          num: 137,
          include: OperatingClassIncludeType.Some
        }
      ],
      analysisType: HeatMapAnalysisType.ItoN,
      fsIdType: HeatMapFsIdType.All
    }
  };


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
    if (this.state.insideOutside === IndoorOutdoorType.BUILDING) {
      return s.kind === "b" && [Number.isFinite(s.in!) && s.in! >= 0, Number.isFinite(s.out!) && s.out! >= 0];
    }
    else {
      return s.kind === "s" && Number.isFinite(s.val!) && [s.val! >= 0];
    }

  }
  private validInOut = (s?: string) => !!s;
  private validEirp = (s: { kind: "s", val?: number } | { kind: "b", in?: number, out?: number }) => {
    if (this.state.analysisType === HeatMapAnalysisType.ItoN) {
      if (this.state.insideOutside === IndoorOutdoorType.BUILDING) {
        if (this.props.limit.indoorEnforce) {
          return s.kind === "b" && [Number.isFinite(s.in!) && s.in! >= this.props.limit.indoorLimit, Number.isFinite(s.out!) && s.out! >= this.props.limit.indoorLimit];
        } else {
          return s.kind === "b" && [Number.isFinite(s.in!), Number.isFinite(s.out!)];
        }
      } else if (this.state.insideOutside === IndoorOutdoorType.INDOOR) {

        if (this.props.limit.indoorEnforce) {
          return s.kind === "s" && Number.isFinite(s.val!) && [s.val! >= this.props.limit.indoorLimit];
        } else {
          return s.kind === "s" && [Number.isFinite(s.val!)];
        }
      }
      else if (this.state.insideOutside === IndoorOutdoorType.OUTDOOR) {

        if (this.props.limit.outdoorEnforce) {
          return s.kind === "s" && Number.isFinite(s.val!) && [s.val! >= this.props.limit.outdoorLimit];
        } else {
          return s.kind === "s" && [Number.isFinite(s.val!)];
        }
      } else {
        return true;
      }
    }else{
      return true;
    }
  }
  private submit = () => {
    const allValid = [true].concat(
      this.validHeight(this.state.height),
      this.validHeightType(this.state.heightType),
      this.validHeightCert(this.state.heightCert),
      [this.validInOut(this.state.insideOutside)],
      this.validEirp(this.state.eirp),
      [!!this.state.inquiredChannel],
      [!!this.state.serialNumber],
      [!!this.state.certificationId && this.state.certificationId.length > 0],
      [this.state.fsIdType === HeatMapFsIdType.All || (this.state.fsIdType === HeatMapFsIdType.Single && !!this.state.fsId)]
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
      inquiredChannel: this.state.inquiredChannel,
      indoorOutdoor: inout,
      certificationId: this.state.certificationId,
      serialNumber: this.state.serialNumber,
      analysis: this.state.analysisType,
      fsIdType: this.state.fsIdType,
      fsId: this.state.fsId,

    } as HeatMapFormData as any)
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

      indoorOutdoor: inout,
      analysis: this.state.analysisType,
      certificationId: this.state.certificationId,
      fsIdType: this.state.fsIdType,
      fsId: this.state.fsId,
      inquiredChannel: this.state.inquiredChannel,
      serialNumber: this.state.serialNumber,

    })) as HeatMapFormData, this.setParams);

  private setParams = (v: HeatMapFormData) => {
    let newState = {
      analysisType: v.analysis,
      certificationId: v.certificationId,
      fsIdType: v.fsIdType,
      inquiredChannel: v.inquiredChannel,
      serialNumber: v.serialNumber,
    }
    if (v.fsIdType === HeatMapFsIdType.Single) {
      newState["fsId"] = v.fsId
    }
    this.setState(newState);
    this.updateOperatingClass({ num: v.inquiredChannel.globalOperatingClass, include: OperatingClassIncludeType.Some, channels: [v.inquiredChannel.channelCfi] }, 0)
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

  deleteCertificationId(currentCid: string): void {
    const copyOfcertificationId = this.state.certificationId.filter(x => x.id != currentCid);
    this.setState({ certificationId: copyOfcertificationId });
  }
  addCertificationId(newCertificationId: CertificationId): void {
    const copyOfcertificationId = this.state.certificationId.slice();
    copyOfcertificationId.push({ id: newCertificationId.id, rulesetId: newCertificationId.rulesetId });
    this.setState({ certificationId: copyOfcertificationId, newCertificationId: '', newCertificationRulesetId: this.props.rulesetIds[0] });
  }

  resetCertificationId(newCertificationId: CertificationId): void {
    const copyOfcertificationId = [{ id: newCertificationId.id, rulesetId: newCertificationId.rulesetId }];
    this.setState({ certificationId: copyOfcertificationId, newCertificationId: '', newCertificationRulesetId: this.props.rulesetIds[0] });
  }

  updateOperatingClass(x: { num: number; include: OperatingClassIncludeType; channels?: number[] | undefined; }, i: number): void {
    let ocs = this.state.operatingClasses.map(oc => {
      if (oc.num == x.num) {
        return { num: x.num, include: OperatingClassIncludeType.Some, channels: x.channels }
      } else {
        return { num: oc.num, include: OperatingClassIncludeType.Some, channels: [] }
      }
    })
    if (!!x.channels && x.channels.length >= 1) {
      this.setState({ inquiredChannel: { globalOperatingClass: x.num, channelCfi: x.channels[0] }, operatingClasses: ocs })
    } else {
      this.setState({ inquiredChannel: undefined, operatingClasses: ocs })
    }
  }

  render() {

    return (<>
      <Gallery gutter="sm">
        <GalleryItem>
          <FormGroup
            label="Serial Number"
            fieldId="horizontal-form-name"
            helperText="Must be unique for every AP"
          >
            {" "}<Tooltip
              position={TooltipPosition.top}
              enableFlip={true}
              className="fs-feeder-loss-tooltip"
              maxWidth="40.0rem"
              content={
                <>
                  <p>The following Serial Number and Certification ID pair can be used for any rulesetID:</p>
                  <ul style={{ listStyleType: "none" }} >
                    <li>Serial Number=HeatMapSerialNumber</li>

                    <li>CertificationId=HeatMapCertificationId</li>
                  </ul>
                </>
              }
            >
              <OutlinedQuestionCircleIcon />
            </Tooltip>
            <TextInput
              value={this.state.serialNumber}
              type="text"
              id="horizontal-form-name"
              aria-describedby="horizontal-form-name-helper"
              name="horizontal-form-name"
              onChange={x => this.setState({ serialNumber: x })}
              isValid={!!this.state.serialNumber}
              style={{ textAlign: "right" }}
            />
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup
            label="Certification Id"
            fieldId="horizontal-form-certification-id"
            helperTextInvalid="Provide at least one Certification Id"
            // helperTextInvalidIcon={<ExclamationCircleIcon />} //This is not supported in our version of Patternfly
            validated={this.state.certificationId.length > 0 ? "success" : "error"}
          >
            {" "}<Tooltip
              position={TooltipPosition.top}
              enableFlip={true}
              className="fs-feeder-loss-tooltip"
              maxWidth="40.0rem"
              content={
                <>
                  <p>The following Serial Number and Certification ID pair can be used for any rulesetID: </p>
                  <ul style={{ listStyleType: "none" }} >
                    <li>Serial Number=HeatMapSerialNumber</li>

                    <li>CertificationId=HeatMapCertificationId</li>
                  </ul>
                </>
              }
            >
              <OutlinedQuestionCircleIcon />
            </Tooltip>
            <ChipGroup aria-orientation="vertical"
            >
              {this.state.certificationId.map(currentCid => (
                <Chip width="100%" key={currentCid.id} onClick={() => this.deleteCertificationId(currentCid.id)}>
                  {currentCid.rulesetId + " " + currentCid.id}
                </Chip>
              ))}
            </ChipGroup>
            <div>
              {" "}<Button key="add-certificationId" variant="tertiary"
                onClick={() => this.addCertificationId({
                  id: this.state.newCertificationId!,
                  rulesetId: this.state.newCertificationRulesetId!
                })}>
                +
              </Button>
            </div>
            <div>
              <FormSelect
                label="Ruleset"
                value={this.state.newCertificationRulesetId}
                onChange={x => this.setState({ newCertificationRulesetId: x })}
                type="text"
                step="any"
                id="horizontal-form-certification-nra"
                name="horizontal-form-certification-nra"
                style={{ textAlign: "left" }}
                placeholder="Ruleset"
              >
                {
                  this.props.rulesetIds.map((x) => (
                    <FormSelectOption label={x} key={x} value={x} />
                  ))
                }
                <FormSelectOption label="Unknown RS - For Testing only" value={"UNKNOWN_RS"} key={"unknown"} />
              </FormSelect>

              <TextInput
                label="Id"
                value={this.state.newCertificationId}
                onChange={x => this.setState({ newCertificationId: x })}
                type="text"
                step="any"
                id="horizontal-form-certification-list"
                name="horizontal-form-certification-list"
                style={{ textAlign: "left" }}
                placeholder="Id"
              />
            </div>
          </FormGroup>
        </GalleryItem>
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
                <FormSelectOption isDisabled={false} key={IndoorOutdoorType.INDOOR} value={IndoorOutdoorType.INDOOR} label={"Indoor"} />
                <FormSelectOption isDisabled={false} key={IndoorOutdoorType.OUTDOOR} value={IndoorOutdoorType.OUTDOOR} label={"Outdoor"} />
                <FormSelectOption isDisabled={false} key={IndoorOutdoorType.BUILDING} value={IndoorOutdoorType.BUILDING} label={"Selected per Building Data"} />
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
        {this.state.analysisType === HeatMapAnalysisType.ItoN &&
          <GalleryItem>
            {this.state.eirp.kind === "s" ?
              <FormGroup label={this.props.limit.indoorEnforce ? "RLAN EIRP (Min: " + this.props.limit.indoorLimit + " dBm)" : "RLAN EIRP"} fieldId="horizontal-form-eirp">
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
                <FormGroup label={this.props.limit.indoorEnforce ? "Indoor RLAN EIRP (Min: " + this.props.limit.indoorLimit + " dBm)" : "Indoor RLAN EIRP"} fieldId="horizontal-form-eirp-in">
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
                <FormGroup label={this.props.limit.outdoorEnforce ? "Outdoor RLAN EIRP (Min: " + this.props.limit.outdoorLimit + " dBm)" : "Outdoor RLAN EIRP"} fieldId="horizontal-form-eirp-out">
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
        }
        <GalleryItem>
          <FormGroup label="Analysis Type" fieldId="horizontal-form-analysis-type">
            <InputGroup>
              <FormSelect
                value={this.state.analysisType}
                // @ts-ignore
                onChange={x => this.setState({ analysisType: x })}
                id="horizontal-form-analysis-type"
                name="horizontal-form-analysis-type"
                style={{ textAlign: "right" }}
              >
                <FormSelectOption isDisabled={false} key={"eirp"} value={HeatMapAnalysisType.EIRP} label="Availability (EIRP)" />
                <FormSelectOption isDisabled={false} key={"iton"} value={HeatMapAnalysisType.ItoN} label="I/N" />
              </FormSelect>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="FS Ids" fieldId="horizontal-form-fs-id">
            <Radio
              id="radio-fs-all"
              key="radio-fs-all"
              name="radio-fs"
              label="All FS Ids"
              value={HeatMapFsIdType.All}
              isChecked={this.state.fsIdType === HeatMapFsIdType.All}
              onChange={(isChecked, e) => { if (isChecked) { this.setState({ fsIdType: HeatMapFsIdType.All }) } }}
            />
            <br />
            <Radio
              id="radio-fs-single"
              key="radio-fs-single"
              name="radio-fs"
              label="Specify FS Id"
              value={HeatMapFsIdType.Single}
              isChecked={this.state.fsIdType === HeatMapFsIdType.Single}
              onChange={(isChecked, e) => { if (isChecked) { this.setState({ fsIdType: HeatMapFsIdType.Single }) } }}
            />
            <br />
            {this.state.fsIdType === HeatMapFsIdType.Single ?
              <TextInput
                label="FS Id to Check"
                value={this.state.fsId}
                onChange={x => !isNaN(+x) ? this.setState({ fsId: Number(x) }) : this.setState({ fsId: undefined })}
                type="number"
                id="horizontal-form-fs-id-single"
                name="horizontal-form-fs-id-single"
                style={{ textAlign: "left" }}
                placeholder="FS Id"
              />
              :
              <></>
            }
          </FormGroup>
        </GalleryItem>
      </Gallery>
      <FormGroup label="Inquired Channel(s)" fieldId="horizontal-form-operating-class" className="inquiredChannelsSection">
        <Gallery className="nestedGallery" width={"1200px"} >
          {this.state.operatingClasses.map(
            (e, i) => (
              <OperatingClassForm key={i}
                operatingClass={e}
                onChange={x => this.updateOperatingClass(x, i)}
                allowOnlyOneChannel={true}
              ></OperatingClassForm>
            )
          )}
        </Gallery>
      </FormGroup>
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
