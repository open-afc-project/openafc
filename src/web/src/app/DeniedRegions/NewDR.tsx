import * as React from "react";
import { AccessPointModel, DeniedRegion, ExclusionCircle, ExclusionHorizon, ExclusionRect, ExclusionTwoRect, RatResponse } from "../Lib/RatApiTypes";
import { Gallery, GalleryItem, FormGroup, TextInput, Button, Alert, AlertActionCloseButton, FormSelect, FormSelectOption, InputGroup } from "@patternfly/react-core";
import { PlusCircleIcon } from "@patternfly/react-icons";
import { UserModel } from "../Lib/RatApiTypes";
import { hasRole } from "../Lib/User";
import { BlankDeniedRegion } from "../Lib/Admin";
import { number } from "prop-types";

/**
 * NewDR.tsx: Form for creating a new access point
 * author: Sam Smucny
 */

const zoneTypes = [
    "Circle", "One Rectangle", "Two Rectangles", "Horizon Distance"
]

/**
 * Interface definition of `NewDR` properties
 */
interface NewDRProps {
    currentRegionStr: string,
    isEdit: boolean,
    drToEdit: DeniedRegion | null,
    onAdd: (dr: DeniedRegion) => void,
    onEdit: (dr: DeniedRegion, prevName: string, prevZoneType: string) => void
}

interface NewDRState {
    isEdit: boolean,
    prevName: string,
    prevZoneType: string,
    needsSave: boolean

    regionStr?: string,
    name?: string,
    startFreq?: number,
    endFreq?: number,
    zoneType?: "Circle" | "One Rectangle" | "Two Rectangles" | "Horizon Distance",
    circleLat?: number,
    circleLong?: number,
    radiusHeight?: number,
    rect1topLat?: number,
    rect1leftLong?: number,
    rect1bottomLat?: number,
    rect1rightLong?: number,
    rect2topLat?: number,
    rect2leftLong?: number,
    rect2bottomLat?: number,
    rect2rightLong?: number,
}

/**
 * Component with form for user to register a new access point.
 * 
 */
export class NewDR extends React.Component<NewDRProps, NewDRState> {
    constructor(props: NewDRProps) {
        super(props);
        if (props.drToEdit && props.isEdit) {
            this.state = {
                isEdit: true,
                prevName: props.drToEdit.name,
                prevZoneType: props.drToEdit.zoneType,
                needsSave: false,
                regionStr: props.currentRegionStr,
                name: props.drToEdit.name,
                startFreq: props.drToEdit.startFreq,
                endFreq: props.drToEdit.endFreq,
                zoneType: props.drToEdit.zoneType,
                circleLat: undefined,
                circleLong: undefined,
                radiusHeight: undefined,
                rect1topLat: undefined,
                rect1leftLong: undefined,
                rect1bottomLat: undefined,
                rect1rightLong: undefined,
                rect2topLat: undefined,
                rect2leftLong: undefined,
                rect2bottomLat: undefined,
                rect2rightLong: undefined,
            };
            switch (props.drToEdit.zoneType) {
                case "Circle":
                    let circ = props.drToEdit.exclusionZone as ExclusionCircle;
                    this.setState({ circleLat: circ.latitude, circleLong: circ.longitude, radiusHeight: circ.radiusKm });
                    break;
                case "One Rectangle":
                    let rect = props.drToEdit.exclusionZone as ExclusionRect;
                    this.setState({ rect1topLat: rect.topLat, rect1leftLong: rect.leftLong, rect1bottomLat: rect.bottomLat, rect1rightLong: rect.rightLong });
                    break;
                case "Two Rectangles":
                    let rect2 = props.drToEdit.exclusionZone as ExclusionTwoRect;
                    this.setState({
                        rect1topLat: rect2.rectangleOne.topLat, rect1leftLong: rect2.rectangleOne.leftLong,
                        rect1bottomLat: rect2.rectangleOne.bottomLat, rect1rightLong: rect2.rectangleOne.rightLong,
                        rect2topLat: rect2.rectangleTwo.topLat, rect2leftLong: rect2.rectangleTwo.leftLong,
                        rect2bottomLat: rect2.rectangleTwo.bottomLat, rect2rightLong: rect2.rectangleTwo.rightLong
                    });
                    break;
                case "Horizon Distance":
                    let horz = props.drToEdit.exclusionZone as ExclusionHorizon;
                    this.setState({ circleLat: horz.latitude, circleLong: horz.longitude, radiusHeight: horz.aglHeightM });
                    break;
                default:
                    break;
            }

        } else {
            this.state = {
                isEdit: false,
                regionStr: props.currentRegionStr,
                prevName: "Placeholder",
                prevZoneType: "Circle",
                needsSave: false,
                name: undefined,
                startFreq: undefined,
                endFreq: undefined,
                zoneType: "Circle",
                circleLat: undefined,
                circleLong: undefined,
                radiusHeight: undefined,
                rect1topLat: undefined,
                rect1leftLong: undefined,
                rect1bottomLat: undefined,
                rect1rightLong: undefined,
                rect2topLat: undefined,
                rect2leftLong: undefined,
                rect2bottomLat: undefined,
                rect2rightLong: undefined,
            };
        }
    }

