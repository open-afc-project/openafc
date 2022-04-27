import * as React from "react";
import {
  ScatterChart,
  CartesianGrid,
  XAxis,
  YAxis,
  Tooltip,
  Scatter,
  Legend,
  ResponsiveContainer, ReferenceArea
} from "recharts";
import { Title, CardHeader, CardBody, Card } from "@patternfly/react-core";
import { SpectrumProfile, PAWSResponse } from "../Lib/RatApiTypes";
import { AvailableSpectrumInquiryResponse, AvailableSpectrumInfo } from "@app/Lib/RatAfcTypes";

/**
 * SpectrumDisplay.tsx: display PAWS response spectrums on graphs
 * author: Sam Smucny
 */

/**
 * Enumeration of possible colors
 */
const colors = ["black", "blue", "orange", "green", "red", "purple", "yellow"];

/**
 * Add null points between disjoint spectral envelopes to give displayed line discontinuities
 * also convert Hz to MHz.
 * @param spectra Spectrum profile according to PAWS
 * @returns adjusted spectrum values that properly display on graph
 */
const insertNullPoints = (spectra: SpectrumProfile[][]): { hz: number, dbm?: number | null }[] => 
  spectra.reduce((prev, curr) => prev.concat([{ hz: (prev[prev.length - 1].hz + curr[0].hz) / 2, dbm: -Infinity }], curr))
    .map(point => ({ hz: point.hz / 1e6, dbm: (point.dbm === -Infinity ? undefined : point.dbm) }));

/**
 * Displays spectrum from a PAWS response on a graph to show spectral envelope
 * @param spectrum `PAWSResponse` object to graph
 */
export const SpectrumDisplayPAWS: React.FunctionComponent<{ spectrum?: PAWSResponse, greyOutUnii: boolean }> = (props) => (
  !props.spectrum || props.spectrum.spectrumSpecs.length === 0 ?
    <Card><CardHeader>There is no data to display. Try sending a request above.</CardHeader></Card> :
    <>{props.spectrum.spectrumSpecs.map(spec =>
      spec.spectrumSchedules.map((schedule, sIndex) => (<Card key={sIndex} isHoverable={true}>
      <CardHeader><Title size="md" style={{ textAlign: "center" }}>{spec.rulesetInfo.rulesetId + ": " + schedule.eventTime.startTime + " to " + schedule.eventTime.stopTime}</Title></CardHeader>
      <CardBody><ResponsiveContainer width="100%" height={400}>
        <ScatterChart
          margin={{
            top: 20, right: 20, bottom: 20, left: 20,
          }}
        >
          <CartesianGrid />
          <XAxis 
          label={{ value: "Frequency (MHz)", position: "bottom" }} 
          domain={[5925, 7125]} 
          type="number" 
          dataKey="hz" 
          name="Frequency (MHz)" 
          ticks={Array(15).fill(0).map((_, i) => 5925 + i * 80).concat([7125])}
          />
          <YAxis
            domain={["dataMin", "auto"]}
            type="number"
            dataKey="dbm"
            name="Max. EIRP (dBm)"
            label={{ value: "Max. EIRP (dBm)", angle: -90, position: "insideLeft" }}
          />
          <Tooltip cursor={true} />
          {schedule.spectra.map((spectrum, i) =>
          <Scatter
            key={i}
            name={spectrum.resolutionBwHz / 1e6 + " MHz"}
            data={insertNullPoints(spectrum.profiles)}
            fill={colors[i]}
            line={true} />
            )}
          <Legend verticalAlign="top" />
          {props.greyOutUnii && <ReferenceArea x1={6425} x2={6525} />}
          {props.greyOutUnii && <ReferenceArea x1={6875} x2={7125} />}
        </ScatterChart></ResponsiveContainer></CardBody>
      </Card>)))}</>
);

/**
 * Converts AP-AFC spectrum info into a format that is able to be displayed on the plot
 * @param sections spectrum infos in AP-AFC format
 */
const makeSteps = (sections: AvailableSpectrumInfo[]): { hz: number, dbm?: number}[] => {
  let prev = sections[0];
  let points: { hz: number, dbm?: number}[] = [
    { hz: prev.frequencyRange.lowFrequency, dbm: prev.maxPSD }, 
    { hz: prev.frequencyRange.highFrequency, dbm: prev.maxPSD }, 
  ];
  for (let i = 1; i < sections.length; i++) {
    let curr = sections[i];

    if (curr.frequencyRange.lowFrequency !== prev.frequencyRange.highFrequency) {
      points.push({ hz: (curr.frequencyRange.lowFrequency + prev.frequencyRange.highFrequency) / 2, dbm: undefined });
    }
    points.push(
      { hz: curr.frequencyRange.lowFrequency, dbm: curr.maxPSD },
      { hz: curr.frequencyRange.highFrequency, dbm: curr.maxPSD }
    );

    prev = curr;
  }
  return points;
}

/**
 * Displays spectrum from a AP-AFC request
 * @param spectrum `AvailableSpectrumInquiryResponse` object to graph
 */
export const SpectrumDisplayAFC: React.FunctionComponent<{ spectrum?: AvailableSpectrumInquiryResponse }> = (props) => (
  !props.spectrum || !props.spectrum.availableFrequencyInfo || props.spectrum.availableFrequencyInfo.length === 0 ?
    <Card><CardHeader>There is no spectrum data to display.</CardHeader></Card> :
    <><Card key={1} isHoverable={true}>
      <CardHeader><Title size="md" style={{ textAlign: "center" }}>{"Request " + props.spectrum?.requestId + ": expires at " + (props.spectrum?.availabilityExpireTime || "No expiration")}</Title></CardHeader>
      <CardBody><ResponsiveContainer width="100%" height={400}>
        <ScatterChart
          margin={{
            top: 20, right: 20, bottom: 20, left: 20,
          }}
        >
          <CartesianGrid />
          <XAxis 
          label={{ value: "Frequency (MHz)", position: "bottom" }} 
          domain={[5925, 7125]} 
          type="number" 
          dataKey="hz" 
          name="Frequency (MHz)" 
          ticks={Array(15).fill(0).map((_, i) => 5925 + i * 80).concat([7125])}
          />
          <YAxis
            domain={["dataMin", "auto"]}
            type="number"
            dataKey="dbm"
            name="PSD (dBm/MHz)"
            label={{ value: "PSD (dBm/MHz)", angle: -90, position: "insideLeft" }}
          />
          <Tooltip cursor={true} />
          <Scatter
            key={1}
            name={"PSD"}
            data={makeSteps(props.spectrum.availableFrequencyInfo)}
            fill={colors[0]}
            line={true} />
          <Legend verticalAlign="top" />
        </ScatterChart></ResponsiveContainer></CardBody>
      </Card></>
);