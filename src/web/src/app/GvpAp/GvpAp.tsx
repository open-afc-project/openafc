import * as React from 'react';
import { PageSection, Card, CardHead, CardBody, Title, Button, TextArea, Alert, Modal } from '@patternfly/react-core';
import { Table, TableHeader, TableBody, TableVariant } from '@patternfly/react-table';
import { exclusionZoneInquiry, exclusionZoneInquiryByString } from '../Lib/GvpApi';
import { ExclusionZoneInquiryResponseMessage, Location } from '../Lib/GvpTypes';
import { GeoJson, RatResponse, ResError } from '../Lib/RatApiTypes';
import { MapContainer } from '../Components/MapContainer';
import { JsonRawDisp } from '../Components/JsonRawDisp';
import { Timer } from '../Components/Timer';
import { areaFeature, gvpToGeoJson, zoneList } from './GvpGeoJson';
import { GvpForm } from './GvpForm';

/**
 * GvpAp.tsx: Page used for GVP Exclusion Zone Inquiry
 */

const mapStyles: Map<string, any> = new Map([
  [
    'GVP_ZONE',
    (feature: any) =>
      feature.getProperty('selected')
        ? { strokeColor: 'gold', strokeWeight: 4, fillColor: 'gold', fillOpacity: 0.3, zIndex: 1 }
        : { strokeColor: 'red', strokeWeight: 2, fillColor: 'red', fillOpacity: 0.15, zIndex: 0 },
  ],
  ['RLAN', { strokeColor: 'blue', fillColor: 'lightblue', fillOpacity: 0.1, zIndex: 2 }],
]);

const zoneColumns = ['Frequency range (MHz)', 'PSD (dBm/MHz)', 'Zone ID'];
const rowStyle = { cursor: 'pointer' };
const selectedRowStyle = { cursor: 'pointer', fontWeight: 600, background: '#fdf2d0' };

const SHOW_DIRECT_SEND = false;

/** Must match the height MapContainer renders. */
const MAP_HEIGHT_PX = 500;

// A malformed message is still sent, so the server can report what is wrong with it.
const pastedArea = (message: string): Location | undefined => {
  try {
    return JSON.parse(message).exclusionZoneInquiryRequests[0].areaOfIntendedOperation;
  } catch {
    return undefined;
  }
};

const fitView = (geoJson: GeoJson) => {
  const points = geoJson.features.reduce(
    (all: [number, number][], feature) => all.concat(feature.geometry.coordinates[0]),
    [],
  );
  const lats = points.map((p) => p[1]);
  const lngs = points.map((p) => p[0]);
  const south = Math.min(...lats);
  const north = Math.max(...lats);
  const west = Math.min(...lngs);
  const east = Math.max(...lngs);
  const cosLat = Math.cos((((south + north) / 2) * Math.PI) / 180);
  const metersTall = (north - south) * 111320;
  const metersWide = (east - west) * 111320 * cosLat;
  // A map tile is 156543 m per pixel at zoom 0 on the equator, halving with each zoom level
  const zoom = Math.floor(Math.log2((MAP_HEIGHT_PX * 156543 * cosLat) / Math.max(metersTall, metersWide, 1)));
  return { center: { lat: (south + north) / 2, lng: (west + east) / 2 }, zoom: Math.max(1, Math.min(zoom, 18)) };
};