    private stateToDeniedRegion(): DeniedRegion {

        let ez;
        switch (this.state.zoneType) {
            case "Circle":
                let rc: ExclusionCircle = {
                    latitude: this.state.circleLat!,
                    longitude: this.state.circleLong!,
                    radiusKm: this.state.radiusHeight!
                };
                ez = rc;
                break;
            case "One Rectangle":
                let er: ExclusionRect = {
                    topLat: this.state.rect1topLat!,
                    leftLong: this.state.rect1leftLong!,
                    bottomLat: this.state.rect1bottomLat!,
                    rightLong: this.state.rect1rightLong!
                }
                ez = er;
                break;
            case "Two Rectangles":
                let er2: ExclusionTwoRect = {
                    rectangleOne: {
                        topLat: this.state.rect1topLat!,
                        leftLong: this.state.rect1leftLong!,
                        bottomLat: this.state.rect1bottomLat!,
                        rightLong: this.state.rect1rightLong!
                    },
                    rectangleTwo: {
                        topLat: this.state.rect2topLat!,
                        leftLong: this.state.rect2leftLong!,
                        bottomLat: this.state.rect2bottomLat!,
                        rightLong: this.state.rect2rightLong!
                    }
                };
                ez = er2;
                break;
            case "Horizon Distance":
                let eh: ExclusionHorizon = {
                    latitude: this.state.circleLat!,
                    longitude: this.state.circleLong!,
                    aglHeightM: this.state.radiusHeight!
                };
                ez = eh;
                break;
            default:
                break;
        }

        let dr: DeniedRegion = {
            regionStr: this.state.regionStr!,
            name: this.state.name!,
            endFreq: this.state.endFreq!,
            startFreq: this.state.startFreq!,
            zoneType: this.state.zoneType!,
            exclusionZone: ez
        }

        return dr;
    }

    private submit() {
        let newDr = this.stateToDeniedRegion();
        if (this.props.isEdit) {
            this.props.onEdit(newDr, this.state.prevName, this.state.prevZoneType);
        } else {
            this.props.onAdd(newDr)
        }

    }

    private setName(n: string) {
        if (n != this.state.name) {
            this.setState({ name: n, needsSave: true });
        }
    }

    private setStartFreq(n: number) {
        if (n != this.state.startFreq) {
            this.setState({ startFreq: n, needsSave: true });
        }
    }
    private setEndFreq(n: number) {
        if (n != this.state.endFreq) {
            this.setState({ endFreq: n, needsSave: true });
        }
    }

    private setZoneType(n: string) {
        if (n != this.state.zoneType && zoneTypes.includes(n)) {
            this.setState({ zoneType: n, needsSave: true });
        }
    }

    private setCircleLat(n: number) {
        if (n != this.state.circleLat) {
            this.setState({ circleLat: n, needsSave: true });
        }
    }

    private setCircleLong(n: number) {
        if (n != this.state.circleLong) {
            this.setState({ circleLong: n, needsSave: true });
        }
    }

    private setCircleRadius(n: number) {
        if (n != this.state.radiusHeight) {
            this.setState({ radiusHeight: n, needsSave: true });
        }
    }

