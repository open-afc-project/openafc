import * as React from "react";
import { Progress, ProgressMeasureLocation } from "@patternfly/react-core";

/**
 * AnalysisProgress.tsx: Progress bar with updates for heat map task
 * author: Sam Smucny
 */

/**
 * Progress bar properties
 */
interface ProgressProps {
    percent: number,
    message: string,
}

/**
 * Progress bar
 * @param props `ProgressProps` 
 */
export const AnalysisProgress: React.FunctionComponent<ProgressProps> = (props: ProgressProps) => (
    <Progress value={props.percent} title="Progress" measureLocation={ProgressMeasureLocation.top} min={0} max={100} label={props.message} valueText={props.message} />
)