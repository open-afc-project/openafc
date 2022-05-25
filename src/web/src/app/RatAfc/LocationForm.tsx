import * as React from "react";
// import ReactTooltip from 'react-tooltip';
import { Location, LinearPolygon, RadialPolygon, Ellipse, Elevation } from "../Lib/RatAfcTypes";
import { GalleryItem, FormGroup, InputGroup, FormSelect, FormSelectOption, TextInput, InputGroupText } from "@patternfly/react-core";
import { ifNum } from "../Lib/Utils";
import { PairList } from "./PairList";
import { ElevationForm } from "./ElevationForm";

/**
 * LocationForm.tsx: Form component for location object in AP-AFC request
 * author: Sam Smucny
 */

/**
 * Parameters for location form component
 */
export interface LocationFormParams {
    location: {
        ellipse?: Ellipse,
        linearPolygon?: LinearPolygon,
        radialPolygon?: RadialPolygon,
        elevation?: Elevation
    },
    onChange: (val: {
        ellipse?: Ellipse,
        linearPolygon?: LinearPolygon,
        radialPolygon?: RadialPolygon,
        elevation?: Elevation
    }) => void
}

/**
 * possible location types
 */
type LocationType = "Ellipse" | "Linear Polygon" | "Radial Polygon";

/**
 * enumerated location types
 */
const locationTypes: LocationType[] = ["Ellipse", "Linear Polygon", "Radial Polygon"];

/**
 * LocationForm component
 */
export class LocationForm extends React.PureComponent<LocationFormParams> {
    private lastType?: LocationType;
    private outerBoundaryText?: string;

    constructor(props: LocationFormParams) {
        super(props);
        if (this.props.location.ellipse) {
            this.lastType = "Ellipse";
        } else if (this.props.location.linearPolygon) {
            this.lastType = "Linear Polygon";
        } else if (this.props.location.radialPolygon) {
            this.lastType = "Radial Polygon";
        }
    }

    private setFormDisplay(dispType: "Ellipse" | "Linear Polygon" | "Radial Polygon") {
        if (dispType === "Ellipse") {
            this.props.onChange({
                ellipse: {
                    center: { latitude: undefined, longitude: undefined },
                    majorAxis: undefined,
                    minorAxis: undefined,
                    orientation: undefined
                },
                linearPolygon: undefined,
                radialPolygon: undefined,
                elevation: this.props.location.elevation
            });
        } else if (dispType === "Linear Polygon") {
            this.props.onChange({
                ellipse: undefined,
                linearPolygon: {
                    outerBoundary: []
                },
                radialPolygon: undefined,
                elevation: this.props.location.elevation
            });
        } else if (dispType === "Radial Polygon") {
            this.props.onChange({
                ellipse: undefined,
                linearPolygon: undefined,
                radialPolygon: {
                    center: { latitude: undefined, longitude: undefined },
                    outerBoundary: []
                },
                elevation: this.props.location.elevation
            });
        }
    }

    private updateEllipse(ellipse: Partial<Ellipse>) {
        let ellipseCopy: Ellipse = this.props.location.ellipse || {} as Ellipse;
        Object.assign(ellipseCopy, ellipse);
        this.props.onChange({
            ellipse: ellipseCopy,
            elevation: this.props.location.elevation
        });
    }

    private updateLinearPoly(linearPoly: Partial<LinearPolygon>) {
        let linearCopy: LinearPolygon = this.props.location.linearPolygon || {} as LinearPolygon;
        Object.assign(linearCopy, linearPoly);
        this.props.onChange({
            linearPolygon: linearCopy,
            elevation: this.props.location.elevation
        });
    }

    private updateRadialPoly(radialPoly: Partial<RadialPolygon>) {
        let radialCopy: RadialPolygon = this.props.location.radialPolygon || {} as RadialPolygon;
        Object.assign(radialCopy, radialPoly);
        this.props.onChange({
            radialPolygon: radialCopy,
            elevation: this.props.location.elevation
        });
    }

    private updateElevation(elevation: Partial<Elevation>) {
        let elevationCopy = this.props.location.elevation || {} as Elevation;
        Object.assign(elevationCopy, elevation);
        this.props.onChange({
            ellipse: this.props.location.ellipse,
            linearPolygon: this.props.location.linearPolygon,
            radialPolygon: this.props.location.radialPolygon,
            elevation: elevationCopy,

        });
    }


