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
import { HeightType, IndoorOutdoorType, PAWSRequest } from "../Lib/RatApiTypes";
import { cacheItem, getCacheItem } from "../Lib/RatApi";

// PAWSForm.tsx: form for capturing paramaters for virtual ap and validating them
// author: Sam Smucny

interface IFormStaging {
  serialNumber: string,
  validSN: boolean,
  manufacturer?: string,
  validManufacturer: boolean,
  model?: string,
  validModel: boolean,
  latitude?: number,
  validLat: boolean,
  longitude?: number,
  validLng: boolean,
  majAxis?: number,
  validMajAxis: boolean,
  minAxis?: number,
  validMinAxis: boolean,
  orientation?: number,
  validOrientation: boolean,
  height?: number,
  validHeight: boolean,
  heightType: HeightType,
  validHeightType: boolean,
  heightCert?: number,
  validHeightCert: boolean,
  insideOutside: IndoorOutdoorType,
  validInsideOutside: boolean,
  errorMessage?: string,
  submitting: boolean
}

export class PAWSForm extends React.Component<{ parentName: string, onSubmit: (a: PAWSRequest) => Promise<void>, onCopyPaste?: (formData: PAWSRequest, updateCallback: (v: PAWSRequest) => void) => void }, IFormStaging> {

  private heightTypes: string[] = [HeightType.AGL.toString(), HeightType.AMSL.toString()];
  private inOutDoors: [string, string][] = [
    ["INDOOR", IndoorOutdoorType.INDOOR.toString()],
    ["OUTDOOR", IndoorOutdoorType.OUTDOOR.toString()],
    ["ANY", IndoorOutdoorType.ANY.toString()]
  ];

  constructor(props: Readonly<{ parentName: string, onSubmit: (a: PAWSRequest) => Promise<void>; }>) {
    super(props);
    this.state = {
      validSN: props.parentName === "analysisPage",
      validManufacturer: true,
      validModel: true,
      validLat: false,
      validLng: false,
      validOrientation: false,
      validMajAxis: false,
      validMinAxis: false,
      validHeight: false,
      validHeightType: true,
      validHeightCert: false,
      validInsideOutside: true,
      serialNumber: props.parentName === "analysisPage" ? "analysis-ap" : "",
      manufacturer: undefined,
      model: undefined,
      latitude: undefined,
      longitude: undefined,
      majAxis: undefined,
      minAxis: undefined,
      orientation: undefined,
      height: undefined,
      heightType: HeightType.AMSL,
      heightCert: undefined,
      insideOutside: IndoorOutdoorType.INDOOR,
      errorMessage: undefined,
      submitting: false
    };

  }

  componentDidMount() {
    const st = getCacheItem("pawsFormState-" + this.props.parentName);
    if (st !== undefined)
      this.setState(st);
  }

  componentWillUnmount() {
    const state: IFormStaging = this.state;
    state.errorMessage = undefined;
    state.submitting = false;
    cacheItem("pawsFormState-" + this.props.parentName, state);
  }

  private setSN(value: string) {
    this.setState({ serialNumber: value });
    if (value !== undefined && value !== "") {
      this.setState({ validSN: true });
    } else {
      this.setState({ validSN: false });
    }
  }

  private setManufacturer(value: string) {
    this.setState({ manufacturer: value });
  }

  private setModel(value: string) {
    this.setState({ model: value });
  }

  private setLat(num: number) {
    this.setState({ latitude: num });
    if (num >= -90 && num <= 90) {
      this.setState({ validLat: true });
    } else {
      this.setState({ validLat: false });
    }
  }

  private setLng(num: number) {
    this.setState({ longitude: num });
    if (num >= -180 && num <= 180) {
      this.setState({ validLng: true });
    } else {
      this.setState({ validLng: false });
    }
  }

