import * as React from "react";
import { PAWSForm } from "./PAWSForm";
import { SpectrumDisplayPAWS } from "../Components/SpectrumDisplay";
import { Card, CardBody, Expandable, Alert, Modal, Button, ClipboardCopy, ClipboardCopyVariant, AlertActionCloseButton } from "@patternfly/react-core";
import { PAWSRequest, PAWSResponse, ResError, error } from "../Lib/RatApiTypes";
import { PAWSAvailableSpectrum } from "../Lib/PawsApi";
import { JsonRawDisp } from "../Components/JsonRawDisp";
import { Timer } from "../Components/Timer";
import { getCacheItem, cacheItem } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";

// PAWSInterface.tsx: Form and data display for virtual ap
// author: Sam Smucny

/**
 * Test data
 */
const test = {
    showParams: true,
    dimensions: { width: 0, height: 0 },
    response: {
        "deviceDesc": {
            "serialNumber": "r"
        },
        "spectrumSpecs": [
            {
                "rulesetInfo": {
                    "authority": "US",
                    "rulesetId": "AFC-6GHZ-DEMO-1.0"
                },
                "spectrumSchedules": [
                    {
                        "eventTime": {
                            "startTime": "2019-07-30T19:53:16Z",
                            "stopTime": "2019-07-31T19:53:16Z"
                        },
                        "spectra": [
                            {
                                "profiles": [
                                    [
                                        {
                                            "dbm": 40,
                                            "hz": 6022000000
                                        },
                                        {
                                            "dbm": 41,
                                            "hz": 6030000000
                                        },
                                        {
                                            "dbm": 31,
                                            "hz": 6033000000
                                        },
                                        {
                                            "dbm": 30,
                                            "hz": 6045000000
                                        },
                                        {
                                            "dbm": 30,
                                            "hz": 6050000000
                                        }
                                    ]
                                ],
                                "resolutionBwHz": 30000000
                            },
                            {
                                "resolutionBwHz": 20000000,
                                "profiles": [[
                                    {
                                        "hz": 6022000000,
                                        "dbm": 20
                                    },
                                    {
                                        "hz": 6042000000,
                                        "dbm": 35
                                    },
                                    {
                                        "hz": 6062000000,
                                        "dbm": 40
                                    }
                                ],
                                [
                                    {
                                        "hz": 6082000000,
                                        "dbm": 30
                                    },
                                    {
                                        "hz": 6102000000,
                                        "dbm": 20
                                    }
                                ]
                                ]
                            }
                        ]
                    }
                ]
            },
            {
                rulesetInfo: { authority: "US", rulesetId: "Some other ruleset" },
                spectrumSchedules: [
                    {
                        eventTime: { startTime: "9:00AM", stopTime: "9:00PM" },
                        spectra: [
                            {
                                resolutionBwHz: 80000000,
                                profiles: [[
                                    { hz: 6050000000, dbm: 100 },
                                    { hz: 6090000000, dbm: 50 }
                                ]]
                            }
                        ]
                    }
                ],
            }
        ],
        "timestamp": "2019-07-30T19:53:16Z"
    } as unknown as PAWSResponse
};

