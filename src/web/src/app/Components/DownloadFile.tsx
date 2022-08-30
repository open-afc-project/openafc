import React from "react";
import { Button } from "@patternfly/react-core";

/**
 * React component that renders a button which can be used to download a file from the server
 * @param url URL to resource to download
 * @param fileName Human readable name file will be given when downloaded
 */
class DownloadFile extends React.Component<{ url: string, fileName: string }> {

    constructor(props: Readonly<{ url: string; fileName: string; }>) {
        super(props);
    }

    downloadFile = () => {
        fetch(this.props.url)
            .then(response => {
                response.blob().then(blob => {
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement("a");
                    a.href = url;
                    a.download = this.props.fileName;
                    a.click();
                });
            });
    }

    render() {
        return (
            <Button type="button" onClick={this.downloadFile}>Download</Button>
        )
    }

}

export default DownloadFile;