  // in the following two methods, the min/maj axis may be undefined, but in those casese the
  // comparison will have the correct behavior of returning 'false'
  private setMajAxis(num: number) {
    this.setState({ majAxis: num });
    if (num >= 0 && this.state.minAxis! <= num) {
      this.setState({ validMajAxis: true, validMinAxis: this.state.minAxis! >= 0 && this.state.minAxis! <= num });
    } else {
      this.setState({ validMajAxis: false, validMinAxis: this.state.minAxis! >= 0 && this.state.minAxis! <= num });
    }
  }

  private setMinAxis(num: number) {
    this.setState({ minAxis: num });
    if (num >= 0 && num <= this.state.majAxis!) {
      this.setState({ validMinAxis: true, validMajAxis: this.state.majAxis! >= 0 && this.state.majAxis! >= num });
    } else {
      this.setState({ validMinAxis: false, validMajAxis: this.state.majAxis! >= 0 && this.state.majAxis! >= num });
    }
  }

  private setOrientation(num: number) {
    this.setState({ orientation: num });
    if (num >= 0 && num < 180) {
      this.setState({ validOrientation: true });
    } else {
      this.setState({ validOrientation: false });
    }
  }

  private setHeight(num: number) {
    this.setState({ height: num });
    if (Number.isFinite(num)) {
      this.setState({ validHeight: true });
    } else {
      this.setState({ validHeight: false });
    }
  }

  private setHeightType(value: string) {
    if (value !== undefined) {
      // @ts-ignore
      this.setState({ heightType: HeightType[value], validHeightType: true });
    } else {
      this.setState({ validHeightType: false });
    }
  }

  private setHeightCert(num: number) {
    this.setState({ heightCert: num });
    if (num >= 0) {
      this.setState({ validHeightCert: true });
    } else {
      this.setState({ validHeightCert: false });
    }
  }

  private setInOut(x: string) {
    if (x !== undefined) {
      // @ts-ignore
      this.setState({ insideOutside: x, validInsideOutside: true });
    } else {
      this.setState({ validInsideOutside: false });
    }
  }

  private submit = () => {
    // Validate all inputs before sending to parent
    const isValidArray = [
      this.state.validSN,
      this.state.validLat || this.props.parentName === "analysisPage",
      this.state.validLng || this.props.parentName === "analysisPage",
      this.state.validMajAxis,
      this.state.validMinAxis,
      this.state.validOrientation,
      this.state.validHeight,
      this.state.validHeightType,
      this.state.validHeightCert,
      this.state.validInsideOutside
    ];

    if (isValidArray.reduce((a, b) => a && b)) {
      this.setState({ submitting: true })
      this.props.onSubmit({
        type: "AVAIL_SPECTRUM_REQ",
        version: "1.0",
        deviceDesc: {
          serialNumber: this.state.serialNumber!,
          manufacturerId: this.state.manufacturer,
          modelId: this.state.model,
          rulesetIds: ["AFC-6GHZ-DEMO-1.1"]
        },
        location: {
          point: {
            center: {
              latitude: this.state.latitude!,
              longitude: this.state.longitude!
            },
            semiMajorAxis: this.state.majAxis!,
            semiMinorAxis: this.state.minAxis!,
            orientation: this.state.orientation!
          }
        },
        antenna: {
          height: this.state.height!,
          heightType: this.state.heightType,
          heightUncertainty: this.state.heightCert!
        },
        capabilities: { indoorOutdoor: this.state.insideOutside }
      } as PAWSRequest).then(() => this.setState({ submitting: false }));

      this.setState({ errorMessage: undefined });
    } else {
      this.setState({ errorMessage: "One or more inputs are invalid" });
    }
  }