    private setRectTopLat(n: number, rectNumber: number): void {
        if (rectNumber == 1) {
            if (n != this.state.rect1topLat) {
                this.setState({ rect1topLat: n, needsSave: true });
            }
        } else if (rectNumber == 2) {
            if (n != this.state.rect2topLat) {
                this.setState({ rect2topLat: n, needsSave: true });
            }
        }
    }

    private setRectLeftLong(n: number, rectNumber: number): void {
        if (rectNumber == 1) {
            if (n != this.state.rect1leftLong) {
                this.setState({ rect1leftLong: n, needsSave: true });
            }
        } else if (rectNumber == 2) {
            if (n != this.state.rect2leftLong) {
                this.setState({ rect2leftLong: n, needsSave: true });
            }
        }
    }

    private setRectBottomLat(n: number, rectNumber: number): void {
        if (rectNumber == 1) {
            if (n != this.state.rect1bottomLat) {
                this.setState({ rect1bottomLat: n, needsSave: true });
            }
        } else if (rectNumber == 2) {
            if (n != this.state.rect2bottomLat) {
                this.setState({ rect2bottomLat: n, needsSave: true });
            }
        }
    }
    private setRectRightLong(n: number, rectNumber: number): void {
        if (rectNumber == 1) {
            if (n != this.state.rect1rightLong) {
                this.setState({ rect1rightLong: n, needsSave: true });
            }
        } else if (rectNumber == 2) {
            if (n != this.state.rect2rightLong) {
                this.setState({ rect2rightLong: n, needsSave: true });
            }
        }
    }

