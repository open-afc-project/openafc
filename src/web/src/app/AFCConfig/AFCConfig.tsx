import * as React from "react";
import {
    PageSection,
    Title,
    Alert,
} from "@patternfly/react-core";
import { AFCForm } from "./AFCForm";
import { AFCConfigFile, FreqRange, RatResponse } from "../Lib/RatApiTypes";
import { getDefaultAfcConf, putAfcConfigFile, guiConfig } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { Limit } from "../Lib/Admin";
import {getLastUsedRegionFromCookie} from "../Lib/Utils"

/**
* AFCConfic.tsx: main component for afc config page
* author: Sam Smucny
*/

/**
 * Interface definition
 */
interface AFCState {
    config: AFCConfigFile,
    ulsFiles: string[],
    antennaPatterns: string[],
    regions: string[],
    messageType?: "Warn",
    messageTitle?: string,
    messageValue?: string,
    isModalOpen?: boolean
}

/**
 * Page level component for AFC Config
 * @param ulsFiles Injected dependency
 * @param afcConf Injected Config from server
 * @param antennaPatterns Injected dependency
 */
class AFCConfig extends React.Component<{
    limit: RatResponse<Limit>,
    ulsFiles: RatResponse<string[]>,
    afcConf: RatResponse<AFCConfigFile>,
    antennaPatterns: RatResponse<string[]>,
    regions: RatResponse<string[]>,
    frequencyBands: RatResponse<FreqRange[]>
}, AFCState> {

    constructor(props: Readonly<{ limit: RatResponse<Limit>; frequencyBands: RatResponse<FreqRange[]>; ulsFiles: RatResponse<string[]>; afcConf: RatResponse<AFCConfigFile>; antennaPatterns: RatResponse<string[]>; regions: RatResponse<string[]>}>) {
        super(props);
        //@ts-ignore
        var lastRegFromCookie = getLastUsedRegionFromCookie();

        const state: AFCState = {
            config: getDefaultAfcConf(lastRegFromCookie), isModalOpen: false, messageValue: "", messageTitle: "",
            ulsFiles: [],
            antennaPatterns: [],
            regions: []
        };

        if (props.afcConf.kind === "Success") {
            let incompatible = false;
            // if stored configurations do not have fields present in default, it's incompatible.
            if (props.afcConf.result.version !== guiConfig.version) {
                for ( var p in state.config) {
                    if ( ! state.config.hasOwnProperty( p ) ) continue;
                    if ( ! props.afcConf.result.hasOwnProperty( p ) ) {
                        incompatible = true;
                        logger.error("Could not load most recent AFC Config Defaults.",
                            "Missing parameters. Current version " + guiConfig.version + " Incompatible version " + props.afcConf.result.version);
                        Object.assign(state, {
                            config: getDefaultAfcConf(lastRegFromCookie),
                            messageType: "Warn",
                            messageTitle: "Invalid Config Version",
                            messageValue: "The current version (" + guiConfig.version + ") is not compatible with the loaded configuration. The loaded configuration was created for version " + props.afcConf.result.version + ". The default configuration has been loaded instead. To resolve this AFC Config will need to be updated below.",
                        });
                        break;
                    }
                }
            }

            if (! incompatible) {
                Object.assign(state, { config: props.afcConf.result });
                state.config.version = guiConfig.version
            }
        } else {
            logger.error("Could not load most recent AFC Config Defaults.",
                "error code: ", props.afcConf.errorCode,
                "description: ", props.afcConf.description)
            Object.assign(state, { config: getDefaultAfcConf(lastRegFromCookie) });
        }

        if (props.ulsFiles.kind === "Success") {
            Object.assign(state, { ulsFiles: props.ulsFiles.result });
        } else {
            logger.error("Could not load ULS Database files",
                "error code: ", props.ulsFiles.errorCode,
                "description: ", props.ulsFiles.description)
            Object.assign(state, { ulsFiles: [] });
        }

        if (props.antennaPatterns.kind === "Success") {
            Object.assign(state, { antennaPatterns: props.antennaPatterns.result });
        } else {
            logger.error("Could not load antenna pattern files",
                "error code: ", props.antennaPatterns.errorCode,
                "description: ", props.antennaPatterns.description)
            Object.assign(state, { antennaPatterns: [] });
        }

        if (props.regions.kind === "Success") {
            Object.assign(state, { regions: props.regions.result });
        } else {
            logger.error("Could not load regions",
                "error code: ", props.regions.errorCode,
                "description: ", props.regions.description)
            Object.assign(state, { regions: [] });
        }


        this.state = state;
    }

    private submit = (conf: AFCConfigFile): Promise<RatResponse<string>> => (
        putAfcConfigFile(conf))
        .then(resp => {
            if (resp.kind === "Success") {
                this.setState({ messageType: undefined, messageTitle: "", messageValue: "" });
            }
            return resp;
        });

    render() {
        return (
            <PageSection>
                <Title size={"lg"}>AFC Configuration</Title>
                {
                    this.state.messageType === "Warn" &&
                    <Alert title={this.state.messageTitle} variant="warning">
                        <pre>{this.state.messageValue}</pre>
                    </Alert>
                }
                <AFCForm frequencyBands={this.props.frequencyBands.kind == "Success" ? this.props.frequencyBands.result : []} limit={this.props.limit.kind == "Success" ? this.props.limit.result : new Limit(false, 0)} config={this.state.config} ulsFiles={this.state.ulsFiles} antennaPatterns={this.state.antennaPatterns} regions={this.state.regions} onSubmit={(x) => this.submit(x)} />
            </PageSection>
        );
    }
}

export { AFCConfig };
