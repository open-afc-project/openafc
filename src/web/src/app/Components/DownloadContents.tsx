import React from 'react';
import { Button } from '@patternfly/react-core';

/**
 * React component that renders a button which can be used to download a file from the server
 * @param contents Binary data to download
 * @param fileName Human readable name file will be given when downloaded
 */
class DownloadContents extends React.Component<{ contents: () => Blob; fileName: string }> {
  constructor(props: Readonly<{ contents: () => Blob; fileName: string }>) {
    super(props);
  }

  downloadFile = () => {
    var data = this.props.contents();
    const url = window.URL.createObjectURL(new Blob([data], { type: 'application/vnd' }));
    const a = document.createElement('a');
    a.href = url;
    a.download = this.props.fileName;
    a.click();
  };

  render() {
    return (
      <Button type="button" onClick={this.downloadFile}>
        Download {this.props.fileName}
      </Button>
    );
  }
}

export default DownloadContents;
