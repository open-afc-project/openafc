import * as React from "react";
import { AFCConfigFile, RatResponse } from "../Lib/RatApiTypes";
import { CardBody, PageSection, Card, CardHead, TextInput, Alert, AlertActionCloseButton } from "@patternfly/react-core";
import DownloadContents from "../Components/DownloadContents";
import { exportCache, putAfcConfigFile, importCache, guiConfig } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";

export class ImportExport extends React.Component<{ afcConfig: RatResponse<AFCConfigFile> }, { message?: string, messageType?: "danger" | "success" }> {
    

    constructor(props: Readonly<{ afcConfig: RatResponse<AFCConfigFile>; }>) {
        super(props);

        if (props.afcConfig.kind === "Success") {
            this.state = {}
        } else {
            logger.error("AFC Config was not loaded", props.afcConfig);
            this.state = { messageType: "danger", message: "AFC Config was not loaded" };
        }
    }

    private export = () =>
        new Blob([JSON.stringify(Object.assign(exportCache(), this.props.afcConfig.kind === "Success" ? { afcConfig: this.props.afcConfig.result } : {}))], {
            type: "application/json"
        });

    private import = async (name: string, ev: React.FormEvent<HTMLInputElement>) => {
        // @ts-ignore
        const file = ev.target.files[0];
        const reader = new FileReader();
        try {
            reader.onload = async () => {
                try {
                    const value: any = JSON.parse(reader.result as string);
                    if (value.afcConfig) {
                        if (value.afcConfig.version !== guiConfig.version) {
                            const warning: string = "The imported file is from a different version. It has version '" 
                                + value.afcConfig.version 
                                + "', and you are currently running '" 
                                + guiConfig.version 
                                + "'.";
                            logger.warn(warning);
                        }
                        const putResp = await putAfcConfigFile(value.afcConfig);
                        if (putResp.kind === "Error") {
                            this.setState({ messageType: "danger", message: putResp.description });
                            return;
                        }
                    }

                    value.afcConfig = undefined;

                    importCache(value);

                    this.setState({ messageType: "success", message: "Import successful!" });
                } catch (e) {
                    this.setState({ messageType: "danger", message: "Unable to import file"})
                }
            }

            reader.readAsText(file);

       } catch (e) {
            logger.error("Failed to import application state", e);
            this.setState({ messageType: "danger", message: "Failed to import application state" });
        }
    }

    render() {
        return (
        <PageSection>
            <Card>
                <CardHead>
                    Import
                </CardHead>
                <CardBody>
                    <br/>
                    <TextInput
                        // @ts-ignore
                        type="file"
                        id="file-import"
                        name="file"
                        // @ts-ignore
                        onChange={(f, ev) => this.import(f, ev)}
                        />
                        {this.state.messageType && (<><br/><br/>
                            <Alert
                                variant={this.state.messageType}
                                title={this.state.messageType.toUpperCase}
                                action={<AlertActionCloseButton onClose={() => this.setState({ messageType: undefined })} />}
                            >
                                <pre>{this.state.message}</pre>
                            </Alert>
                        </>)}
                </CardBody>
            </Card>
            <br/>
            <Card>
                <CardHead>
                    Export
                </CardHead>
                <CardBody>
                    <DownloadContents fileName="RLAN_AFC_Tool.json" contents={() => this.export()} />
                    <br/>
                </CardBody>
            </Card>
        </PageSection>);
    }
}