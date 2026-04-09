import React, { CSSProperties } from 'react';
import GoogleMapReact from 'google-map-react';
import { guiConfig } from '../Lib/RatApi';
import { GeoJson } from '../Lib/RatApiTypes';
import { logger } from '../Lib/Logger';

/**
 * MapContainer.tsx: Wrapper for google map component so that our app can communicate with geoJson
 * author: Sam Smucny
 */

/**
 * Test data
 */
const rectTest: GeoJson = {
  type: 'FeatureCollection',
  features: [
    {
      type: 'Feature',
      properties: {
        ItoN: -43.34,
        kind: 'HMAP',
        indoor: 'Y',
      },
      geometry: {
        type: 'Polygon',
        coordinates: [
          [
            [-82.84790039062499, 38.09133660751176],
            [-80.74951171875, 38.09133660751176],
            [-80.74951171875, 39.51251701659638],
            [-82.84790039062499, 39.51251701659638],
            [-82.84790039062499, 38.09133660751176],
          ],
        ],
      },
    },
  ],
};

/**
 * Properties to be passed to `MapContainer`
 */
interface MapProps {
  /**
   * geographic data to render on map. To rerender
   * assign a new object to the property. Do not edit
   * the existing object.
   */
  geoJson: GeoJson;

  /**
   * Bounds of heat map region
   */
  selectionRectangle?: {
    north: number;
    south: number;
    east: number;
    west: number;
  };

  /**
   * Location of RLAN in Point Analysis
   */
  markerPosition?: {
    lat: number;
    lng: number;
  };

  /**
   * Color of marker
   */
  markerColor?: string;

  /**
   * Callback when user changes `selectionRectangle` from the
   * Google maps drawer
   */
  onRectUpdate?: (rect: any) => void;

  /**
   * Callback when user changes `markerPosition` from the
   * Google maps drawer
   */
  onMarkerUpdate?: (lat: number, lon: number) => void;

  /**
   * Initial center of map
   */
  center: {
    lat: number;
    lng: number;
  };

  /**
   * The specified mode enables/disables certain features
   */
  mode: 'Point' | 'Exclusion' | 'Heatmap' | 'Mobile';

  /**
   * Initial zoom level
   */
  zoom: number;

  /**
   * Styles to apply to map items.
   * The key specifies the kind of the `GeoJson`.
   * The value is either a dictionary of CSS classes
   * or a function that produces such a dictionary
   * given a Google Map feature object
   */
  styles?: Map<string, CSSProperties | ((feature: any) => CSSProperties)>;

  /**
   * URL to a kml file to display on the map
   */
  kmlUrl?: string;

  /**
   * Version counter used by component to indicate when it should update.
   */
  versionId: number;
}

/**
 * URL keys interface that specifies which Google libraries to use
 */
interface URLKeys {
  key: string;
  libraries?: string;
}

/**
 * Wrapper around Google Map React component.
 * Adds additional functionality by working directly with Google Maps API
 */
class MapContainer extends React.Component<MapProps> {
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
  private center?: { lat: number; lng: number };

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

  /**
   * AdvancedMarkerElement constructor — loaded unconditionally using
   * DEMO_MAP_ID (Google's built-in constant; no Cloud Console Map ID needed).
   * Falls back to google.maps.Marker when unavailable.
   */
  private AdvancedMarkerElement?: any;

  /**
   * First click position for two-click rectangle drawing (Heatmap mode).
   * Replaces the decommissioned Drawing Library.
   */
  private heatmapFirstClick?: any;

