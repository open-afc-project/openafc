import * as React from "react";
import { Stage, Layer, Text, Shape, Rect, Line } from "react-konva";
import { ChannelData } from "../Lib/RatApiTypes";
import { KonvaEventObject } from "konva/types/Node";

/** 
 * ChannelDisplay.tsx: Draws channel shapes on page and colors/labels them according to props
 * author: Sam Smucny
 */

/**
 * Channel set up data
 *   If changes are made here they need to be made in SpectrumDisplay.tsx as well
 */
const emptyChannels: ChannelData[] = [
    { channelWidth: 20, startFrequency: 5945, channels: Array(59).fill(0).map((_, i) => ({ name: String(1 + 4 * i), color: "grey", maxEIRP: 0 })) },
    { channelWidth: 40, startFrequency: 5945, channels: Array(29).fill(0).map((_, i) => ({ name: String(3 + 8 * i), color: "grey", maxEIRP: 0 })) },
    { channelWidth: 80, startFrequency: 5945, channels: Array(14).fill(0).map((_, i) => ({ name: String(7 + 16 * i), color: "grey", maxEIRP: 0 })) },
    { channelWidth: 160, startFrequency: 5945, channels: Array(7).fill(0).map((_, i) => ({ name: String(15 + 32 * i), color: "grey", maxEIRP: 0 })) },
    { channelWidth: 20, startFrequency: 5925, channels: Array(1).fill(0).map((_, i) => ({ name: String(2 + 4 * i), color: "grey", maxEIRP: 0 })) },
];

interface ChannelProps {
    start: number,
    vertical: number,
    height: number,
    key: number,
    end: number,
    color: string,
    name: string,
    maxEIRP?: number,
    stageSize: { width: number, height: number }
};

/**
 * Fundamental component that is used as building block
 */
class Channel extends React.Component<ChannelProps, { showToolTip: boolean }> {

    constructor(props: ChannelProps) {
        super(props);
        this.state = { showToolTip: false };
    }

    private handleMouseEnter = (evt: KonvaEventObject<MouseEvent>) => {
        this.setState({ showToolTip: true });

    }

    private handleMouseOut = (evt: KonvaEventObject<MouseEvent>) => {
        this.setState({ showToolTip: false });
    }

    render = () =>
        <>
            <Shape
                sceneFunc={(context, shape) => {
                    context.beginPath();
                    context.moveTo(this.props.start, this.props.vertical + this.props.height);
                    context.lineTo(this.props.start + 5, this.props.vertical);
                    context.lineTo(this.props.end - 5, this.props.vertical);
                    context.lineTo(this.props.end, this.props.vertical + this.props.height);
                    context.closePath();
                    context.fillStrokeShape(shape);
                }}
                fill={this.props.color}
                onMouseEnter={this.handleMouseEnter}
                onMouseLeave={this.handleMouseOut}
                stroke="black"
                strokeWidth={2} />
            {this.state.showToolTip ? <>
                <Rect
                    x={Math.min(this.props.start, this.props.stageSize.width - 240)}
                    y={this.props.vertical - (this.props.name ? 40 : 30)}
                    width={230}
                    height={this.props.name ? 35 : 25}
                    fill="#FFFFFF"
                    stroke="#000000" />
                <Text
                    name="tootTipText"
                    fontSize={12}
                    text={(this.props.name ? ("Channel Index: " + this.props.name + "\n") : "") + "Max. EIRP (dBm): " + 
                    ((this.props.color !== "grey" &&  this.props.maxEIRP) ? this.props.maxEIRP : "")}
                    x={Math.min(this.props.start + 5, this.props.stageSize.width - 235)}
                    y={this.props.vertical - (this.props.name ? 30 : 20)} /></>
                : <></>}
        </>
};

interface ChannelArrayProps {
    topLeft: { x: number, y: number },
    height: number,
    channelWidth: number,
    startOffset: number,
    channels: { name: string, color: string, maxEIRP?: number }[],
    stageSize: { width: number, height: number }
}

