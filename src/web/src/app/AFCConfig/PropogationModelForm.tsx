import * as React from 'react';
import {
  FormGroup,
  FormSelect,
  FormSelectOption,
  TextInput,
  InputGroup,
  InputGroupText,
  Tooltip,
  TooltipPosition,
} from '@patternfly/react-core';
import { OutlinedQuestionCircleIcon } from '@patternfly/react-icons';
import {
  BuildingSourceValues,
  CustomPropagation,
  CustomPropagationLOSOptions,
  FCC6GHz,
  FSPL,
  IsedDbs06,
  PropagationModel,
  Win2ItmClutter,
  Win2ItmDb,
  BrazilPropModel,
  OfcomPropModel,
} from '../Lib/RatApiTypes';

/**
 * PropogationModelForm.tsx: sub form of afc config form
 * author: Sam Smucny
 */

/**
 * Sub for for propogation model
 */
export class PropogationModelForm extends React.PureComponent<{
  data: PropagationModel;
  region: string;
  onChange: (x: PropagationModel) => void;
}> {
  private getModelOptionsByRegion = (region: string) => {
    if (region.endsWith('CA')) {
      return [
        'FSPL',
        'ITM with building data',
        //"ITM with no building data",
        'ISED DBS-06',
        'Ray Tracing',
        'Custom',
      ];
    } else if (region.endsWith('BR')) {
      return [
        'FSPL',
        'ITM with building data',
        //"ITM with no building data",
        'Brazilian Propagation Model',
        'Ray Tracing',
        'Custom',
      ];
    } else if (region.endsWith('GB')) {
      return [
        'FSPL',
        'ITM with building data',
        //"ITM with no building data",
        'Ofcom Propagation Model',
        'Ray Tracing',
        'Custom',
      ];
    } else {
      return [
        'FSPL',
        'ITM with building data',
        //"ITM with no building data",
        'FCC 6GHz Report & Order',
        'Ray Tracing',
        'Custom',
      ];
    }
  };

  private setKind = (s: string) => {
    switch (s) {
      case 'FSPL':
        this.props.onChange({ kind: s, fsplUseGroundDistance: false } as FSPL);
        break;
      case 'ITM with building data':
        this.props.onChange({
          kind: s,
          win2ProbLosThreshold: 10,
          win2ConfidenceCombined: 50,
          win2ConfidenceLOS: 50,
          win2ConfidenceNLOS: 50,
          itmConfidence: 50,
          itmReliability: 50,
          p2108Confidence: 50,
          buildingSource: 'LiDAR',
        } as Win2ItmDb);
        break;
      case 'ITM with no building data':
        this.props.onChange({
          kind: s,
          win2ProbLosThreshold: 10,
          win2ConfidenceCombined: 50,
          win2ConfidenceLOS: 50,
          win2ConfidenceNLOS:50,
          itmConfidence: 50,
          itmReliability: 50,
          p2108Confidence: 50,
          terrainSource: 'SRTM (90m)',
        } as Win2ItmClutter);
        break;
      case 'FCC 6GHz Report & Order':
        this.props.onChange({
          kind: s,
          win2ConfidenceCombined: 16,
          win2ConfidenceLOS: 16,
          winner2LOSOption: 'BLDG_DATA_REQ_TX',
          win2UseGroundDistance: false,
          fsplUseGroundDistance: false,
          winner2HgtFlag: false,
          winner2HgtLOS: 15,
          itmConfidence: 5,
          itmReliability: 20,
          p2108Confidence: 25,
          buildingSource: 'None',
          terrainSource: '3DEP (30m)',
        } as FCC6GHz);
        break;
      case 'Ray Tracing':
        break; // do nothing
      case 'Custom':
        this.props.onChange({
          kind: s,
          winner2LOSOption: 'UNKNOWN',
          win2ConfidenceCombined: 50,
          itmConfidence: 50,
          itmReliability: 50,
          p2108Confidence: 50,
          rlanITMTxClutterMethod: 'FORCE_TRUE',
          buildingSource: 'None',
          terrainSource: '3DEP (30m)',
          win2ConfidenceLOS: 50,
          win2UseGroundDistance: false,
          fsplUseGroundDistance: false,
          winner2HgtFlag: false,
          winner2HgtLOS: 15,
        } as CustomPropagation);
        break;
      case 'ISED DBS-06':
        this.props.onChange({
          kind: s,
          win2ConfidenceCombined: 16,
          win2ConfidenceLOS: 50,
          win2ConfidenceNLOS: 50,
          winner2LOSOption: 'CDSM',
          win2UseGroundDistance: false,
          fsplUseGroundDistance: false,
          winner2HgtFlag: false,
          winner2HgtLOS: 15,
          itmConfidence: 5,
          itmReliability: 20,
          p2108Confidence: 10,
          surfaceDataSource: 'Canada DSM (2000)',
          terrainSource: '3DEP (30m)',
          rlanITMTxClutterMethod: 'FORCE_TRUE',
        } as IsedDbs06);
        break;
      case 'Brazilian Propagation Model':
        this.props.onChange({
          kind: s,
          win2ConfidenceCombined: 50,
          win2ConfidenceLOS: 50,
          winner2LOSOption: 'BLDG_DATA_REQ_TX',
          win2UseGroundDistance: false,
          fsplUseGroundDistance: false,
          winner2HgtFlag: false,
          winner2HgtLOS: 50,
          itmConfidence: 50,
          itmReliability: 50,
          p2108Confidence: 50,
          buildingSource: 'None',
          terrainSource: 'SRTM (30m)',
        } as BrazilPropModel);
        break;
      case 'Ofcom Propagation Model':
        this.props.onChange({
          kind: s,
          win2ConfidenceCombined: 50,
          win2ConfidenceLOS: 50,
          winner2LOSOption: 'BLDG_DATA_REQ_TX',
          win2UseGroundDistance: false,
          fsplUseGroundDistance: false,
          winner2HgtFlag: false,
          winner2HgtLOS: 50,
          itmConfidence: 50,
          itmReliability: 50,
          p2108Confidence: 50,
          buildingSource: 'None',
          terrainSource: 'SRTM (30m)',
        } as OfcomPropModel);
        break;
    }
  };

  setWin2ConfidenceCombined = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { win2ConfidenceCombined: n }));
  };

  setWin2ConfidenceLOS = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { win2ConfidenceLOS: n }));
  };

  setWinConfidenceNLOS = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { win2ConfidenceNLOS: n }));
  };

  setWin2ConfidenceLOS_NLOS = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { win2ConfidenceLOS: n, win2ConfidenceNLOS: n }));
  };

  setItmConfidence = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { itmConfidence: n }));
  };

  setItmReliability = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { itmReliability: n }));
  };

  setP2108Confidence = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { p2108Confidence: n }));
  };

  setProbLOS = (s: string) => {
    const n: number = Number(s);
    this.props.onChange(Object.assign(this.props.data, { win2ProbLosThreshold: n }));
  };

  setBuildingSource = (s: BuildingSourceValues) => {
    const model = this.props.data;
    if (model.hasOwnProperty('buildingSource')) {
      // Ensure that there is terrain source is set to default when there is building data

      if (model.kind === 'FCC 6GHz Report & Order') {
        let newData: Partial<FCC6GHz> = { buildingSource: s };
        if (model.buildingSource === 'None' && s !== 'LiDAR') {
          newData.terrainSource = '3DEP (30m)';
        }

        if (s == 'LiDAR') {
          if (!model.win2ConfidenceLOS) {
            newData.win2ConfidenceLOS = 50;
          }
          if (!model.win2ConfidenceNLOS) {
            newData.win2ConfidenceNLOS = 50;
          }
        }
        if (s === 'None') {
          newData.winner2LOSOption = 'UNKNOWN';
          newData.win2ConfidenceLOS = 50;
        } else {
          newData.winner2LOSOption = 'BLDG_DATA_REQ_TX';
        }

        this.props.onChange(Object.assign(this.props.data, newData));
      } else if (model.kind === 'Brazilian Propagation Model' || model.kind === 'Ofcom Propagation Model') {
        let newData: Partial<BrazilPropModel> = { buildingSource: s };
        if (model.buildingSource === 'None' && s !== 'LiDAR') {
          newData.terrainSource = 'SRTM (30m)';
        }
        if (s == 'LiDAR') {
          if (!model.win2ConfidenceLOS) {
            newData.win2ConfidenceLOS = 50;
          }
          if (!model.win2ConfidenceNLOS) {
            newData.win2ConfidenceNLOS = 50;
          }
        }
        if (s === 'None') {
          newData.winner2LOSOption = 'UNKNOWN';
          newData.win2ConfidenceLOS = 50;
        } else {
          newData.winner2LOSOption = 'BLDG_DATA_REQ_TX';
        }
        this.props.onChange(Object.assign(this.props.data, newData));
      } else {
        if ((model as Win2ItmDb | CustomPropagation).buildingSource === 'None' && s !== 'None') {
          this.props.onChange(Object.assign(this.props.data, { buildingSource: s, terrainSource: '3DEP (30m)' }));
        } else {
          this.props.onChange(Object.assign(this.props.data, { buildingSource: s }));
        }
      }
    }
  };

  setTerrainSource = (s: string) => {
    this.props.onChange(Object.assign(this.props.data, { terrainSource: s }));
  };

  setSurfaceDataSource = (s: string) => {
    this.props.onChange(Object.assign(this.props.data, { surfaceDataSource: s }));
  };

  setLosOption = (s: string) => {
    let newLos = s as CustomPropagationLOSOptions;

    const model = this.props.data as CustomPropagation;
    let newModel: Partial<CustomPropagation> = { winner2LOSOption: newLos };

    switch (newLos) {
      case 'BLDG_DATA_REQ_TX':
        if (!model.win2ConfidenceLOS) {
          newModel.win2ConfidenceLOS = 50;
        }
        if (!model.win2ConfidenceNLOS) {
          newModel.win2ConfidenceNLOS = 50;
        }

      case 'FORCE_LOS':
        if (model.winner2LOSOption === 'BLDG_DATA_REQ_TX') {
          newModel.buildingSource = 'None';
          newModel.terrainSource = '3DEP (30m)';
        }
        if (!model.win2ConfidenceLOS) {
          newModel.win2ConfidenceLOS = 50;
        }

      case 'FORCE_NLOS':
        if (model.winner2LOSOption === 'BLDG_DATA_REQ_TX') {
          newModel.buildingSource = 'None';
          newModel.terrainSource = '3DEP (30m)';
        }
        if (!model.win2ConfidenceNLOS) {
          newModel.win2ConfidenceNLOS = 50;
        }

      case 'UNKNOWN':
        if (!model.win2ConfidenceCombined) {
          newModel.win2ConfidenceCombined = 50;
        }
        if (!model.win2ConfidenceLOS) {
          newModel.win2ConfidenceLOS = 50;
        }
        if (model.winner2LOSOption === 'BLDG_DATA_REQ_TX') {
          newModel.buildingSource = 'None';
          newModel.terrainSource = '3DEP (30m)';
        }
    }

    this.props.onChange(Object.assign(this.props.data, newModel));
  };

  setItmClutterMethod = (s: string) => {
    const model = this.props.data as CustomPropagation;
    if (model.rlanITMTxClutterMethod === 'BLDG_DATA' && s !== 'BLDG_DATA') {
      this.props.onChange(
        Object.assign(this.props.data, {
          rlanITMTxClutterMethod: s,
          buildingSource: 'None',
          terrainSource: '3DEP (30m)',
        }),
      );
    } else {
      this.props.onChange(Object.assign(this.props.data, { rlanITMTxClutterMethod: s }));
    }
  };
  renderCustomLosNlosConfidence(model: CustomPropagation) {
    switch (model.winner2LOSOption) {
      case 'UNKNOWN':
        //Combined, show nothing
        return (
          <>
            <FormGroup label="WinnerII Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="Winner II LOS Confidence" fieldId="propogation-model-win-los-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceLOS}
                  type="number"
                  onChange={(v) => {
                    this.setWin2ConfidenceLOS(v);
                  }}
                  id="propogation-model-win-los-confidence"
                  name="propogation-model-win-los-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
          </>
        );
      case 'FORCE_LOS':
        return (
          <>
            <FormGroup label="Winner II LOS Confidence" fieldId="propogation-model-win-los-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceLOS}
                  type="number"
                  onChange={(v) => {
                    this.setWin2ConfidenceLOS(v);
                  }}
                  id="propogation-model-win-los-confidence"
                  name="propogation-model-win-los-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
          </>
        );
      case 'FORCE_NLOS':
        return (
          <>
            <FormGroup label="Winner II NLOS Confidence" fieldId="propogation-model-win-nlos-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceNLOS}
                  type="number"
                  onChange={(v) => {
                    this.setWinConfidenceNLOS(v);
                  }}
                  id="propogation-model-win-nlos-confidence"
                  name="propogation-model-win-nlos-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceNLOS >= 0 && model.win2ConfidenceNLOS <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
          </>
        );
      case 'BLDG_DATA_REQ_TX':
        return (
          <>
            <FormGroup label="WinnerII Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>

            <FormGroup label="Winner II LOS/NLOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceLOS}
                  type="number"
                  onChange={(v) => {
                    this.setWin2ConfidenceLOS(v);
                    this.setWinConfidenceNLOS(v);
                  }}
                  id="propogation-model-win-los-nlos-confidence"
                  name="propogation-model-win-los-nlos-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
          </>
        );
    }
  }

  getExpansion = () => {
    const model = this.props.data;
    switch (model.kind) {
      case 'FSPL':
        return <></>;
      case 'ITM with building data':
        return (
          <>
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
              <FormSelect
                value={model.buildingSource}
                onChange={(v) => this.setBuildingSource(v as BuildingSourceValues)}
                id="propogation-model-data-source"
                name="propogation-model-data-source"
                style={{ textAlign: 'right' }}
                isValid={model.buildingSource === 'LiDAR' || model.buildingSource === 'B-Design3D'}
              >
                <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
              </FormSelect>
            </FormGroup>
          </>
        );
      /* case "ITM with no building data":
                return <>
                    <FormGroup
                        label="Winner II Probability Line of Sight Threshold"
                        fieldId="prop-los-threshold"
                    ><InputGroup><TextInput
                        value={model.win2ProbLosThreshold}
                        type="number"
                        onChange={this.setProbLOS}
                        id="prop-los-threshold"
                        name="prop-los-threshold"
                        style={{ textAlign: "right" }}
                        isValid={model.win2ProbLosThreshold >= 0 && model.win2ProbLosThreshold <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Winner II Confidence"
                        fieldId="propogation-model-win-confidence"
                    ><InputGroup><TextInput
                        value={model.win2ConfidenceCombined}
                        type="number"
                        onChange={this.setWin2Confidence}
                        id="propogation-model-win-confidence"
                        name="propogation-model-win-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="ITM Confidence"
                        fieldId="propogation-model-itm-confidence"
                    ><InputGroup><TextInput
                        value={model.itmConfidence}
                        type="number"
                        onChange={this.setItmConfidence}
                        id="propogation-model-itm-confidence"
                        name="propogation-model-itm-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="ITM Reliability"
                        fieldId="propogation-model-itm-reliability"
                    >
                        <InputGroup><TextInput
                            value={model.itmReliability}
                            type="number"
                            onChange={this.setItmReliability}
                            id="propogation-model-itm-reliability"
                            name="propogation-model-itm-reliability"
                            style={{ textAlign: "right" }}
                            isValid={model.itmReliability >= 0 && model.itmReliability <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="P.2108 Percentage of Locations"
                        fieldId="propogation-model-p2108-confidence"
                    ><InputGroup><TextInput
                        value={model.p2108Confidence}
                        type="number"
                        onChange={this.setP2108Confidence}
                        id="propogation-model-p2108-confidence"
                        name="propogation-model-p2108-confidence"
                        style={{ textAlign: "right" }}
                        isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100} />
                            <InputGroupText>%</InputGroupText></InputGroup>
                    </FormGroup>
                    <FormGroup
                        label="Terrain Source"
                        fieldId="terrain-source"
                    >
                        <FormSelect
                            value={model.terrainSource}
                            onChange={this.setTerrainSource}
                            id="terrain-source"
                            name="terrain-source"
                            style={{ textAlign: "right" }}
                            isValid={model.terrainSource === "SRTM (90m)" || model.terrainSource === "3DEP (30m)"}>
                            <FormSelectOption key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
                            <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                        </FormSelect>
                    </FormGroup>
                </> */
      case 'FCC 6GHz Report & Order':
        return (
          <>
            <FormGroup label="Winner II Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            {model.buildingSource === 'LiDAR' ? (
              <>
                <FormGroup label="Winner II LOS/NLOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS_NLOS(v);
                      }}
                      id="propogation-model-win-los-nlos-confidence"
                      name="propogation-model-win-los-nlos-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            {model.buildingSource === 'None' ? (
              <>
                <FormGroup label="Winner II LOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS(v);
                      }}
                      id="propogation-model-win-los-confidence"
                      name="propogation-model-win-los-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
              <InputGroup>
                <TextInput
                  value={model.p2108Confidence}
                  type="number"
                  onChange={this.setP2108Confidence}
                  id="propogation-model-p2108-confidence"
                  name="propogation-model-p2108-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
              <FormSelect
                value={model.buildingSource}
                onChange={(v) => this.setBuildingSource(v as BuildingSourceValues)}
                id="propogation-model-data-source"
                name="propogation-model-data-source"
                style={{ textAlign: 'right' }}
                isValid={
                  model.buildingSource === 'LiDAR' ||
                  model.buildingSource === 'B-Design3D' ||
                  model.buildingSource === 'None'
                }
              >
                <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
                <FormSelectOption key="None" value="None" label="None" />
              </FormSelect>
            </FormGroup>
            {model.buildingSource === 'None' ? (
              <FormGroup label="Terrain Source" fieldId="terrain-source">
                <FormSelect
                  value={model.terrainSource}
                  onChange={this.setTerrainSource}
                  id="terrain-source"
                  name="terrain-source"
                  style={{ textAlign: 'right' }}
                  isValid={model.terrainSource === '3DEP (30m)'}
                >
                  <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                  <FormSelectOption isDisabled={true} key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
                </FormSelect>
              </FormGroup>
            ) : (
              false
            )}
          </>
        );
      case 'Ray Tracing':
        return <></>;
      case 'Custom':
        return (
          <>
            <FormGroup label="WinnerII LoS Option" fieldId="propogation-model-win-los-option">
              {' '}
              <Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="propogation-model-win-los-option-tooltip"
                maxWidth="40.0rem"
                content={
                  <>
                    <p>
                      If <strong>LoS/NLoS per building data</strong> is selected, and if the RLAN is in a region where
                      there is building database, then terrain plus building database is used to determine whether the
                      path is LoS or not. Otherwise, the Combined LoS/NLoS model is used. By the same logic, if the{' '}
                      <strong>Building Data Source</strong> is None, the Combined LoS/NLoS model is always used.{' '}
                    </p>
                  </>
                }
              >
                <OutlinedQuestionCircleIcon />
              </Tooltip>
              <FormSelect
                value={model.winner2LOSOption}
                onChange={this.setLosOption}
                id="propogation-model-win-los-option"
                name="propogation-model-win-los-option"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="UNKNOWN" value="UNKNOWN" label="Combined LoS/NLoS" />
                <FormSelectOption key="FORCE_LOS" value="FORCE_LOS" label="LoS" />
                <FormSelectOption key="FORCE_NLOS" value="FORCE_NLOS" label="NLoS" />
                <FormSelectOption key="BLDG_DATA_REQ_TX" value="BLDG_DATA_REQ_TX" label="Los/NLoS per building data" />
              </FormSelect>
            </FormGroup>

            {this.renderCustomLosNlosConfidence(model)}
            <FormGroup label="ITM with Tx Clutter Method" fieldId="propogation-model-itm-tx-clutter">
              {' '}
              <Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="propogation-model-itm-tx-clutter-tooltip"
                maxWidth="40.0rem"
                content={
                  <>
                    <p>
                      If <strong>Clutter/No clutter per building</strong> is selected, the path is determined to be LoS
                      or NLoS based on terrain and/or building database (if available). By the same logic, if the{' '}
                      <strong>Building Data Source</strong> is None, blockage is determined based solely on terrain
                      database.
                    </p>
                  </>
                }
              >
                <OutlinedQuestionCircleIcon />
              </Tooltip>
              <FormSelect
                value={model.rlanITMTxClutterMethod}
                onChange={this.setItmClutterMethod}
                id="propogation-model-itm-tx-clutter"
                name="propogation-model-itm-tx-clutter"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="FORCE_TRUE" value="FORCE_TRUE" label="Always add clutter" />
                <FormSelectOption key="FORCE_FALSE" value="FORCE_FALSE" label="Never add clutter" />
                <FormSelectOption key="BLDG_DATA" value="BLDG_DATA" label="Clutter/No clutter per building data" />
              </FormSelect>
            </FormGroup>
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
              <InputGroup>
                <TextInput
                  value={model.p2108Confidence}
                  type="number"
                  onChange={this.setP2108Confidence}
                  id="propogation-model-p2108-confidence"
                  name="propogation-model-p2108-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>

            {model.rlanITMTxClutterMethod === 'BLDG_DATA' || model.winner2LOSOption === 'BLDG_DATA_REQ_TX' ? (
              <>
                <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
                  <FormSelect
                    value={model.buildingSource}
                    onChange={(v) => this.setBuildingSource(v as BuildingSourceValues)}
                    id="propogation-model-data-source"
                    name="propogation-model-data-source"
                    style={{ textAlign: 'right' }}
                    isValid={
                      model.buildingSource === 'LiDAR' ||
                      model.buildingSource === 'B-Design3D' ||
                      model.buildingSource === 'None'
                    }
                  >
                    <FormSelectOption key="B-Design3D" value="B-Design3D" label="B-Design3D (Manhattan)" />
                    <FormSelectOption key="LiDAR" value="LiDAR" label="LiDAR" />
                    <FormSelectOption key="None" value="None" label="None" />
                  </FormSelect>
                </FormGroup>

                <FormGroup label="Terrain Source" fieldId="terrain-source">
                  {' '}
                  <Tooltip
                    position={TooltipPosition.top}
                    enableFlip={true}
                    className="fs-feeder-loss-tooltip"
                    maxWidth="40.0rem"
                    content={
                      <>
                        <p>
                          The higher terrain height resolution (that goes with the building database) is used instead
                          within the first 1 km (when WinnerII building data is chosen) and greater than 1 km (when ITM
                          with building data is chosen).
                        </p>
                      </>
                    }
                  >
                    <OutlinedQuestionCircleIcon />
                  </Tooltip>
                  <FormSelect
                    value={model.terrainSource}
                    onChange={this.setTerrainSource}
                    id="terrain-source"
                    name="terrain-source"
                    style={{ textAlign: 'right' }}
                  >
                    <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                    <FormSelectOption isDisabled={true} key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
                  </FormSelect>
                </FormGroup>
              </>
            ) : (
              false
            )}
          </>
        );
      case 'ISED DBS-06':
        return (
          <>
            <FormGroup label="WinnerII LoS Option" fieldId="propogation-model-win-los-option">
              {' '}
              <Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="propogation-model-win-los-option-tooltip"
                maxWidth="40.0rem"
                content={
                  <>
                    <p>
                      If <strong>LoS/NLoS per surface data</strong> is selected, and if the RLAN is in a region where
                      there is surface database, then terrain plus surface database is used to determine whether the
                      path is LoS or not. Otherwise, the Combined LoS/NLoS model is used. By the same logic, if the{' '}
                      <strong>Surface Data Source</strong> is None, the Combined LoS/NLoS model is always used.{' '}
                    </p>
                  </>
                }
              >
                <OutlinedQuestionCircleIcon />
              </Tooltip>
              <FormSelect
                value={model.winner2LOSOption}
                onChange={this.setLosOption}
                id="propogation-model-win-los-option"
                name="propogation-model-win-los-option"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="CDSM" value="CDSM" label="Los/NLos per surface data" />
              </FormSelect>
            </FormGroup>

            <FormGroup label="WinnerII Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>

            <FormGroup label="Winner II LOS/NLOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceLOS}
                  type="number"
                  onChange={(v) => {
                    this.setWin2ConfidenceLOS(v);
                    this.setWinConfidenceNLOS(v);
                  }}
                  id="propogation-model-win-los-nlos-confidence"
                  name="propogation-model-win-los-nlos-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>

            <FormGroup label="ITM with Tx Clutter Method" fieldId="propogation-model-itm-tx-clutter">
              {' '}
              <Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="propogation-model-itm-tx-clutter-tooltip"
                maxWidth="40.0rem"
                content={
                  <>
                    <p>
                      If <strong>Clutter/No clutter per building</strong> is selected, the path is determined to be LoS
                      or NLoS based on terrain and/or building database (if available). By the same logic, if the{' '}
                      <strong>Building Data Source</strong> is None, blockage is determined based solely on terrain
                      database.
                    </p>
                  </>
                }
              >
                <OutlinedQuestionCircleIcon />
              </Tooltip>
              <FormSelect
                value={model.rlanITMTxClutterMethod}
                onChange={this.setItmClutterMethod}
                id="propogation-model-itm-tx-clutter"
                name="propogation-model-itm-tx-clutter"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="FORCE_TRUE" value="FORCE_TRUE" label="Always add clutter" />
                {/* <FormSelectOption key="FORCE_FALSE" value="FORCE_FALSE" label="Never add clutter" />
                            <FormSelectOption key="BLDG_DATA" value="BLDG_DATA" label="Clutter/No clutter per building data" /> */}
              </FormSelect>
            </FormGroup>
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
              <InputGroup>
                <TextInput
                  value={model.p2108Confidence}
                  type="number"
                  onChange={this.setP2108Confidence}
                  id="propogation-model-p2108-confidence"
                  name="propogation-model-p2108-confidence"
                  style={{
                    textAlign: 'right',
                  }}
                  isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>

            <FormGroup label="Surface Data Source" fieldId="surface-source">
              {/* {" "}<Tooltip
                            position={TooltipPosition.top}
                            enableFlip={true}
                            className="fs-feeder-loss-tooltip"
                            maxWidth="40.0rem"
                            content={
                                <>
                                    <p>The higher terrain height resolution (that goes with the building database)
                                        is used instead within the first 1 km (when WinnerII building data is chosen)
                                        and greater than 1 km (when ITM with building data is chosen).</p>
                                </>
                            }
                        >
                            <OutlinedQuestionCircleIcon />
                        </Tooltip> */}
              <FormSelect
                value={model.surfaceDataSource}
                onChange={this.setSurfaceDataSource}
                id="surface-source"
                name="surface-source"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="CDSM" value="Canada DSM (2000)" label="Canada DSM (2000)" />
                <FormSelectOption key="None" value="None" label="None" />
              </FormSelect>
            </FormGroup>

            <FormGroup label="Terrain Source" fieldId="terrain-source">
              {' '}
              <Tooltip
                position={TooltipPosition.top}
                enableFlip={true}
                className="fs-feeder-loss-tooltip"
                maxWidth="40.0rem"
                content={
                  <>
                    <p>
                      The higher terrain height resolution (that goes with the building database) is used instead within
                      the first 1 km (when WinnerII building data is chosen) and greater than 1 km (when ITM with
                      building data is chosen).
                    </p>
                  </>
                }
              >
                <OutlinedQuestionCircleIcon />
              </Tooltip>
              <FormSelect
                value={model.terrainSource}
                onChange={this.setTerrainSource}
                id="terrain-source"
                name="terrain-source"
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption key="3DEP (30m)" value="3DEP (30m)" label="3DEP (30m)" />
                <FormSelectOption isDisabled={true} key="SRTM (90m)" value="SRTM (90m)" label="SRTM (90m)" />
              </FormSelect>
            </FormGroup>
          </>
        );
      case 'Brazilian Propagation Model':
        return (
          <>
            <FormGroup label="Winner II Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            {model.buildingSource === 'LiDAR' ? (
              <>
                <FormGroup label="Winner II LOS/NLOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS_NLOS(v);
                      }}
                      id="propogation-model-win-los-nlos-confidence"
                      name="propogation-model-win-los-nlos-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            {model.buildingSource === 'None' ? (
              <>
                <FormGroup label="Winner II LOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS(v);
                      }}
                      id="propogation-model-win-los-confidence"
                      name="propogation-model-win-los-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
              <InputGroup>
                <TextInput
                  value={model.p2108Confidence}
                  type="number"
                  onChange={this.setP2108Confidence}
                  id="propogation-model-p2108-confidence"
                  name="propogation-model-p2108-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
              <FormSelect
                value={model.buildingSource}
                onChange={(v) => this.setBuildingSource(v as BuildingSourceValues)}
                id="propogation-model-data-source"
                name="propogation-model-data-source"
                style={{ textAlign: 'right' }}
                isValid={model.buildingSource === 'None'}
              >
                <FormSelectOption key="None" value="None" label="None" />
              </FormSelect>
            </FormGroup>
            {model.buildingSource === 'None' ? (
              <FormGroup label="Terrain Source" fieldId="terrain-source">
                <FormSelect
                  value={model.terrainSource}
                  onChange={this.setTerrainSource}
                  id="terrain-source"
                  name="terrain-source"
                  style={{ textAlign: 'right' }}
                  isValid={model.terrainSource === 'SRTM (30m)'}
                >
                  <FormSelectOption key="SRTM (30m)" value="SRTM (30m)" label="SRTM (30m)" />
                </FormSelect>
              </FormGroup>
            ) : (
              false
            )}
          </>
        );
      case 'Ofcom Propagation Model':
        return (
          <>
            <FormGroup label="Winner II Combined Confidence" fieldId="propogation-model-win-confidence">
              <InputGroup>
                <TextInput
                  value={model.win2ConfidenceCombined}
                  type="number"
                  onChange={this.setWin2ConfidenceCombined}
                  id="propogation-model-win-confidence"
                  name="propogation-model-win-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.win2ConfidenceCombined >= 0 && model.win2ConfidenceCombined <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            {model.buildingSource === 'LiDAR' ? (
              <>
                <FormGroup label="Winner II LOS/NLOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS_NLOS(v);
                      }}
                      id="propogation-model-win-los-nlos-confidence"
                      name="propogation-model-win-los-nlos-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            {model.buildingSource === 'None' ? (
              <>
                <FormGroup label="Winner II LOS Confidence" fieldId="propogation-model-win-los-nlos-confidence">
                  <InputGroup>
                    <TextInput
                      value={model.win2ConfidenceLOS}
                      type="number"
                      onChange={(v) => {
                        this.setWin2ConfidenceLOS(v);
                      }}
                      id="propogation-model-win-los-confidence"
                      name="propogation-model-win-los-confidence"
                      style={{ textAlign: 'right' }}
                      isValid={model.win2ConfidenceLOS >= 0 && model.win2ConfidenceLOS <= 100}
                    />
                    <InputGroupText>%</InputGroupText>
                  </InputGroup>
                </FormGroup>
              </>
            ) : (
              <></>
            )}
            <FormGroup label="ITM Confidence" fieldId="propogation-model-itm-confidence">
              <InputGroup>
                <TextInput
                  value={model.itmConfidence}
                  type="number"
                  onChange={this.setItmConfidence}
                  id="propogation-model-itm-confidence"
                  name="propogation-model-itm-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmConfidence >= 0 && model.itmConfidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="ITM Reliability" fieldId="propogation-model-itm-reliability">
              <InputGroup>
                <TextInput
                  value={model.itmReliability}
                  type="number"
                  onChange={this.setItmReliability}
                  id="propogation-model-itm-reliability"
                  name="propogation-model-itm-reliability"
                  style={{ textAlign: 'right' }}
                  isValid={model.itmReliability >= 0 && model.itmReliability <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="P.2108 Percentage of Locations" fieldId="propogation-model-p2108-confidence">
              <InputGroup>
                <TextInput
                  value={model.p2108Confidence}
                  type="number"
                  onChange={this.setP2108Confidence}
                  id="propogation-model-p2108-confidence"
                  name="propogation-model-p2108-confidence"
                  style={{ textAlign: 'right' }}
                  isValid={model.p2108Confidence >= 0 && model.p2108Confidence <= 100}
                />
                <InputGroupText>%</InputGroupText>
              </InputGroup>
            </FormGroup>
            <FormGroup label="Building Data Source" fieldId="propogation-model-data-source">
              <FormSelect
                value={model.buildingSource}
                onChange={(v) => this.setBuildingSource(v as BuildingSourceValues)}
                id="propogation-model-data-source"
                name="propogation-model-data-source"
                style={{ textAlign: 'right' }}
                isValid={model.buildingSource === 'None'}
              >
                <FormSelectOption key="None" value="None" label="None" />
              </FormSelect>
            </FormGroup>
            {model.buildingSource === 'None' ? (
              <FormGroup label="Terrain Source" fieldId="terrain-source">
                <FormSelect
                  value={model.terrainSource}
                  onChange={this.setTerrainSource}
                  id="terrain-source"
                  name="terrain-source"
                  style={{ textAlign: 'right' }}
                  isValid={model.terrainSource === 'SRTM (30m)'}
                >
                  <FormSelectOption key="SRTM (30m)" value="SRTM (30m)" label="SRTM (30m)" />
                </FormSelect>
              </FormGroup>
            ) : (
              false
            )}
          </>
        );

      default:
        throw 'Invalid propogation model';
    }
  };

  render = () => (
    <FormGroup label="Propagation Model" fieldId="horizontal-form-propogation-model">
      {' '}
      <Tooltip
        position={TooltipPosition.top}
        enableFlip={true}
        className="prop-model-tooltip"
        maxWidth="40.0rem"
        content={
          <>
            <p>For "ITM with no building data," the following model is used: </p>
            <p>Urban/Suburban RLAN:</p>
            <ul>
              <li>- Distance from FS &lt; 1 km: Winner II</li>
              <li>- Distance from FS &gt; 1 km: ITM + P.2108 Clutter</li>
            </ul>
            <p>Rural/Barren RLAN: ITM + P.456 Clutter</p>
            <br />
            <p>For "FCC 6 GHz Report &amp; Order", the following model is used:</p>
            <ul>
              <li>- Path loss &lt; FSPL is clamped to FSPL</li>
              <li>- Distance &lt; 30m: FSPL</li>
              <li>- 30m &lt;= Distance &lt;= 50m: WinnerII LOS Urban/Suburban/Rural</li>
              <li>
                - 50m &lt; Distance &lt; 1km: Combined WinnerII (in absence of building database); WinnerII LOS or NLOS
                (in presence of building database)
              </li>
              <li>
                - Distance &gt; 1km: ITM + [P.2108 Clutter (Urban/Suburban) or P.452 Clutter (Rural) (in absence of
                building database)]; in presence of building database, if no obstruction is found in the path, no
                clutter is applied; otherwise, clutter is applied per above.
              </li>
              <li>
                - For the terrain source, the highest resolution selected terrain source is used (1m LiDAR -&gt; 30m
                3DEP -&gt; 90m SRTM -&gt; 1km Globe)
              </li>
            </ul>
          </>
        }
      >
        <OutlinedQuestionCircleIcon />
      </Tooltip>
      <FormSelect
        value={this.props.data.kind}
        onChange={(x) => this.setKind(x)}
        id="horzontal-form-propogation-model"
        name="horizontal-form-propogation-model"
        isValid={this.props.data.kind !== undefined}
        style={{ textAlign: 'right' }}
      >
        <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select Propogation Model" />
        {this.getModelOptionsByRegion(this.props.region).map((option) => (
          <FormSelectOption isDisabled={option === 'Ray Tracing'} key={option} value={option} label={option} />
        ))}
      </FormSelect>
      {this.getExpansion()}
    </FormGroup>
  );
}