    render() {

        return (
            <>
                <br />
                <Gallery gutter="sm">
                    <GalleryItem>
                        <FormGroup
                            label="Location Name"
                            isRequired={true}
                            fieldId="location-name-form"
                        >
                            <TextInput
                                type="text"
                                id="location-name-form"
                                name="location-name-form"
                                aria-describedby="location-name-form-helper"
                                value={this.state.name}
                                onChange={x => this.setName(x)}
                                isValid={(!!this.state.name && this.state.name.length > 0)}
                            />
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup
                            label="Start Frequency (MHz)"
                            isRequired={true}
                            fieldId="start-freq-form"
                        >
                            <TextInput
                                type="number"
                                id="start-freq-form"
                                name="start-freq-form"
                                aria-describedby="start-freq-form-helper"
                                value={this.state.startFreq}
                                onChange={(x) => this.setStartFreq(Number(x))}
                                isValid={(!!this.state.startFreq && this.state.startFreq > 0)}
                            />
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup
                            label="End Frequency (MHz)"
                            isRequired={true}
                            fieldId="end-freq-form"
                            helperTextInvalid="End frequency must be provided and be larger than Start Frequency"
                        >
                            <TextInput
                                type="number"
                                id="end-freq-form"
                                name="end-freq-form"
                                aria-describedby="end-freq-form-helper"
                                value={this.state.endFreq}
                                onChange={(x) => this.setEndFreq(Number(x))}
                                isValid={(!!this.state.endFreq && this.state.endFreq > 0 && (this.state.startFreq ?? 0) <= this.state.endFreq)}
                            />
                        </FormGroup>
                    </GalleryItem>

                    <GalleryItem>
                        <FormGroup
                            label="Exclusion Zone"
                            isRequired={true}
                            fieldId="zone-type-form"
                        >
                            <FormSelect
                                type="number"
                                id="zone-type-form"
                                name="zone-type-form"
                                aria-describedby="zone-type-form-helper"
                                value={this.state.zoneType}
                                onChange={(x) => this.setZoneType(x)}
                                isValid={!!this.state.zoneType}>
                                <FormSelectOption label="Circle" key="Circle" value="Circle" />
                                <FormSelectOption label="One Rectangle" key="One Rectangle" value="One Rectangle" />
                                <FormSelectOption label="Two Rectangles" key="Two Rectangles" value="Two Rectangles" />
                                <FormSelectOption label="Horizon Distance" key="Horizon Distance" value="Horizon Distance" />
                            </FormSelect>
                        </FormGroup>
                    </GalleryItem>

                    {this.state.zoneType == "Circle" &&
                        <>
                            <FormGroup fieldId="circle-group">

                                <GalleryItem>
                                    <FormGroup
                                        label="Circle Center Latitude"
                                        isRequired={true}
                                        fieldId="circ-lat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-lat-form"
                                            name="circ-lat-form"
                                            aria-describedby="circ-lat-form-helper"
                                            value={this.state.circleLat}
                                            onChange={(x) => this.setCircleLat(Number(x))}
                                            isValid={(this.state.circleLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Circle Center Longitude"
                                        isRequired={true}
                                        fieldId="circ-long-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-long-form"
                                            name="circ-long-form"
                                            aria-describedby="circ-long-form-helper"
                                            value={this.state.circleLong}
                                            onChange={(x) => this.setCircleLong(Number(x))}
                                            isValid={(this.state.circleLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Circle Radius (km)"
                                        isRequired={true}
                                        fieldId="circ-radius-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-radius-form"
                                            name="circ-radius-form"
                                            aria-describedby="circ-radius-form-helper"
                                            value={this.state.radiusHeight}
                                            onChange={(x) => this.setCircleRadius(Number(x))}
                                            isValid={(this.state.radiusHeight !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                            </FormGroup>
                        </>
                    }
                    {this.state.zoneType == "One Rectangle" &&
                        <>
                            <FormGroup fieldId="one-rect-group">
                                <GalleryItem>
                                    <FormGroup
                                        label="Top Latitude"
                                        isRequired={true}
                                        fieldId="rect-toplat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-toplat-form"
                                            name="rect-toplat-form"
                                            aria-describedby="rect-toplat-form-helper"
                                            value={this.state.rect1topLat}
                                            onChange={(x) => this.setRectTopLat(Number(x), 1)}
                                            isValid={(this.state.circleLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Left Longitude"
                                        isRequired={true}
                                        fieldId="circ-long-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-long-form"
                                            name="circ-long-form"
                                            aria-describedby="circ-long-form-helper"
                                            value={this.state.circleLong}
                                            onChange={(x) => this.setRectLeftLong(Number(x), 1)}
                                            isValid={(this.state.circleLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Bottom Latitude"
                                        isRequired={true}
                                        fieldId="rect-botlat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-botlat-form"
                                            name="rect-botlat-form"
                                            aria-describedby="rect-botlat-form-helper"
                                            value={this.state.rect1bottomLat}
                                            onChange={(x) => this.setRectBottomLat(Number(x), 1)}
                                            isValid={(this.state.rect1bottomLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Right Longitude"
                                        isRequired={true}
                                        fieldId="rect-rgtlon-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-rgtlon-form"
                                            name="rect-rgtlon-formm"
                                            aria-describedby="rect-rgtlon-form-helper"
                                            value={this.state.rect1rightLong}
                                            onChange={(x) => this.setRectRightLong(Number(x), 1)}
                                            isValid={(this.state.rect1rightLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                            </FormGroup>
                        </>
                    }
                    {this.state.zoneType == "Two Rectangles" &&
                        <>
                            <FormGroup label="Rectangle One" fieldId="rect-one">
                                <GalleryItem>
                                    <FormGroup
                                        label="Top Latitude"
                                        isRequired={true}
                                        fieldId="rect-toplat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-toplat-form"
                                            name="rect-toplat-form"
                                            aria-describedby="rect-toplat-form-helper"
                                            value={this.state.rect1topLat}
                                            onChange={(x) => this.setRectTopLat(Number(x), 1)}
                                            isValid={(this.state.rect1topLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Left Longitude"
                                        isRequired={true}
                                        fieldId="circ-long-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-long-form"
                                            name="rect-long-form"
                                            aria-describedby="rect-long-form-helper"
                                            value={this.state.rect1leftLong}
                                            onChange={(x) => this.setRectLeftLong(Number(x), 1)}
                                            isValid={(this.state.rect1leftLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Bottom Latitude"
                                        isRequired={true}
                                        fieldId="rect-botlat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-botlat-form"
                                            name="rect-botlat-form"
                                            aria-describedby="rect-botlat-form-helper"
                                            value={this.state.rect1bottomLat}
                                            onChange={(x) => this.setRectBottomLat(Number(x), 1)}
                                            isValid={(this.state.rect1bottomLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Right Longitude"
                                        isRequired={true}
                                        fieldId="rect-rgtlon-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect-rgtlon-form"
                                            name="rect-rgtlon-formm"
                                            aria-describedby="rect-rgtlon-form-helper"
                                            value={this.state.rect1rightLong}
                                            onChange={(x) => this.setRectRightLong(Number(x), 1)}
                                            isValid={(this.state.rect1rightLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                            </FormGroup>
                            <FormGroup label="Rectangle Two" fieldId="rect-two">
                                <GalleryItem>
                                    <FormGroup
                                        label="Top Latitude"
                                        isRequired={true}
                                        fieldId="rect2-toplat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect2-toplat-form"
                                            name="rect2-toplat-form"
                                            aria-describedby="rect2-toplat-form-helper"
                                            value={this.state.rect2topLat}
                                            onChange={(x) => this.setRectTopLat(Number(x), 2)}
                                            isValid={(this.state.rect2topLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Left Longitude"
                                        isRequired={true}
                                        fieldId="rect2-leftlong-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect2-leftlong-form"
                                            name="rect2-leftlong-form"
                                            aria-describedby="rect2-leftlong-form-helper"
                                            value={this.state.rect2leftLong}
                                            onChange={(x) => this.setRectLeftLong(Number(x), 2)}
                                            isValid={(this.state.rect2leftLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Bottom Latitude"
                                        isRequired={true}
                                        fieldId="rect2-botlat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect2-botlat-form"
                                            name="rect2-botlat-form"
                                            aria-describedby="rect2-botlat-form-helper"
                                            value={this.state.rect2bottomLat}
                                            onChange={(x) => this.setRectBottomLat(Number(x), 2)}
                                            isValid={(this.state.rect2bottomLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Right Longitude"
                                        isRequired={true}
                                        fieldId="rect2-rgtlon-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="rect2-rgtlon-form"
                                            name="rect2-rgtlon-formm"
                                            aria-describedby="rect2-rgtlon-form-helper"
                                            value={this.state.rect2rightLong}
                                            onChange={(x) => this.setRectRightLong(Number(x), 2)}
                                            isValid={(this.state.rect2rightLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                            </FormGroup>
                        </>
                    }
                    {this.state.zoneType == "Horizon Distance" &&
                        <>
                            <FormGroup fieldId="horz-group">
                                <GalleryItem>
                                    <FormGroup
                                        label="Latitude"
                                        isRequired={true}
                                        fieldId="circ-lat-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-lat-form"
                                            name="circ-lat-form"
                                            aria-describedby="circ-lat-form-helper"
                                            value={this.state.circleLat}
                                            onChange={(x) => this.setCircleLat(Number(x))}
                                            isValid={(this.state.circleLat !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Longitude"
                                        isRequired={true}
                                        fieldId="circ-long-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-long-form"
                                            name="circ-long-form"
                                            aria-describedby="circ-long-form-helper"
                                            value={this.state.circleLong}
                                            onChange={(x) => this.setCircleLong(Number(x))}
                                            isValid={(this.state.circleLong !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                                <GalleryItem>
                                    <FormGroup
                                        label="Antenna AGL height (m)"
                                        isRequired={true}
                                        fieldId="circ-radius-form"
                                    >
                                        <TextInput
                                            type="number"
                                            id="circ-radius-form"
                                            name="circ-radius-form"
                                            aria-describedby="circ-radius-form-helper"
                                            value={this.state.radiusHeight}
                                            onChange={(x) => this.setCircleRadius(Number(x))}
                                            isValid={(this.state.radiusHeight !== undefined)}
                                        />
                                    </FormGroup>
                                </GalleryItem>
                            </FormGroup>
                        </>
                    }
                    <GalleryItem>
                        <Button variant="primary" icon={<PlusCircleIcon />} onClick={() => this.submit()}>Add</Button>
                    </GalleryItem>
                </Gallery></>
        )
    }

}
