import * as React from 'react';
import { Card, CardHeader, CardBody } from '@patternfly/react-core';
import { HeatMapAnalysisType } from '../Lib/RatApiTypes';

/**
 * ColorLegend.tsx: Graphical element that shows colors displayed on heatmap
 * author: Sam Smucny
 */

/**
 * Calculates the color to be used for a given tile based on its gain
 * @param val value to find color for
 * @param threshold threshold to measure `val` against
 * @returns the color of the tile as a CSS color
 */
export const getColor = (val: number, threshold: number) =>
  val < threshold - 20
    ? 'LightGray'
    : val < threshold
      ? 'Gray'
      : val < threshold + 3
        ? 'Blue'
        : val < threshold + 6
          ? 'DarkBlue'
          : val < threshold + 9
            ? 'Green'
            : val < threshold + 12
              ? 'DarkGreen'
              : val < threshold + 15
                ? 'Yellow'
                : val < threshold + 18
                  ? 'Orange'
                  : val < threshold + 21
                    ? 'Red'
                    : 'Maroon';

/**
 * list of color ranges
 * @param threshold threshold value to shift colors by
 */
const colorValues = (threshold: number) => [
  { label: '< ' + (threshold - 20), color: getColor(threshold - 30, threshold) },
  { label: '[' + (threshold - 20) + ', ' + threshold + ')', color: getColor(threshold - 10, threshold) },
  { label: '[' + threshold + ', ' + (threshold + 3) + ')', color: getColor(threshold + 1, threshold) },
  { label: '[' + (threshold + 3) + ', ' + (threshold + 6) + ')', color: getColor(threshold + 4, threshold) },
  { label: '[' + (threshold + 6) + ', ' + (threshold + 9) + ')', color: getColor(threshold + 7, threshold) },
  { label: '[' + (threshold + 9) + ', ' + (threshold + 12) + ')', color: getColor(threshold + 10, threshold) },
  { label: '[' + (threshold + 12) + ', ' + (threshold + 15) + ')', color: getColor(threshold + 13, threshold) },
  { label: '[' + (threshold + 15) + ', ' + (threshold + 18) + ')', color: getColor(threshold + 16, threshold) },
  { label: '[' + (threshold + 18) + ', ' + (threshold + 21) + ')', color: getColor(threshold + 19, threshold) },
  { label: '>= ' + (threshold + 21), color: getColor(threshold + 22, threshold) },
];

/**
 * Legend which shows gain values accociated with different colors on the heat map
 * @param threshold relative value to shift color values by
 */
export const ColorLegend: React.FunctionComponent<{ threshold: number; analysisType: HeatMapAnalysisType }> = (
  props,
) => (
  <Card>
    <CardHeader>
      {props.analysisType === HeatMapAnalysisType.ItoN ? <>I/N Legend (dB)</> : <>EIRP Legend (dBm)</>}
    </CardHeader>
    <CardBody>
      <div key={100} style={{ height: '30px', width: '600px', backgroundColor: 'Black' }}>
        <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
          Denied region
        </div>
      </div>
      <div key={101} style={{ height: '30px', width: '600px', backgroundColor: 'White' }}>
        <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
          No restriction
        </div>
      </div>
      {props.analysisType === HeatMapAnalysisType.ItoN &&
        colorValues(props.threshold || -6).map((val, i) => (
          <div key={i} style={{ height: '30px', width: '600px', backgroundColor: val.color }}>
            <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
              {val.label}
            </div>
          </div>
        ))}
      {props.analysisType === HeatMapAnalysisType.EIRP && (
        <>
          <div key={200} style={{ height: '30px', width: '600px', backgroundColor: 'Green' }}>
            <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
              maxEIRP is met
            </div>
          </div>
          <div key={201} style={{ height: '30px', width: '600px', backgroundColor: 'Yellow' }}>
            <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
              minEIRP &lt;= EIRP &lt; maxEIRP
            </div>
          </div>
          <div key={202} style={{ height: '30px', width: '600px', backgroundColor: 'Red' }}>
            <div style={{ height: '30px', width: '300px', backgroundColor: 'White', borderRightStyle: 'solid' }}>
              EIRP &lt; minEIRP
            </div>
          </div>
        </>
      )}
    </CardBody>
  </Card>
);