export const GvpAp: React.FunctionComponent = () => {
  const [requestText, setRequestText] = React.useState('');
  const [isDirectSendOpen, setIsDirectSendOpen] = React.useState(false);
  const [rawJson, setRawJson] = React.useState<any>(undefined);
  const [error, setError] = React.useState<ResError | undefined>(undefined);
  const [isSending, setIsSending] = React.useState(false);
  const [response, setResponse] = React.useState<ExclusionZoneInquiryResponseMessage | undefined>(undefined);
  const [selectedZone, setSelectedZone] = React.useState<number | undefined>(undefined);
  const [responseKey, setResponseKey] = React.useState(0);
  const [mapVersion, setMapVersion] = React.useState(0);
  const [area, setArea] = React.useState<Location | undefined>(undefined);

  const zones = React.useMemo(() => {
    if (response === undefined) {
      return undefined;
    }
    const geoJson = gvpToGeoJson(response, selectedZone);
    const areaShape = area === undefined ? undefined : areaFeature(area);
    return areaShape === undefined ? geoJson : { ...geoJson, features: geoJson.features.concat([areaShape]) };
  }, [response, selectedZone, area]);

  const send = async (
    inquire: () => Promise<RatResponse<ExclusionZoneInquiryResponseMessage>>,
    areaOfOperation?: Location,
  ) => {
    setArea(areaOfOperation);
    setError(undefined);
    setRawJson(undefined);
    setResponse(undefined);
    setSelectedZone(undefined);
    setIsSending(true);
    const result = await inquire();
    setIsSending(false);
    if (result.kind === 'Success') {
      setRawJson(result.result);
      setResponse(result.result);
      // A new key remounts the map, which centers it on the new zones.
      setResponseKey((key) => key + 1);
      setMapVersion((version) => version + 1);
      return;
    }
    setError(result);
    // An HTTP error carries the fetch Response, which has no JSON to show.
    setRawJson(result.body instanceof Response ? undefined : result.body);
  };

  const sendDirect = () => {
    setIsDirectSendOpen(false);
    send(() => exclusionZoneInquiryByString(requestText), pastedArea(requestText));
  };

  // The table keeps the first click handler it is given, so state is read through updater functions.
  const selectZone = (index: number) => {
    setSelectedZone((selected) => (selected === index ? undefined : index));
    setMapVersion((version) => version + 1);
  };

  const zoneRows =
    response === undefined
      ? []
      : zoneList(response).map((zone, index) => {
          const style = index === selectedZone ? selectedRowStyle : rowStyle;
          return {
            index: index,
            cells: [
              zone.exclusionZoneFrequencyRange.lowFrequency + '-' + zone.exclusionZoneFrequencyRange.highFrequency,
              String(zone.psdLevel),
              zone.exclusionZoneId || '',
            ].map((title) => ({ title: title, props: { style: style } })),
          };
        });

  const view = zones !== undefined && zones.features.length > 0 ? fitView(zones) : undefined;

  return (
    <PageSection>
      <Card>
        <CardHead>
          <Title size="lg">Virtual GVP AP</Title>
        </CardHead>
        <CardBody>
          <GvpForm
            isSubmitting={isSending}
            onSubmit={(request) => send(() => exclusionZoneInquiry(request), request.areaOfIntendedOperation)}
          />
          {SHOW_DIRECT_SEND && (
            <>
              <Button variant="secondary" onClick={() => setIsDirectSendOpen(true)}>
                Direct Send Request
              </Button>
              <Modal
                title="Direct Send Request"
                isLarge={true}
                isOpen={isDirectSendOpen}
                onClose={() => setIsDirectSendOpen(false)}
                actions={[
                  <Button key="directSend-close" variant="secondary" onClick={() => setIsDirectSendOpen(false)}>
                    Close
                  </Button>,
                  <Button
                    key="directSend-send"
                    variant="primary"
                    isDisabled={requestText.length === 0}
                    onClick={sendDirect}
                  >
                    Send
                  </Button>,
                ]}
              >
                <TextArea
                  aria-label="Exclusion Zone Inquiry Request message"
                  resizeOrientation="vertical"
                  rows={16}
                  value={requestText}
                  onChange={(text: string) => setRequestText(text)}
                />
              </Modal>
            </>
          )}
          {isSending && (
            <Alert title="Processing" variant="info">
              {'Your request has been submitted. '}
              <Timer />
            </Alert>
          )}
          {error !== undefined && (
            <Alert title={error.errorCode === undefined ? 'Error' : 'Error: ' + error.errorCode} variant="danger">
              <pre>{error.description}</pre>
              {error.body?.response?.supplementalInfo && (
                <pre>{JSON.stringify(error.body.response.supplementalInfo)}</pre>
              )}
            </Alert>
          )}
        </CardBody>
      </Card>
      {zones !== undefined && view !== undefined && (
        <>
          <br />
          <Card>
            <CardBody>
              <MapContainer
                key={responseKey}
                mode="Exclusion"
                geoJson={zones}
                styles={mapStyles}
                center={view.center}
                zoom={view.zoom}
                versionId={mapVersion}
              />
              <Table
                aria-label="Exclusion zones"
                variant={TableVariant.compact}
                caption={
                  zoneRows.length > 0
                    ? 'Select a zone to highlight it on the map'
                    : 'No exclusion zones overlap the area of intended operation'
                }
                cells={zoneColumns}
                rows={zoneRows}
              >
                <TableHeader />
                <TableBody onRowClick={(_: any, row: any) => selectZone(row.index)} />
              </Table>
            </CardBody>
          </Card>
        </>
      )}
      <br />
      <JsonRawDisp value={rawJson} />
    </PageSection>
  );
};