  /**
   * Temporary marker shown at the first click corner in Heatmap mode.
   */
  private heatmapCornerMarker?: any;

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
    // The Drawing Library was decommissioned in May 2026. We no longer load
    // it here; click-based listeners handle marker placement and rectangle
    // drawing instead.
  }

  /**
   * Handler for when google map is loaded.
   * This is when we can call geoJson functions
   * and do a bunch of initialization of the map
   */
  private apiIsLoaded = async (map: any, maps: any) => {
    this.map = map;
    this.maps = maps;
    this.map.data.setStyle(
      this.props.styles
        ? (feature: any) => {
            const kind = feature.getProperty('kind');
            if (this.props.styles!.has(kind)) {
              const style = this.props.styles!.get(kind);
              return style instanceof Function ? style(feature) : style;
            }
            return {};
          }
        : {},
    );

    // Load AdvancedMarkerElement unconditionally — mapId: 'DEMO_MAP_ID' is
    // Google's official constant for enabling advanced markers without
    // needing a Cloud Console Map ID (cloud styling is not required).
    try {
      const markerLib = await maps.importLibrary('marker');
      this.AdvancedMarkerElement = markerLib.AdvancedMarkerElement;
    } catch (e) {
      logger.warn('AdvancedMarkerElement unavailable, using legacy Marker:', e);
    }

    // update Marker, Rectangle, GeoJson, and set center/zoom
    this.componentDidUpdate();

    // Point analysis: click on the map to place/move the RLAN marker.
    // The Drawing Library (which provided DrawingManager) was decommissioned
    // in May 2026 and is no longer available.
    if (this.props.mode === 'Point') {
      this.map.setOptions({ draggableCursor: 'crosshair' });
      this.maps.event.addListener(this.map, 'click', (event: any) => {
        if (this.rlanMarker) {
          // Move existing marker
          if (this.AdvancedMarkerElement) {
            this.rlanMarker.position = event.latLng;
          } else {
            this.rlanMarker.setPosition(event.latLng);
          }
        } else {
          this.rlanMarker = this.createMarker({
            position: event.latLng,
            map: this.map,
            title: 'RLAN',
            label: 'R',
            zIndex: 100,
            clickable: false,
          });
        }
        if (this.props.onMarkerUpdate) {
          const pos = event.latLng;
          this.props.onMarkerUpdate(pos.lat(), pos.lng());
        }
      });
    }

    // Heatmap: two-click rectangle drawing replacing the decommissioned
    // Drawing Library. First click sets one corner; second click completes
    // the rectangle.
    if (this.props.mode === 'Heatmap') {
      this.map.setOptions({ draggableCursor: 'crosshair' });
      this.maps.event.addListener(this.map, 'click', (event: any) => {
        if (!this.heatmapFirstClick) {
          this.heatmapFirstClick = event.latLng;
          // Show a small marker at the first corner for visual feedback
          this.heatmapCornerMarker = this.createMarker({
            position: event.latLng,
            map: this.map,
            title: 'Corner 1 — click to set the opposite corner',
            zIndex: 10,
            clickable: false,
          });
        } else {
          // Complete the rectangle
          const lat1 = this.heatmapFirstClick.lat();
          const lng1 = this.heatmapFirstClick.lng();
          const lat2 = event.latLng.lat();
          const lng2 = event.latLng.lng();
          const bounds = {
            north: Math.max(lat1, lat2),
            south: Math.min(lat1, lat2),
            east: Math.max(lng1, lng2),
            west: Math.min(lng1, lng2),
          };

          if (this.heatmapCornerMarker) {
            if (this.AdvancedMarkerElement) {
              this.heatmapCornerMarker.map = null;
            } else {
              this.heatmapCornerMarker.setMap(null);
            }
            this.heatmapCornerMarker = undefined;
          }

          if (this.rectangle) this.rectangle.setMap(null);
          this.rectangle = new this.maps.Rectangle({
            bounds,
            editable: false,
            draggable: false,
            fillColor: 'yellow',
            fillOpacity: 0.1,
            map: this.map,
          });

          if (this.props.onRectUpdate) this.props.onRectUpdate(this.rectangle);
          this.heatmapFirstClick = undefined;
        }
      });

      // listeners for showing tile info on hover
      this.map.data.addListener('mouseover', (a: any) => {
        if (a.feature.getProperty('kind') !== 'HMAP') return;
        logger.info('Showing heat map info: ', a);
        const points: { lat: number; lng: number }[] = a.feature
          .getGeometry()
          .getArray()[0]
          .getArray()
          .map((x: any) => x.toJSON());
        const lat = points.map((x) => x.lat).reduce((x, y) => x + y) / points.length;
        const lng = points.map((x) => x.lng).reduce((x, y) => x + y) / points.length;
        let content = '<p>&lt; minEIRP</p>';
        if (!!a.feature.getProperty('ItoN')) {
          content =
            '<p>I/N: ' +
            a.feature.getProperty('ItoN').toFixed(2) +
            '</p><p>' +
            (a.feature.getProperty('indoor') === 'Y' ? 'Indoors' : 'Outdoors') +
            '</p>';
        }
        if (!!a.feature.getProperty('eirpLimit')) {
          content =
            '<p>EIRP Limit: ' +
            a.feature.getProperty('eirpLimit') +
            '</p><p>' +
            (a.feature.getProperty('indoor') === 'Y' ? 'Indoors' : 'Outdoors') +
            '</p>';
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
            disableAutoPan: true,
          });
        }
        this.infoWindow.open(this.map);
      });
      this.map.data.addListener('mouseout', (a: any) => {
        if (a.feature.getProperty('kind') !== 'HMAP') return;
        logger.info('Closing heat map info: ', a);
        if (this.infoWindow) this.infoWindow.close();
      });
    }
  };

  /**
   * Create a marker using AdvancedMarkerElement if a Map ID is configured,
   * falling back to the legacy google.maps.Marker.
   */
  private createMarker(opts: {
    position: any;
    map: any;
    title?: string;
    label?: string;
    zIndex?: number;
    icon?: any;
    clickable?: boolean;
    visible?: boolean;
  }): any {
    if (this.AdvancedMarkerElement) {
      const el = document.createElement('div');
      el.style.cssText =
        'background:#4285F4;color:#fff;border-radius:50%;width:24px;height:24px;display:flex;align-items:center;justify-content:center;font-weight:bold;font-size:12px;';
      if (opts.label) el.textContent = opts.label;
      if (opts.icon) {
        el.style.background = opts.icon.fillColor || '#4285F4';
        el.style.width = `${(opts.icon.scale || 5) * 4}px`;
        el.style.height = `${(opts.icon.scale || 5) * 4}px`;
        el.textContent = '';
      }
      return new this.AdvancedMarkerElement({
        position: opts.position,
        map: opts.map,
        title: opts.title,
        zIndex: opts.zIndex,
        content: el,
      });
    }
    return new this.maps.Marker({
      position: opts.position,
      map: opts.map,
      title: opts.title,
      label: opts.label,
      zIndex: opts.zIndex,
      icon: opts.icon,
      clickable: opts.clickable !== false,
      visible: opts.visible !== false,
    });
  }

  /**
   * Get position from either an AdvancedMarkerElement or legacy Marker.
   */
  private getMarkerPosition(marker: any): { lat: () => number; lng: () => number } {
    if (this.AdvancedMarkerElement && marker instanceof this.AdvancedMarkerElement) {
      const pos = marker.position;
      if (pos && typeof pos.lat === 'function') return pos;
      return { lat: () => pos.lat, lng: () => pos.lng };
    }
    return marker.getPosition();
  }

  /**
   * Called when Google API is finished loading. Triggers initialization.
   */
  private onLoad = ({ map, maps }: { map: any; maps: any }) => this.apiIsLoaded(map, maps);

  /**
   * Update the `GeoJson` if it has changed
   */
  private updateGeoJson() {
    if (this.map && this.map !== null) {
      if (this.rectangle) {
        this.rectangle.setVisible(false);
        this.rectangle.setMap(null);
        this.rectangle = undefined;
      } else if (this.props.selectionRectangle) {
        const rect = new this.maps.Rectangle({
          strokeColor: '#000000',
          fillColor: '#000000',
          bounds: this.props.selectionRectangle,
          clickable: false,
          fillOpacity: 0,
          map: this.map,
        });
        this.rectangle = rect;
      }

      // short circuit update function to avoid costly redraw if no change in object
      // props.geoJson object should not be mutated. If it is changed the value is
      // set to a new object (result of HTTP response)
      if (this.currentGeoJson === this.props.geoJson) return;

      this.currentGeoJson = this.props.geoJson;

      // remove current geographical data from map
      this.map.data.forEach((f: any) => this.map.data.remove(f));
      this.markers.forEach((m) => {
        if (this.AdvancedMarkerElement && m instanceof this.AdvancedMarkerElement) {
          m.map = null;
        } else {
          m.setMap(null);
        }
      });
      this.markers.length = 0; // empty array

      // add new features
      this.map.data.addGeoJson(this.props.geoJson); // add polygons

      // render additional features
      this.props.geoJson.features.forEach((poly) => {
        if (poly.properties.kind === 'FS') {
          // for each fs, add marker
          const existingMarker = this.markers.find((x) => {
            const pos = this.getMarkerPosition(x);
            return pos.lat() === poly.geometry.coordinates[0][0][1] && pos.lng() === poly.geometry.coordinates[0][0][0];
          });
          if (existingMarker) {
            const newTitle =
              [
                'FSID: ' + poly.properties.FSID,
                'Start Freq: ' + poly.properties.startFreq.toFixed(2) + ' MHz',
                'Stop Freq:  ' + poly.properties.stopFreq.toFixed(2) + ' MHz',
              ].join('\n') +
              '\n\n' +
              (this.AdvancedMarkerElement && existingMarker instanceof this.AdvancedMarkerElement
                ? existingMarker.title
                : existingMarker.getTitle());
            if (this.AdvancedMarkerElement && existingMarker instanceof this.AdvancedMarkerElement) {
              existingMarker.title = newTitle;
            } else {
              existingMarker.setTitle(newTitle);
            }
          } else {
            this.markers.push(
              this.createMarker({
                map: this.map,
                position: {
                  // use the first coordinate of polygon as FS location
                  // @ts-ignore
                  lat: poly.geometry.coordinates[0][0][1],
                  // @ts-ignore
                  lng: poly.geometry.coordinates[0][0][0],
                },
                title: [
                  'FSID: ' + poly.properties.FSID,
                  'Start Freq: ' + poly.properties.startFreq.toFixed(2) + ' MHz',
                  'Stop Freq:  ' + poly.properties.stopFreq.toFixed(2) + ' MHz',
                ].join('\n'),
              }),
            );
          }
        } else if (poly.properties.kind === 'ZONE') {
          // add marker for the FS at center of exclusion zone
          const zone = this.createMarker({
            map: this.map,
            position: {
              lat: poly.properties.lat,
              lng: poly.properties.lon,
            },
            title: [
              'FSID: ' + poly.properties.FSID,
              'Terrain height: ' + poly.properties.terrainHeight + ' m',
              'Height (AGL): ' + poly.properties.height + ' m',
            ].join('\n'),
            zIndex: 100,
          });
          this.markers.push(zone);
          this.center = this.getMarkerPosition(zone).lat
            ? { lat: this.getMarkerPosition(zone).lat(), lng: this.getMarkerPosition(zone).lng() }
            : undefined;
          this.zoom = 16;
        }
      });
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
          zIndex: 3,
        });
      }
      if (this.map) {
        this.map.fitBounds(this.rectangle.getBounds());
        this.center = this.map.getCenter();
        this.zoom = this.map.getZoom();
      }
      logger.info('Updating rect bounds: ', this.rectangle.getBounds().toJSON());
    }
  }

  /**
   * Update the Point Analysis marker
   */
  private updateMarker() {
    if (
      this.props.markerPosition === undefined ||
      !Number.isFinite(this.props.markerPosition.lng) ||
      !Number.isFinite(this.props.markerPosition.lat) ||
      !this.maps
    )
      return;
    if (this.props.onMarkerUpdate) {
      const curPos = this.rlanMarker ? this.getMarkerPosition(this.rlanMarker) : undefined;
      const same =
        curPos && curPos.lat() === this.props.markerPosition.lat && curPos.lng() === this.props.markerPosition.lng;
      if (this.rlanMarker && !same) {
        if (this.AdvancedMarkerElement && this.rlanMarker instanceof this.AdvancedMarkerElement) {
          this.rlanMarker.position = this.props.markerPosition;
        } else {
          this.rlanMarker.setPosition(this.props.markerPosition);
        }
      } else if (!this.rlanMarker) {
        this.rlanMarker = this.createMarker({
          clickable: false,
          map: this.map,
          visible: true,
          zIndex: 100,
          label: 'R',
          title: 'RLAN',
          position: this.props.markerPosition,
        });
        this.center = this.props.markerPosition;
        this.zoom = 17;
      }
    } else if (this.props.markerPosition) {
      // called for mobile AP
      const circleRLAN = {
        path: this.maps.SymbolPath.CIRCLE,
        scale: 5,
        fillOpacity: 1,
        fillColor: this.props.markerColor || 'blue',
        strokeColor: this.props.markerColor || 'blue',
      };
      if (this.rlanMarker) {
        if (this.AdvancedMarkerElement && this.rlanMarker instanceof this.AdvancedMarkerElement) {
          this.rlanMarker.position = this.props.markerPosition;
        } else {
          if (this.props.markerColor) this.rlanMarker.setIcon(circleRLAN);
          this.rlanMarker.setPosition(this.props.markerPosition);
        }
        this.center = this.props.markerPosition;
        this.zoom = this.map.getZoom();
      } else {
        this.rlanMarker = this.createMarker({
          clickable: false,
          map: this.map,
          visible: true,
          zIndex: 100,
          title: 'MOBILE AP',
          icon: circleRLAN,
          position: this.props.markerPosition,
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
        map: this.map,
      });
    } else if (!this.kmlLayer) {
      // make new layer
      this.kmlLayer = new this.maps.KmlLayer(newKml, {
        suppressInfoWindows: true,
        preserveViewport: false,
        map: this.map,
      });
    }
  }

  /**
   * @override
   * @param nextProps If `nextProps.versionId` is different then update
   */
  shouldComponentUpdate(nextProps: MapProps) {
    const update = nextProps.versionId !== this.props.versionId;
    if (nextProps.markerPosition && nextProps.markerPosition !== this.props.markerPosition) {
      this.updateMarker();
      if (this.map && this.center) this.map.setCenter(this.center);
      if (this.map && this.zoom) this.map.setZoom(this.zoom);
      if (
        nextProps.onMarkerUpdate &&
        nextProps.markerPosition &&
        nextProps.markerPosition.lat !== undefined &&
        nextProps.markerPosition.lng !== undefined
      )
        if (this.map) this.map.panTo(nextProps.markerPosition);
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
      if (this.props.kmlUrl) this.updateKml(this.props.kmlUrl);

      // update map center an zoom to they match the previous values
      // unless one of the three previous methods changed them
      if (this.center) this.map.setCenter(this.center);
      if (this.zoom) this.map.setZoom(this.zoom);
    }
  }

  render() {
    const mapOptions: any = {
      gestureHandling: 'cooperative',
      mapTypeControl: true,
      // Suppress the google-map-react default styles:[{featureType:"poi"...}]
      // which would conflict with mapId and produce a console warning.
      // null overrides the library's Object.assign merge so Google Maps
      // never sees a non-null styles array alongside mapId.
      styles: null,
      // Set at construction time so it does not conflict with the mapId's
      // vector rendering pipeline (post-load setMapTypeId causes a warning
      // when mapId is present).
      mapTypeId: 'satellite',
    };
    // DEMO_MAP_ID is Google's built-in constant that enables AdvancedMarkerElement
    // without a real Cloud Console Map ID (cloud styling is optional/separate).
    mapOptions.mapId = 'DEMO_MAP_ID';
    return (
      <div style={{ height: 500, width: '100%' }}>
        <GoogleMapReact
          bootstrapURLKeys={this.urlParams}
          defaultCenter={this.props.center}
          defaultZoom={this.props.zoom}
          yesIWantToUseGoogleMapApiInternals={true}
          onGoogleApiLoaded={this.onLoad}
          options={mapOptions}
        />
      </div>
    );
  }
}

export { MapContainer, MapProps };