  private copyPasteClick = () => this.props.onCopyPaste && this.props.onCopyPaste({
    type: "AVAIL_SPECTRUM_REQ",
    version: "1.0",
    deviceDesc: {
      serialNumber: this.state.serialNumber!,
      manufacturerId: this.state.manufacturer,
      modelId: this.state.model,
      rulesetIds: ["AFC-6GHZ-DEMO-1.1"]
    },
    location: {
      point: {
        center: {
          latitude: this.state.latitude!,
          longitude: this.state.longitude!
        },
        semiMajorAxis: this.state.majAxis!,
        semiMinorAxis: this.state.minAxis!,
        orientation: this.state.orientation!
      }
    },
    antenna: {
      height: this.state.height!,
      heightType: this.state.heightType,
      heightUncertainty: this.state.heightCert!
    },
    capabilities: { indoorOutdoor: this.state.insideOutside }
  } as PAWSRequest, this.setParams);

  private setParams = (v: PAWSRequest) => {
    this.setLat(v.location.point.center.latitude);
    this.setLng(v.location.point.center.longitude);
    this.setSN(v.deviceDesc.serialNumber);
    this.setManufacturer(v.deviceDesc.manufacturerId);
    this.setModel(v.deviceDesc.modelId);
    this.setOrientation(v.location.point.orientation);
    this.setHeight(v.antenna.height);
    this.setHeightType(v.antenna.heightType);
    this.setHeightCert(v.antenna.heightUncertainty);
    this.setInOut(v.capabilities.indoorOutdoor);
    this.setState({ majAxis: v.location.point.semiMajorAxis, minAxis: v.location.point.semiMinorAxis });
    if (v.location.point.semiMajorAxis >= 0 && v.location.point.semiMinorAxis >= 0 && v.location.point.semiMajorAxis >= v.location.point.semiMinorAxis) {
      this.setState({ validMajAxis: true, validMinAxis: true });
    } else {
      this.setState({ validMajAxis: false, validMinAxis: false });
    }
  }

