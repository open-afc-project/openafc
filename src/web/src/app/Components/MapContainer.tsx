import React, { CSSProperties } from "react";
import GoogleMapReact from "google-map-react";
import { guiConfig } from "../Lib/RatApi";
import { GeoJson } from "../Lib/RatApiTypes";
import { logger } from "../Lib/Logger";

/**
 * MapContainer.tsx: Wrapper for google map component so that our app can communicate with geoJson
 * author: Sam Smucny
 */

/**
 * Test data
 */
const rectTest: GeoJson = {
    "type": "FeatureCollection",
    "features": [
        {
            "type": "Feature",
            "properties": {
                ItoN: -43.34,
                kind: "HMAP",
                indoor: "Y"
            },
            "geometry": {
                "type": "Polygon",
                "coordinates": [
                    [
                        [
                            -82.84790039062499,
                            38.09133660751176
                        ],
                        [
                            -80.74951171875,
                            38.09133660751176
                        ],
                        [
                            -80.74951171875,
                            39.51251701659638
                        ],
                        [
                            -82.84790039062499,
                            39.51251701659638
                        ],
                        [
                            -82.84790039062499,
                            38.09133660751176
                        ]
                    ]
                ]
            }
        }
    ]
}

/**
 * Properties to be passed to `MapContainer`
 */
interface MapProps {

    /**
     * geographic data to render on map. To rerender
     * assign a new object to the property. Do not edit
     * the existing object.
     */
    geoJson: GeoJson,

    /**
     * Bounds of heat map region
     */
    selectionRectangle?: {
        north: number,
        south: number,
        east: number,
        west: number
    },

    /**
     * Location of RLAN in Point Analysis
     */
    markerPosition?: {
        lat: number,
        lng: number
    },

    /**
     * Color of marker
     */
    markerColor?: string,

    /**
     * Callback when user changes `selectionRectangle` from the 
     * Google maps drawer
     */
    onRectUpdate?: (rect: any) => void,

    /**
     * Callback when user changes `markerPosition` from the 
     * Google maps drawer
     */
    onMarkerUpdate?: (lat: number, lon: number) => void,

    /**
     * Initial center of map 
     */
    center: {
        lat: number,
        lng: number
    },

    /**
     * The specified mode enables/disables certain features
     */
    mode: "Point" | "Exclusion" | "Heatmap" | "Mobile",

    /**
     * Initial zoom level
     */
    zoom: number,

    /**
     * Styles to apply to map items.
     * The key specifies the kind of the `GeoJson`.
     * The value is either a dictionary of CSS classes
     * or a function that produces such a dictionary
     * given a Google Map feature object
     */
    styles?: Map<string, CSSProperties | ((feature: any) => CSSProperties)>,

    /**
     * URL to a kml file to display on the map
     */
    kmlUrl?: string,

    /**
     * Version counter used by component to indicate when it should update.
     */
    versionId: number
}

/**
 * URL keys interface that specifies which Google libraries to use
 */
interface URLKeys {
    key: string,
    libraries?: string
}

/**
 * Wrapper around Google Map React component.
 * Adds additional functionality by working directly with Google Maps API
 */
class MapContainer extends React.Component<MapProps>  {

    /**
     * Google maps map object
     */
    private map: any;

    /**
     * Google maps API object
     */
    private maps: any;

    /**
     * FS markers
     */
    private markers: any[];

    /**
     * Special RLAN marker
     */
    private rlanMarker: any;

    /**
     * Heat map bounding rectangle
     */
    private rectangle: any;

    /**
     * parameters to pass to Google Maps API
     */
    private urlParams: URLKeys;

    /**
     * Current center of map
     */
    private center?: { lat: number, lng: number };

    /**
     * Current zoom of map
     */
    private zoom?: number;

    /**
     * Tool tip popup over heat map tiles
     */
    private infoWindow?: any;

    /**
     * Reference to the GeoJson that is being displayed.
     * Used to check if GeoJson should be updated which
     * is a costly redraw procedure.
     */
    private currentGeoJson?: GeoJson;

    /**
     * Stores map KML layer
     */
    private kmlLayer?: any;

    constructor(props: any) {
        super(props);
        this.map = undefined;
        this.maps = undefined;
        this.markers = [];
        this.rectangle = undefined;
        this.center = undefined;
        this.zoom = undefined;
        this.currentGeoJson = undefined;

        this.urlParams = { key: guiConfig.google_apikey };
        this.urlParams.libraries = "drawing";
    }