export class PAWSInterface extends React.Component<{}, { 
    response?: PAWSResponse, 
    showParams: boolean, 
    status: "None" | "Error" | "Info", 
    err?: ResError, 
    isModalOpen: boolean,
    extraWarning?: string,
    extraWarningTitle?: string
}> {

    private paramValue?: PAWSRequest;
    private formUpdateCallback?: (val: PAWSRequest) => void

    constructor(props: {}) {
        super(props);

        this.state = { 
            showParams: true, 
            status: "None", 
            isModalOpen: false,
            extraWarning: undefined,
            extraWarningTitle: undefined
        };

    }

    componentDidMount() {
        const st = getCacheItem("pawsInterfaceCache");
        if (st !== undefined)
            this.setState(st);
    }

    componentWillUnmount() {
        // before removing object, let's cache the state in case we want to come back
        const state: {
            response?: PAWSResponse | undefined;
            showParams: boolean;
            status: string;
            err?: ResError | undefined;
        } = this.state;
        state.status = "None";
        state.err = undefined;
        cacheItem("pawsInterfaceCache", state);
    }

    private sendRequest(data: PAWSRequest) {
        logger.info(data);

         // check AGL settings and possibly truncate
        if (data.antenna.heightType === "AGL" && data.antenna.height - data.antenna.heightUncertainty < 1) {
            // modify if height is not 1m above terrain height
            const minHeight = 1;
            const maxHeight = data.antenna.height + data.antenna.heightUncertainty;
            if (maxHeight < minHeight) {
                this.setState({
                    status: "Error",
                    err: error(`The height value must allow the AP to be at least 1m above the terrain. Currently the maximum height is ${maxHeight}m`)
                });
                return;
            }
            const newHeight = (minHeight + maxHeight) / 2;
            const newUncertainty = newHeight - minHeight;
            data.antenna.height = newHeight;
            data.antenna.heightUncertainty = newUncertainty;
            logger.warn("Height was not at least 1 m above terrain, so it was truncated to fit AFC requirement");
            this.setState({ extraWarningTitle: "Truncated Height", extraWarning: `The AP height has been truncated so that its minimum height is 1m above the terrain. The new height is ${newHeight}+/-${newUncertainty}m`});
        }

        this.setState({ status: "Info" });
        return PAWSAvailableSpectrum(data)
            .then(result => {
                if (result.kind === "Success") {
                    logger.info(result.result);
                    this.setState({ response: result.result, status: "None" });
                    return;
                } else {
                    logger.error(result);
                    this.setState({ status: "Error", err: result });
                    return;
                }
            }).catch(e => {
                this.setState({ status: "Error", err: error(e.message, undefined, e) })
                logger.error(e);
                return;
            })
    }

    private setConfig(value: string) {
        try {
          const params: PAWSRequest = JSON.parse(value) as PAWSRequest;
          if (this.formUpdateCallback)
            this.formUpdateCallback(params);
        } catch (e) {
          logger.error("Pasted value is not valid JSON");
        }
      }
    
      private copyPaste(formData: PAWSRequest, updateCallback: (v: PAWSRequest) => void) {
        this.formUpdateCallback = updateCallback;
        this.paramValue = formData;
        this.setState({isModalOpen: true});
      }
    
      private getParamsText = () => this.paramValue ? JSON.stringify(this.paramValue) : "";

    render() {

        const toggleParams = () => this.setState({ showParams: !this.state.showParams });

        return (
            <>
                <Card>
                    <Modal
                        title="Copy/Paste"
                        isLarge={true}
                        isOpen={this.state.isModalOpen}
                        onClose={() => this.setState({ isModalOpen: false })}
                        actions={[<Button key="update" variant="primary" onClick={() => this.setState({ isModalOpen: false })}>Close</Button>]}>
                        <ClipboardCopy variant={ClipboardCopyVariant.expansion} onChange={(v: string) => this.setConfig(v)} aria-label="text area">{this.getParamsText()}</ClipboardCopy>
                    </Modal>
                    <CardBody>
                        <Expandable toggleText={this.state.showParams ? "Hide parameters" : "Show parameters"} onToggle={toggleParams} isExpanded={this.state.showParams}>
                            <PAWSForm parentName="pawsInterface" onSubmit={x => this.sendRequest(x)} onCopyPaste={(formData: PAWSRequest, updateCallback: (v: PAWSRequest) => void) => this.copyPaste(formData, updateCallback)} />
                        </Expandable>
                    </CardBody>
                </Card>
                <br />
                {
                    this.state.extraWarning &&
                    <Alert title={this.state.extraWarningTitle || "Warning"} variant="warning" action={<AlertActionCloseButton onClose={() => this.setState({ extraWarning: undefined, extraWarningTitle: undefined })} />}>
                        <pre>{this.state.extraWarning}</pre>
                    </Alert>
                }
                {
                    this.state.status === "Info" &&
                    <Alert title={"Processing"} variant="info">
                        {"Your request has been submitted. "}
                        <Timer />
                    </Alert>
                }
                {
                    this.state.status === "Error" &&
                    <Alert title={"Error: " + this.state.err?.errorCode} variant="danger">
                        <pre>{this.state.err?.description}</pre>
                    </Alert>
                }
                <br />
                <SpectrumDisplayPAWS spectrum={this.state.response} />
                <br />
                <JsonRawDisp value={this.state.response} />
            </>
        )
    }

}