/**
 * One row of channels
 * @param props `ChannelArrayProps`
 */
const ChannelArray: React.FunctionComponent<ChannelArrayProps> = (props: ChannelArrayProps) => (
    <>{props.channels.map((chan, i) => (
        <Channel
            name={chan.name}
            key={i}
            color={chan.color}
            maxEIRP={chan.maxEIRP}
            start={i * props.channelWidth + props.topLeft.x + props.startOffset}
            height={props.height}
            end={(i + 1) * props.channelWidth + props.topLeft.x + props.startOffset}
            vertical={props.topLeft.y}
            stageSize={props.stageSize} />)
    )}
    </>
);

interface ChannelDisplayProps {
    channels?: ChannelData[],
    topLeft: { x: number, y: number },
    channelHeight: number,
    totalWidth: number
}

/**
 * helper method to determine size of components
 * @returns value to scale drawing by
 */
const calcScaleFactor = (props: ChannelDisplayProps): number => {
    const maxWidth = props.channels!    // will only be called after props.channels !== undefined
        .map(row => row.channelWidth * row.channels.length + (row.startFrequency - startFreq)) // get total size
        .reduce((a, b) => a > b ? a : b); // maximum
    const f = 0.98 * props.totalWidth / maxWidth;
    return f;
}


const startFreq = 5925;
const lines = Array(15).fill(0);

/**
 * top level component that renders multiple rows of channels
 */
const ChannelDisplay: React.FunctionComponent<ChannelDisplayProps> = (props) => {
    if (props.channels === undefined) return <></>;

    const scaleFactor = calcScaleFactor(props);
    return (
        <Stage width={props.totalWidth} height={(props.channelHeight + 10) * props.channels.length + 150}>
            <Layer>
                {
                    lines.map((_, i) =>
                        <Line
                            key={i}
                            x={80 * scaleFactor * i + props.topLeft.x}
                            y={20}
                            points={[
                                0, // realtive start x
                                0, // relative start y
                                0, // end x
                                (10 + props.channelHeight) * props.channels!.length + 50 // end y
                            ]}
                            stroke="#3697d8"
                            strokeWidth={2}
                            dash={[10, 5]}
                        />
                    ).concat([
                        <Line
                            key={-1}
                            x={60 * 20 * scaleFactor + 5}
                            y={20}
                            points={[
                                0, // realtive start x
                                0, // relative start y
                                0, // end x
                                (10 + props.channelHeight) * props.channels!.length + 50 // end y
                            ]}
                            stroke="#3697d8"
                            strokeWidth={2}
                            dash={[10, 5]}
                        />
                    ])
                }
                {
                    lines.map((_, i) =>
                        <Text
                            key={i}
                            text={(startFreq + 80 * i) + " MHz"}
                            rotation={-90}
                            fontSize={14}
                            verticalAlign="top"
                            x={80 * scaleFactor * i}
                            y={140 + (10 + props.channelHeight) * props.channels!.length}
                        />
                    ).concat([
                        <Text
                            key={-1}
                            text="7125 MHz"
                            rotation={-90}
                            fontSize={14}
                            verticalAlign="top"
                            x={60 * 20 * scaleFactor}
                            y={140 + (10 + props.channelHeight) * props.channels!.length}
                        />
                    ])
                }
                {props.channels.map((row, i) => (
                    <ChannelArray
                        key={i}
                        stageSize={{ width: props.totalWidth, height: props.channelHeight + 10 }}
                        topLeft={{ x: props.topLeft.x, y: props.topLeft.y + (props.channelHeight + 10) * i + 40 }}
                        height={props.channelHeight}
                        channelWidth={row.channelWidth * scaleFactor}
                        startOffset={(row.startFrequency - startFreq) * scaleFactor}
                        channels={row.channels} />
                ))}
            </Layer>
        </Stage>);
};

export { ChannelDisplay, emptyChannels };