    /**
     * Handler for when google map is loaded.
     * This is when we can call geoJson functions
     * and do a bunch of initialization of the map
     */
    private apiIsLoaded = (map: any, maps: any) => {
        this.map = map;
        this.maps = maps;
        this.map.setMapTypeId("satellite");
        this.map.data.setStyle(this.props.styles ? (feature: any) => {
            const kind = feature.getProperty("kind");
            // @ts-ignore
            if (this.props.styles.has(kind))
                // @ts-ignore
                const style = this.props.styles.get(kind);
            return style instanceof Function ? style(feature) : style;
        } : {});

        // update Marker, Rectangle, GeoJson, and set center/zoom
        this.componentDidUpdate();

        // Point analysis specific initialization
        if (this.props.mode === "Point") {
            const drawingManager = new this.maps.drawing.DrawingManager({
                drawingMode: this.maps.drawing.OverlayType.MARKER,
                drawingControl: true,
                drawingControlOptions: {
                    position: this.maps.ControlPosition.TOP_CENTER,
                    drawingModes: ["marker"]
                },
                markerOptions: {
                    clickable: false,
                    label: "R",
                    title: "RLAN",
                    zIndex: 100
                }
            });
            drawingManager.setMap(map);

            this.maps.event.addListener(drawingManager, "markercomplete", (marker: any) => {
                // remove existing marker
                if (this.rlanMarker) this.rlanMarker.setMap(null);

                // add new marker
                this.rlanMarker = marker;
                if (this.props.onMarkerUpdate)
                    this.props.onMarkerUpdate(this.rlanMarker.getPosition().lat(), this.rlanMarker.getPosition().lng());
            })
        }

        // Heat map specific initalization
        if (this.props.mode === "Heatmap") {
            const drawingManager = new this.maps.drawing.DrawingManager({
                drawingMode: this.maps.drawing.OverlayType.RECTANGLE,
                drawingControl: true,
                drawingControlOptions: {
                    position: this.maps.ControlPosition.TOP_CENTER,
                    drawingModes: ["rectangle"]
                },
                rectangleOptions: {
                    editable: false,
                    draggable: false,
                    color: "yellow",
                    fillOpacity: 0.1,
                    map: this.map
                }
            });
            drawingManager.setMap(map);
            this.maps.event.addListener(drawingManager, "rectanglecomplete", (rect: any) => {
                if (this.props.onRectUpdate) {
                    // remove existing rectangle if it exists
                    if (this.rectangle)
                        this.rectangle.setMap(null);

                    // set new rectangle
                    this.rectangle = rect;
                    this.props.onRectUpdate(this.rectangle);
                }
            });

            // listeners for showing tile info on hover
            this.map.data.addListener("mouseover", (a: any) => {
                if (a.feature.getProperty("kind") !== "HMAP") return;
                logger.info("Showing heat map info: ", a);
                const points: { lat: number, lng: number }[] = a.feature.getGeometry().getArray()[0].getArray().map((x: any) => x.toJSON())
                const lat = points.map(x => x.lat).reduce((x, y) => x + y) / points.length;
                const lng = points.map(x => x.lng).reduce((x, y) => x + y) / points.length;
                let content = "<p>&lt; minEIRP</p>";
                if(!!a.feature.getProperty("ItoN")){
                    content = "<p>I/N: " + a.feature.getProperty("ItoN").toFixed(2) + "</p><p>" + (a.feature.getProperty("indoor") === "Y" ? "Indoors" : "Outdoors") + "</p>"
                }
                if(!!a.feature.getProperty("eirpLimit")){
                    content = "<p>EIRP Limit: " + a.feature.getProperty("eirpLimit") + "</p><p>" + (a.feature.getProperty("indoor") === "Y" ? "Indoors" : "Outdoors") + "</p>"
                }
                const infoAnchor = { lat: lat, lng: lng };
                if (this.infoWindow) {
                    this.infoWindow.setContent(content);
                    this.infoWindow.setPosition(infoAnchor);
                } else {
                    this.infoWindow = new this.maps.InfoWindow({
                        content: content,
                        map: this.map,
                        position: infoAnchor,
                        disableAutoPan: true
                    });
                }
                this.infoWindow.open(this.map);
            })
            this.map.data.addListener("mouseout", (a: any) => {
                if (a.feature.getProperty("kind") !== "HMAP") return;
                logger.info("Closing heat map info: ", a);
                if (this.infoWindow)
                    this.infoWindow.close()
            })
        }
    }

