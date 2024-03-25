import * as React from 'react';
// import ReactTooltip from 'react-tooltip';
import { Location, LinearPolygon, RadialPolygon, Ellipse } from '../Lib/RatAfcTypes';
import {
  GalleryItem,
  FormGroup,
  InputGroup,
  FormSelect,
  FormSelectOption,
  TextInput,
  InputGroupText,
} from '@patternfly/react-core';
import { ifNum } from '../Lib/Utils';
import { PairList } from './PairList';

/** ElevationForm.tsx - Form component to display and create the Elevation object
 *
 * mgelman 2022-01-04
 */

/**
 * possible height types
 */
enum HeightType {
  AGL = 'AGL',
  AMSL = 'AMSL',
}

/**
 * enumerated location types
 */
const heightTypes: string[] = [HeightType.AGL, HeightType.AMSL];

export interface ElevationFormParams {
  elevation: {
    height?: number;
    heightType?: string;
    verticalUncertainty?: number;
  };
  onChange: (val: { height?: number; heightType?: string; verticalUncertainty?: number }) => void;
}

export interface ElevationFormState {
  height?: number;
  heightType?: string;
  verticalUncertainty?: number;
}

export class ElevationForm extends React.PureComponent<ElevationFormParams, ElevationFormState> {
  constructor(props: ElevationFormParams) {
    super(props);
    this.state = {
      height: props.elevation.height,
      heightType: props.elevation.heightType,
      verticalUncertainty: props.elevation.verticalUncertainty,
    };
  }

  private setHeight(n: number) {
    this.props.onChange({
      height: n,
      heightType: this.props.elevation.heightType,
      verticalUncertainty: this.props.elevation.verticalUncertainty,
    });
  }

  private setVerticalUncertainty(n: number) {
    this.setState({ verticalUncertainty: n }, () =>
      this.props.onChange({
        height: this.props.elevation.height,
        heightType: this.props.elevation.heightType,
        verticalUncertainty: n,
      }),
    );
  }

  private setHeightType(s: string) {
    this.setState({ heightType: s }, () =>
      this.props.onChange({
        height: this.props.elevation.height,
        heightType: s,
        verticalUncertainty: this.props.elevation.verticalUncertainty,
      }),
    );
  }

  render() {
    return (
      <>
        <GalleryItem>
          <FormGroup label="Height" fieldId="horizontal-form-height">
            <InputGroup>
              <TextInput
                value={this.props.elevation.height}
                onChange={(x) => this.setHeight(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-height"
                name="horizontal-form-height"
                isValid={this.props.elevation.height >= 0}
                style={{ textAlign: 'right' }}
              />
              <InputGroupText>meters</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Height Type" fieldId="horizontal-form-height-type">
            <InputGroup>
              <FormSelect
                value={this.props.elevation.heightType}
                onChange={(x) => this.setHeightType(x)}
                id="horzontal-form-height-type"
                name="horizontal-form-height-type"
                isValid={heightTypes.includes(this.props.elevation.heightType)}
                style={{ textAlign: 'right' }}
              >
                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a height type" />
                {heightTypes.map((option) => (
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
                value={this.props.elevation.verticalUncertainty}
                onChange={(x) => this.setVerticalUncertainty(Number(x))}
                type="number"
                step="any"
                id="horizontal-form-height-cert"
                name="horizontal-form-height-cert"
                isValid={this.props.elevation.verticalUncertainty >= 0}
                style={{ textAlign: 'right' }}
              />
              <InputGroupText>meters</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
      </>
    );
  }
}