    render() {

        let locType: LocationType | undefined;
        if (this.props.location.ellipse) {
            locType = "Ellipse";
        } else if (this.props.location.linearPolygon) {
            locType = "Linear Polygon";
        } else if (this.props.location.radialPolygon) {
            locType = "Radial Polygon";
        } else {
            locType = undefined;
        }
        const ellipse: Ellipse | undefined = this.props.location.ellipse;
        const linearPoly: LinearPolygon | undefined = this.props.location.linearPolygon;
        const radialPolygon: RadialPolygon | undefined = this.props.location.radialPolygon;

        return (<>
            <GalleryItem>
                <FormGroup label="Location Type" fieldId="horizontal-form-location-type">
                    <InputGroup>
                        <FormSelect
                            value={locType}
                            onChange={(x: LocationType) => this.setFormDisplay(x)}
                            id="horzontal-form-location-type"
                            name="horizontal-form-location-type"
                            style={{ textAlign: "right" }}
                        >
                            <FormSelectOption key={undefined} value={undefined} label="Select a location type" isDisabled={true} />
                            {locationTypes.map((option) => (
                                <FormSelectOption key={option} value={option} label={option} />
                            ))}
                        </FormSelect>
                    </InputGroup>
                </FormGroup>
            </GalleryItem>
            {locType === "Ellipse" && ellipse && (<>
                <GalleryItem>
                        <FormGroup label="Latitude"
                            fieldId="horizontal-form-latitude">
                            <InputGroup>
                                <TextInput
                                    value={ellipse.center.latitude}
                                    onChange={x => ifNum(x, n => this.updateEllipse({ center: { latitude: n, longitude: ellipse.center.longitude } }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-latitude"
                                    name="horizontal-form-latitude"
                                    isValid={ellipse.center.latitude !== undefined && ellipse.center.latitude >= -90 && ellipse.center.latitude <= 90}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>degrees</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Longitude"
                            fieldId="horizontal-form-longitude">
                            <InputGroup>
                                <TextInput
                                    value={ellipse.center.longitude}
                                    onChange={x => ifNum(x, n => this.updateEllipse({ center: { latitude: ellipse.center.latitude, longitude: n } }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-longitude"
                                    name="horizontal-form-longitude"
                                    isValid={ellipse.center.longitude !== undefined && ellipse.center.longitude >= -180 && ellipse.center.longitude <= 180}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>degrees</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Semi-major Axis"
                            fieldId="horizontal-form-maj-axis"
                            helperText="Attribute of uncertainty ellipse">
                            <InputGroup>
                                <TextInput
                                    value={ellipse.majorAxis}
                                    onChange={x => ifNum(x, n => this.updateEllipse({ majorAxis: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-maj-axis"
                                    name="horizontal-form-maj-axis"
                                    isValid={ellipse.majorAxis >= 0 && ellipse.majorAxis >= ellipse.minorAxis}
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
                                    value={ellipse.minorAxis}
                                    onChange={x => ifNum(x, n => this.updateEllipse({ minorAxis: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-min-axis"
                                    name="horizontal-form-min-axis"
                                    isValid={ellipse.minorAxis >= 0 && ellipse.majorAxis >= ellipse.minorAxis}
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
                                    value={ellipse.orientation}
                                    onChange={x => ifNum(x, n => this.updateEllipse({ orientation: n }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-orientation"
                                    name="horizontal-form-orientation"
                                    isValid={ellipse.orientation >= 0 && ellipse.orientation < 180}
                                    style={{ textAlign: "right" }}
                                /><InputGroupText>degrees</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
            </>)}
            {locType === "Linear Polygon" && linearPoly && (
                    <GalleryItem>
                        <PairList 
                            tableName="Outer Boundary"
                            firstLabel="Latitude"
                            fstValid={n => n >= -90 && n <= 90}
                            secondLabel="Longitude"
                            sndValid={n => n >= -180 && n <= 180}
                            values={linearPoly.outerBoundary.map(point => [point.latitude, point.longitude])}
                            onChange={values => this.updateLinearPoly({ outerBoundary: values.map(p => ({ latitude: p[0], longitude: p[1] }))})} />
                    </GalleryItem>)}
            {locType === "Radial Polygon" && radialPolygon && (<>
                    <GalleryItem>
                        <FormGroup label="Center Latitude"
                            fieldId="horizontal-form-latitude">
                            <InputGroup>
                                <TextInput
                                    value={radialPolygon.center.latitude}
                                    onChange={x => ifNum(x, n => this.updateRadialPoly({ center: { latitude: n, longitude: radialPolygon.center.longitude } }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-latitude"
                                    name="horizontal-form-latitude"
                                    isValid={radialPolygon.center.latitude !== undefined && radialPolygon.center.latitude >= -90 && radialPolygon.center.latitude <= 90}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>degrees</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup label="Center Longitude"
                            fieldId="horizontal-form-longitude">
                            <InputGroup>
                                <TextInput
                                    value={radialPolygon.center.longitude}
                                    onChange={x => ifNum(x, n => this.updateRadialPoly({ center: { latitude: radialPolygon.center.latitude, longitude: n } }))}
                                    type="number"
                                    step="any"
                                    id="horizontal-form-longitude"
                                    name="horizontal-form-longitude"
                                    isValid={radialPolygon.center.longitude !== undefined && radialPolygon.center.longitude >= -180 && radialPolygon.center.longitude <= 180}
                                    style={{ textAlign: "right" }}
                                />
                                <InputGroupText>degrees</InputGroupText>
                            </InputGroup>
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <PairList 
                            tableName="Outer Boundary"
                            firstLabel="Angle (deg)"
                            firstHelperText = "degrees off north clockwise in [0,360)"
                            fstValid={n => n >= 0 && n <= 360}
                            secondLabel="Radius (m)"
                            sndValid={n => n > 0}
                            values={radialPolygon.outerBoundary.map(point => [point.angle, point.length])}
                            onChange={values => this.updateRadialPoly({ outerBoundary: values.map(p => ({ angle: p[0], length: p[1] }))})} />
                    </GalleryItem>
                    </>)}
                    <ElevationForm
                        elevation={this.props.location.elevation}
                        onChange={x => this.updateElevation(x)}
                    />
          </>);
    }
}