  render() {

    return (<>
      <Gallery gutter="sm">
        {this.props.parentName === "pawsInterface" && (<>
          <GalleryItem>
            <FormGroup
              label="Serial Number"
              fieldId="horizontal-form-name"
              helperText="Must be unique for every AP"
            >
              <TextInput
                value={this.state.serialNumber}
                type="text"
                id="horizontal-form-name"
                aria-describedby="horizontal-form-name-helper"
                name="horizontal-form-name"
                onChange={x => this.setSN(x)}
                isValid={this.state.validSN}
                style={{ textAlign: "right" }}
              />
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup
              label="Manufacturer"
              fieldId="horizontal-form-manufacturer"
            >
              <TextInput
                value={this.state.manufacturer}
                type="text"
                id="horizontal-form-manufacturer"
                aria-describedby="horizontal-form-manufacturer-helper"
                name="horizontal-form-manufacturer"
                onChange={x => this.setManufacturer(x)}
                isValid={this.state.validManufacturer}
                style={{ textAlign: "right" }}
              />
            </FormGroup>
          </GalleryItem>
          <GalleryItem>
            <FormGroup
              label="Model"
              fieldId="horizontal-form-model"
            >
              <TextInput
                value={this.state.model}
                type="text"
                id="horizontal-form-model"
                aria-describedby="horizontal-form-model-helper"
                name="horizontal-form-model"
                onChange={x => this.setModel(x)}
                isValid={this.state.validModel}
                style={{ textAlign: "right" }}
              />
            </FormGroup>
          </GalleryItem></>)}
        {this.props.parentName === "pawsInterface" && (
          <GalleryItem>
            <FormGroup label="Latitude"
              fieldId="horizontal-form-latitude">
              <InputGroup>
                <TextInput
                  value={this.state.latitude}
                  onChange={x => this.setLat(Number(x))}
                  type="number"
                  step="any"
                  id="horizontal-form-latitude"
                  name="horizontal-form-latitude"
                  isValid={this.state.validLat}
                  style={{ textAlign: "right" }}
                />
                <InputGroupText>degrees</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
        )}
        {this.props.parentName === "pawsInterface" && (
          <GalleryItem>
            <FormGroup label="Longitude"
              fieldId="horizontal-form-longitude">
              <InputGroup>
                <TextInput
                  value={this.state.longitude}
                  onChange={x => this.setLng(Number(x))}
                  type="number"
                  step="any"
                  id="horizontal-form-longitude"
                  name="horizontal-form-longitude"
                  isValid={this.state.validLng}
                  style={{ textAlign: "right" }}
                />
                <InputGroupText>degrees</InputGroupText>
              </InputGroup>
            </FormGroup>
          </GalleryItem>
        )}
        <GalleryItem>
          <FormGroup label="Semi-major Axis"
            fieldId="horizontal-form-maj-axis"
            helperText="Attribute of uncertainty ellipse">
            <InputGroup>
              <TextInput
                value={this.state.majAxis}
                onChange={x => this.setMajAxis(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-maj-axis"
                name="horizontal-form-maj-axis"
                isValid={this.state.validMajAxis}
                style={{ textAlign: "right" }}
              />
              <InputGroupText>meters</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Semi-minor Axis"
            fieldId="horizontal-form-min-axis"
            helperText="Attribute of uncertainty ellipse">
            <InputGroup>
              <TextInput
                value={this.state.minAxis}
                onChange={x => this.setMinAxis(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-min-axis"
                name="horizontal-form-min-axis"
                isValid={this.state.validMinAxis}
                style={{ textAlign: "right" }}
              />
              <InputGroupText>meters</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Orientation"
            fieldId="horizontal-form-orientation"
            helperText="Degrees of major-axis off north clockwise in [0, 180)" >
            <InputGroup>
              <TextInput
                value={this.state.orientation}
                onChange={x => this.setOrientation(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-orientation"
                name="horizontal-form-orientation"
                isValid={this.state.validOrientation}
                style={{ textAlign: "right" }}
              /><InputGroupText>degrees</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Height" fieldId="horizontal-form-height">
            <InputGroup>
              <TextInput
                value={this.state.height}
                onChange={x => this.setHeight(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-height"
                name="horizontal-form-height"
                isValid={this.state.validHeight}
                style={{ textAlign: "right" }}
              />
              <InputGroupText>meters</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Height Type" fieldId="horizontal-form-height-type">
            <InputGroup>
              <FormSelect
                value={this.state.heightType}
                onChange={x => this.setHeightType(x)}
                id="horzontal-form-height-type"
                name="horizontal-form-height-type"
                isValid={this.state.validHeightType}
                style={{ textAlign: "right" }}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                {this.heightTypes.map((option) => (
                  <FormSelectOption key={option} value={option} label={option} />
                ))}
              </FormSelect>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Height Uncertainty (+/-)" fieldId="horizontal-form-height-cert">
            <InputGroup>
              <TextInput
                value={this.state.heightCert}
                onChange={x => this.setHeightCert(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-height-cert"
                name="horizontal-form-height-cert"
                isValid={this.state.validHeightCert}
                style={{ textAlign: "right" }}
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
                onChange={x => this.setInOut(x)}
                id="horzontal-form-indoor-outdoor"
                name="horizontal-form-indoor-outdoor"
                isValid={this.state.validInsideOutside}
                style={{ textAlign: "right" }}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select device environment" />
                {this.inOutDoors.map(([option, label]) => (
                  <FormSelectOption isDisabled={false} key={label} value={label} label={label} />
                ))}
              </FormSelect>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
      </Gallery>
      <br />
      <React.Fragment>
        {this.state.errorMessage !== undefined && (
          <Alert
            variant="danger"
            title="Invalid Input"
            action={<AlertActionCloseButton onClose={() => this.setState({ errorMessage: undefined })} />}
          >
            {this.state.errorMessage}
          </Alert>
        )}
      </React.Fragment>
      <br />
      <>
        <Button variant="primary" isDisabled={this.state.submitting} onClick={this.submit}>Send Request</Button>{" "}
        <Button key="open-modal" variant="secondary" onClick={() => this.copyPasteClick()}>Copy/Paste</Button>
      </>
    </>);
  }
}
