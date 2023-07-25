import * as React from "react";
import {
  LineChart,
  ScatterChart,
  CartesianGrid,
  XAxis,
  YAxis,
  Tooltip,
  Scatter,
  Line,
  Legend,
  ResponsiveContainer, ReferenceArea, TooltipProps
} from "recharts";
import { Title, CardHeader, CardBody, Card, TextArea, } from "@patternfly/react-core";
import { SpectrumProfile, PAWSResponse } from "../Lib/RatApiTypes";
import { AvailableSpectrumInquiryResponse, AvailableSpectrumInfo, AvailableChannelInfo } from "@app/Lib/RatAfcTypes";

import ReactDOMServer from 'react-dom/server';

/**
 * SpectrumDisplay.tsx: display PAWS response spectrums on graphs
 * author: Sam Smucny
 */

/**
 * Enumeration of possible colors
 */
const colors = ["black", "blue", "orange", "green", "red", "purple", "gold"];



/***
 * Things to know to draw the Spectrum line  chart correctly
 *  If changes are made here they need to be made in ChannelDisplay.tsx as well
 */
const channelDescription = [
  { channelWidth: 20, startFrequency: 5945, OperatingClass: 131, OCDisplayname: "131", channels: Array(59).fill(0).map((_, i) => ({ cfi: Number(1 + 4 * i) })) },
  { channelWidth: 40, startFrequency: 5945, OperatingClass: 132, OCDisplayname: "132", channels: Array(29).fill(0).map((_, i) => ({ cfi: Number(3 + 8 * i) })) },
  { channelWidth: 80, startFrequency: 5945, OperatingClass: 133, OCDisplayname: "133", channels: Array(14).fill(0).map((_, i) => ({ cfi: Number(7 + 16 * i) })) },
  { channelWidth: 160, startFrequency: 5945, OperatingClass: 134, OCDisplayname: "134", channels: Array(7).fill(0).map((_, i) => ({ cfi: Number(15 + 32 * i) })) },
  { channelWidth: 20, startFrequency: 5925, OperatingClass: 136, OCDisplayname: "136", channels: Array(1).fill(0).map((_, i) => ({ cfi: Number(2 + 4 * i) })) },
  { channelWidth: 320, startFrequency: 5945, OperatingClass: 137, OCDisplayname: "137-1", channels: Array(3).fill(0).map((_, i) => ({ cfi: Number(31 + 64 * i) })) },
  { channelWidth: 320, startFrequency: 6105, OperatingClass: 137, OCDisplayname: "137-2", channels: Array(3).fill(0).map((_, i) => ({ cfi: Number(63 + 64 * i) })) },
];

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
const makeSteps = (sections: AvailableSpectrumInfo[]): { hz: number, dbm?: number }[] => {
  let prev = sections[0];
  let points: { hz: number, dbm?: number }[] = [
    { hz: prev.frequencyRange.lowFrequency, dbm: prev.maxPsd },
    { hz: prev.frequencyRange.highFrequency, dbm: prev.maxPsd },
  ];
  for (let i = 1; i < sections.length; i++) {
    let curr = sections[i];

    if (curr.frequencyRange.lowFrequency !== prev.frequencyRange.highFrequency) {
      points.push({ hz: (curr.frequencyRange.lowFrequency + prev.frequencyRange.highFrequency) / 2, dbm: undefined });
    }
    points.push(
      { hz: curr.frequencyRange.lowFrequency, dbm: curr.maxPsd },
      { hz: curr.frequencyRange.highFrequency, dbm: curr.maxPsd }
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

/**
 * Converts the channel data to frequencies
 * @param spectra Channel info
 * @returns Spectrum profile for plotting
 */
const convertOpClassData = (spectra: AvailableChannelInfo): { hz: number, dbm: number | undefined, cfi: number }[] => {
  var desc = channelDescription.find(x => x.OperatingClass == spectra.globalOperatingClass);
  if (!!desc) {
    return desc.channels.map((e, i) => {
      let cfiIndex = spectra.channelCfi.indexOf(e.cfi);
      return [{
        hz: desc!.startFrequency + (desc!.channelWidth * i),
        dbm: cfiIndex >= 0 ? spectra.maxEirp[cfiIndex] : undefined,
        cfi: e.cfi
      }, {
        hz: desc!.startFrequency + (desc!.channelWidth * i) + desc!.channelWidth,
        dbm: cfiIndex >= 0 ? spectra.maxEirp[cfiIndex] : undefined,
        cfi: e.cfi
      }]
    }).reduce((r, x) => r.concat(x));
  }
  return [];
}

// converts the data for 137 depending on which line it gets drawn on
const convert137OpClassData = (spectra: AvailableChannelInfo, channelIndex:number): { hz: number, dbm: number | undefined, cfi: number }[] => {
  var desc = channelDescription.find(x => x.OperatingClass == spectra.globalOperatingClass && x.OCDisplayname.endsWith(String(channelIndex)));
  if (!!desc) {
    return desc.channels.map((e, i) => {
      let cfiIndex = spectra.channelCfi.indexOf(e.cfi);
      return [{
        hz: desc!.startFrequency + (desc!.channelWidth * i),
        dbm: cfiIndex >= 0 ? spectra.maxEirp[cfiIndex] : undefined,
        cfi: e.cfi
      }, {
        hz: desc!.startFrequency + (desc!.channelWidth * i) + desc!.channelWidth,
        dbm: cfiIndex >= 0 ? spectra.maxEirp[cfiIndex] : undefined,
        cfi: e.cfi
      }]
    }).reduce((r, x) => r.concat(x));
  }
  return [];
}

// Op Class 137 is two lines in the graph, but sent as a single set of channel info.  This breaks it up 
// into the two lines
const split137IntoSubchannels = (spectra: AvailableChannelInfo) => {

  var chan1 = channelDescription.find(x => x.OCDisplayname == "137-1")?.channels.map(x => x.cfi);

  var spectra1: AvailableChannelInfo = { globalOperatingClass: 137, channelCfi: [], maxEirp: [] }
  var spectra2: AvailableChannelInfo = { globalOperatingClass: 137, channelCfi: [], maxEirp: [] }

  for (let index = 0; index < spectra.channelCfi.length; index++) {
    if (chan1?.includes(spectra.channelCfi[index])) {
      spectra1.channelCfi.push(spectra.channelCfi[index]);
      spectra1.maxEirp.push(spectra.maxEirp[index])
    } else {
      spectra2.channelCfi.push(spectra.channelCfi[index]);
      spectra2.maxEirp.push(spectra.maxEirp[index])
    }
  }
  return [spectra1, spectra2];

}

// Current operating class for custom tooltip
var tooltipLabel: string = "";

/** 
 * Custom tooltip to display Operating class, channel Cfi, frequency and EIRP
*/
const CustomTooltip = (pr: TooltipProps) => {
  if (pr.active && pr.payload && pr.payload.length) {
    var res = <> <div className="custom-tooltip" >
      {pr.payload.map(v => { return (<p>OC {tooltipLabel} Ch {v.payload.cfi} {v.name}:{v.value}</p>) })}
    </div>
    </>;
    return (res);
  }

  return null;
};

const generateScatterPlot = (data: AvailableChannelInfo[]) => {
  return data.map((spectrum, i) => {
    if (spectrum.globalOperatingClass == 137) {

      return split137IntoSubchannels(spectrum).map((s137, j) => {
       return <Scatter
          key={i + j}
          dataKey='dbm'
          name={String(spectrum.globalOperatingClass) + "-" + (j + 1)}
          data={convert137OpClassData(s137,j+1)}
          fill={colors[i + j]} //This will only work if 137 is the last class mapped
          stroke={colors[i + j]}
          line={true}
          onMouseOver={() => tooltipLabel = String(spectrum.globalOperatingClass) + "-" + (j + 1)}
        />
      })

    } else return <Scatter
      key={i}
      dataKey='dbm'
      name={String(spectrum.globalOperatingClass)}
      data={convertOpClassData(spectrum)}
      fill={colors[i]}
      stroke={colors[i]}
      line={true}
      onMouseOver={() => tooltipLabel = String(spectrum.globalOperatingClass)}
    />
  }).flat()
}



/**
 * Displays the channel information from a virtual AP request as a line chart
 * @param props 
 * @returns 
 */
export const SpectrumDisplayLineAFC: React.FunctionComponent<{ spectrum?: AvailableSpectrumInquiryResponse }> = (props) => (
  !props.spectrum || !props.spectrum.availableChannelInfo || props.spectrum.availableChannelInfo.length === 0 ?
    <Card><CardHeader>There is no spectrum data to display.</CardHeader></Card> :
    <Card isHoverable={true}>
      <CardHeader><Title size="md" style={{ textAlign: "center" }}>{"Request " + props.spectrum.requestId + ": expires at " + (props.spectrum.availabilityExpireTime || "No expiration")}</Title></CardHeader>
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
          <Tooltip cursor={true} content={<CustomTooltip />} active={true} />
          {generateScatterPlot(props.spectrum.availableChannelInfo)}
          <Legend verticalAlign="top" />
        </ScatterChart>
      </ResponsiveContainer>
        {/* <TextArea readOnly={true} value={JSON.stringify(props.spectrum.availableChannelInfo.map((spectrum, i) => convertOpClassData(spectrum)))}></TextArea> */}
      </CardBody>
    </Card>
);

