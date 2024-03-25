import React from 'react';
import { Button } from '@patternfly/react-core';
import { GeoJson } from '../Lib/RatApiTypes';
import { guiConfig } from '../Lib/RatApi';

/**
 * React component that renders a button which loads RAS Bounds onto Google Maps feature list
 */
class LoadRasBounds extends React.Component<{ currentGeoJson: GeoJson; onLoad: (data: GeoJson) => void }> {
  constructor(props: Readonly<{ currentGeoJson: GeoJson; onLoad: (data: GeoJson) => void }>) {
    super(props);
  }

  downloadFile = () => {
    fetch(guiConfig.ras_bounds).then(async (response) => {
      const data: GeoJson = await response.json();
      let newJson: GeoJson = JSON.parse(JSON.stringify(this.props.currentGeoJson));
      newJson.features.push(...data.features);
      this.props.onLoad(newJson);
    });
  };

  render() {
    return (
      <Button type="button" variant="secondary" onClick={this.downloadFile}>
        Show RAS Exclusion Zone
      </Button>
    );
  }
}

export default LoadRasBounds;