    /**
     * Called when Google API is finished loading. Triggers initialization.
     */
    private onLoad = ({ map, maps }: { map: any, maps: any }) => this.apiIsLoaded(map, maps);

    /**
     * Update the `GeoJson` if it has changed
     */
    private updateGeoJson() {
        if (this.map && this.map !== null) {

            if (this.rectangle) {
                this.rectangle.setVisible(false)
                this.rectangle.setMap(null);
                this.rectangle = undefined;
            } else if (this.props.selectionRectangle) {
                const rect = new this.maps.Rectangle({
                    strokeColor: "#000000",
                    fillColor: "#000000",
                    bounds: this.props.selectionRectangle,
                    clickable: false,
                    fillOpacity: 0,
                    map: this.map
                })
                this.rectangle = rect;
            }

            // short circuit update function to avoid costly redraw if no change in object
            // props.geoJson object should not be mutated. If it is changed the value is
            // set to a new object (result of HTTP response)
            if (this.currentGeoJson === this.props.geoJson) return;

            this.currentGeoJson = this.props.geoJson;

            // remove current geographical data from map
            this.map.data.forEach((f: any) => this.map.data.remove(f));
            this.markers.forEach(m => {
                m.setMap(null);
            })
            this.markers.length = 0; // empty array

            // add new features
            this.map.data.addGeoJson(this.props.geoJson);   // add polygons

            // render additional features
            this.props.geoJson.features.forEach((poly) => {

                if (poly.properties.kind === "FS") {
                    // for each fs, add marker
                    const existingMarker = this.markers.find(x =>
                        x.getPosition().lat() === poly.geometry.coordinates[0][0][1]
                        && x.getPosition().lng() === poly.geometry.coordinates[0][0][0])
                    if (existingMarker) {
                        const newTitle = [
                            "FSID: " + poly.properties.FSID,
                            "Start Freq: " + poly.properties.startFreq.toFixed(2) + " MHz",
                            "Stop Freq:  " + poly.properties.stopFreq.toFixed(2) + " MHz"
                        ].join("\n") + "\n\n" + existingMarker.getTitle();
                        existingMarker.setTitle(newTitle);
                    } else {
                        this.markers.push(new this.maps.Marker({
                            map: this.map,
                            position: { // use the first coordinate of polygon as FS location
                                // @ts-ignore
                                lat: poly.geometry.coordinates[0][0][1],
                                // @ts-ignore
                                lng: poly.geometry.coordinates[0][0][0]
                            },
                            title: [
                                "FSID: " + poly.properties.FSID,
                                "Start Freq: " + poly.properties.startFreq.toFixed(2) + " MHz",
                                "Stop Freq:  " + poly.properties.stopFreq.toFixed(2) + " MHz"
                            ].join("\n")
                        }));
                    }
                } else if (poly.properties.kind === "ZONE") {
                    // add marker for the FS at center of exclusion zone
                    const zone = new this.maps.Marker({
                        map: this.map,
                        position: {
                            lat: poly.properties.lat,
                            lng: poly.properties.lon
                        },
                        title: [
                            "FSID: " + poly.properties.FSID,
                            "Terrain height: " + poly.properties.terrainHeight + " m",
                            "Height (AGL): " + poly.properties.height + " m"
                        ].join("\n"),
                        zIndex: 100
                    })
                    this.markers.push(zone);
                    this.center = zone.getPosition();
                    this.zoom = 16;
                }
            })

        }
    }

    /**
     * Update the Heat Map boundary
     */
    private updateRect() {
        if (this.props.selectionRectangle && this.maps) {
            if (this.rectangle) {
                this.rectangle.setBounds(this.props.selectionRectangle);
            } else {
                this.rectangle = new this.maps.Rectangle({
                    bounds: this.props.selectionRectangle,
                    editable: false,
                    map: this.map,
                    visible: true,
                    zIndex: 3
                })
            }
            if (this.map) {
                this.map.fitBounds(this.rectangle.getBounds());
                this.center = this.map.getCenter();
                this.zoom = this.map.getZoom();
            }
            logger.info("Updating rect bounds: ", this.rectangle.getBounds().toJSON());
        }
    }

