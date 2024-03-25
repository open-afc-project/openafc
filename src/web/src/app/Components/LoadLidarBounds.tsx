import React from 'react';
import { Button } from '@patternfly/react-core';
import { GeoJson } from '../Lib/RatApiTypes';
import { guiConfig } from '../Lib/RatApi';

/**
 * React component that renders a button which loads LiDAR Bounds onto Google Maps feature list
 */
class LoadLidarBounds extends React.Component<{ currentGeoJson: GeoJson; onLoad: (data: GeoJson) => void }> {
  constructor(props: Readonly<{ currentGeoJson: GeoJson; onLoad: (data: GeoJson) => void }>) {
    super(props);
  }

  downloadFile = () => {
    fetch(guiConfig.lidar_bounds).then(async (response) => {
      const data: GeoJson = await response.json();
      let newJson: GeoJson = JSON.parse(JSON.stringify(this.props.currentGeoJson));
      newJson.features.push(...data.features);
      this.props.onLoad(newJson);
    });
  };

  render() {
    return (
      <Button type="button" variant="secondary" onClick={this.downloadFile}>
        Show LiDAR Bounds
      </Button>
    );
  }
}

export default LoadLidarBounds;
