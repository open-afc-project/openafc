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
  TextArea,
} from '@patternfly/react-core';
import { OutlinedQuestionCircleIcon } from '@patternfly/react-icons';
import { APUncertainty } from '../Lib/RatApiTypes';

/**
 * APUncertaintyForm.tsx: sub form of afc config form
 */

/**
 * Sub for for propogation model
 */
export class APUncertaintyForm extends React.PureComponent<{
  data: APUncertainty;
  onChange: (x: APUncertainty) => void;
}> {
  private setPointsPerDegree = (s: string) =>
    // @ts-ignore
    this.props.onChange({ points_per_degree: Number(s), height: this.props.data.height });

  private setHeight = (s: string) =>
    // @ts-ignore
    this.props.onChange({ points_per_degree: this.props.data.points_per_degree, height: Number(s) });
  /*
    
    */

  render = () => (
    <>
      <FormGroup label="AP Uncertainty Region Scanning Resolution" fieldId="uncertainty-form-AP-Uncertainty">
        {' '}
        <Tooltip
          position={TooltipPosition.top}
          enableFlip={true}
          className="ap-uncertainty-res-tooltip"
          maxWidth="40.0rem"
          content={
            <>
              <p>Orientation of the AP uncertainty region is defined as follows:</p>
              <ul>
                <li>
                  - The direction of "UP" is defined by a 3D vector drawn from the center of the earth towards the AP
                  position.
                </li>
                <li>
                  - The "Horizontal plane" is then defined as the plane passing through the AP position that is
                  orthogonal to "UP".
                </li>
                <li>
                  - Scan points are placed on a rectangular X-Y grid in the "Horizontal plane" where the direction of Y
                  is North and the direction of X is east.
                </li>
                <li>
                  - The spacing in the Horizonal Plane is specified in points per degree. For example, 3600 points per
                  degree corresponds to 1arcsecond which is 30m at the equator.
                </li>
                <li>- The "Height" is the scanning resolution in the direction of "UP".</li>
              </ul>
            </>
          }
        >
          <OutlinedQuestionCircleIcon />
        </Tooltip>
        <InputGroup>
          <TextInput
            id="horizontal-label"
            name="horizontal-label"
            isReadOnly={true}
            value="Horizontal Plane (points per degree)"
            style={{ textAlign: 'left', minWidth: '50%' }}
          />

          <TextInput
            type="number"
            id="horizontal-value-in"
            name="horizontal-value-in"
            isValid={this.props.data.points_per_degree > 0}
            value={this.props.data.points_per_degree}
            onChange={this.setPointsPerDegree}
            style={{ textAlign: 'right' }}
          />
          <InputGroupText>ppd</InputGroupText>
        </InputGroup>
        <InputGroup>
          <TextInput
            id="ap-height-label"
            name="ap-height-label"
            isReadOnly={true}
            value="Height"
            style={{ textAlign: 'left', minWidth: '50%' }}
          />
          <TextInput
            type="number"
            id="height-value-out"
            name="height-value-out"
            isValid={this.props.data.height > 0}
            value={this.props.data.height}
            onChange={this.setHeight}
            style={{ textAlign: 'right' }}
          />
          <InputGroupText>m</InputGroupText>
        </InputGroup>
      </FormGroup>
    </>
  );
}