    /**
     * Update the Point Analysis marker
     */
    private updateMarker() {
        if (this.props.markerPosition === undefined || !Number.isFinite(this.props.markerPosition.lng) || !Number.isFinite(this.props.markerPosition.lat) || !this.maps) return;
        if (this.props.onMarkerUpdate) {
            const same = this.rlanMarker && this.rlanMarker.getPosition().lat() === this.props.markerPosition.lat
                && this.rlanMarker.getPosition().lng() === this.props.markerPosition.lng;
            if (this.rlanMarker && !same) {
                this.rlanMarker.setPosition(this.props.markerPosition);
            } else if (!this.rlanMarker) {
                this.rlanMarker = new this.maps.Marker({
                    clickable: false,
                    map: this.map,
                    visible: true,
                    zIndex: 100,
                    label: "R",
                    title: "RLAN",
                    position: this.props.markerPosition
                })
                this.center = this.props.markerPosition;
                this.zoom = 17;
            }
        } else if (this.props.markerPosition) {
            // called for mobile AP
            if (this.rlanMarker) {
                if (this.props.markerColor) {
                    const circleRLAN = {
                        path: this.maps.SymbolPath.CIRCLE,
                        scale: 5,
                        fillOpacity: 1,
                        fillColor: this.props.markerColor,
                        strokeColor: this.props.markerColor
                    };
                    this.rlanMarker.setIcon(circleRLAN);
                }
                this.rlanMarker.setPosition(this.props.markerPosition);
                this.center = this.props.markerPosition;
                this.zoom = this.map.getZoom();
            } else {
                const circleRLAN = {
                    path: this.maps.SymbolPath.CIRCLE,
                    scale: 5,
                    fillOpacity: 1,
                    fillColor: this.props.markerColor || "blue",
                    strokeColor: this.props.markerColor ||  "blue"
                  };
                this.rlanMarker = new this.maps.Marker({
                    clickable: false,
                    map: this.map,
                    visible: true,
                    zIndex: 100,
                    title: "MOBILE AP",
                    icon: circleRLAN,
                    position: this.props.markerPosition
                });
                this.center = this.props.markerPosition;
                this.zoom = 17;
            }
        }
    }

    /**
     * Update google maps with new KML layer
     */
    private updateKml(newKml: string) {
        if (this.kmlLayer && this.props.kmlUrl !== newKml) {
            // KML layer already exists, update
            this.kmlLayer.setMap(null);
            this.kmlLayer = new this.maps.KmlLayer(newKml, {
                suppressInfoWindows: true,
                preserveViewport: false,
                map: this.map
            });
        } else if (!this.kmlLayer) {
            // make new layer 
            this.kmlLayer = new this.maps.KmlLayer(newKml, {
                suppressInfoWindows: true,
                preserveViewport: false,
                map: this.map
            });
        }
    }

    /**
     * @override
     * @param nextProps If `nextProps.versionId` is different then update
     */
    shouldComponentUpdate(nextProps: MapProps) {
        const update = nextProps.versionId !== this.props.versionId;
        if (nextProps.markerPosition && (nextProps.markerPosition !== this.props.markerPosition)) {
            this.updateMarker()
            this.map && this.center && this.map.setCenter(this.center);
            this.map && this.zoom && this.map.setZoom(this.zoom);
            if (nextProps.onMarkerUpdate && nextProps.markerPosition && nextProps.markerPosition.lat !== undefined && nextProps.markerPosition.lng !== undefined)
                this.map && this.map.panTo(nextProps.markerPosition);
        }
        if (update && this.maps && this.map && this.map.getCenter() && this.map.getZoom()) {
            // update center so that map doesn't move back
            this.center = this.map.getCenter().toJSON();
            this.zoom = this.map.getZoom();
        }
        if (this.maps && nextProps.kmlUrl) {
            this.updateKml(nextProps.kmlUrl);
        }
        return update;
    }

    /**
     * Update map elements
     * @override
     */
    componentDidUpdate() {
        if (this.maps && this.map) {
            this.updateGeoJson();
            this.updateRect();
            this.updateMarker();
            this.props.kmlUrl && this.updateKml(this.props.kmlUrl);

            // update map center an zoom to they match the previous values
            // unless one of the three previous methods changed them
            if (this.center)
                this.map.setCenter(this.center);
            if (this.zoom)
                this.map.setZoom(this.zoom);
        }
    }

    render() {
        return (
            <div style={{ height: 500, width: "100%" }}>
                <GoogleMapReact
                    bootstrapURLKeys={this.urlParams}
                    defaultCenter={this.props.center}
                    defaultZoom={this.props.zoom}
                    yesIWantToUseGoogleMapApiInternals={true}
                    onGoogleApiLoaded={this.onLoad}
                    options={{
                        gestureHandling: "cooperative",
                        mapTypeControl: true
                    }}
                />
            </div>
        );
    }
}

export { MapContainer, MapProps };