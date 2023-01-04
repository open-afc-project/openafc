import * as React from "react";
import { AccessPointModel, RatResponse } from "../Lib/RatApiTypes";
import { Gallery, GalleryItem, FormGroup, TextInput, Button, Alert, AlertActionCloseButton, FormSelect, FormSelectOption, InputGroup } from "@patternfly/react-core";
import { PlusCircleIcon } from "@patternfly/react-icons";
import { UserModel } from "../Lib/RatApiTypes";
import { hasRole} from "../Lib/User";

/**
 * NewAP.tsx: Form for creating a new access point
 * author: Sam Smucny
 */

/**
 * Interface definition of `NewAP` properties
 */
interface NewAPProps {
    userId: number,
    org: string,
    onAdd: (ap: AccessPointModel) => Promise<RatResponse<string>>
}

interface NewAPState {
    serialNumber?: string,
    certificationIdNra?: string,
    certificationIdId?: string,
    model?: string,
    manufacturer?: string,
    messageType?: "danger" | "success",
    messageValue: string,
    org: string,
}

/**
 * Component with form for user to register a new access point.
 * 
 */
export class NewAP extends React.Component<NewAPProps, NewAPState> {
    constructor(props: NewAPProps) {
        super(props);
        this.state = {
            messageValue: "",
            serialNumber: "",
            model: undefined,
            manufacturer: undefined,
            certificationIdNra: undefined,
            certificationIdId: undefined,
            org: this.props.org,
        };
    }

    private submit() {
        if (this.state.certificationIdNra.toLowerCase() !== "fcc") {
            this.setState({ messageType: "danger", messageValue: "Invalid NRA"});
            return;
        }

        if (!this.state.serialNumber) {
            this.setState({ messageType: "danger", messageValue: "Serial number must be specified" });
            return;
        }
        if (!this.state.org === 0) {
            this.setState({ messageType: "danger", messageValue: "Org must be specified" });
            return;
        }

        this.props.onAdd({
            id: 0,
            serialNumber: this.state.serialNumber,
            certificationId: this.state.certificationIdNra + " " + this.state.certificationIdId,
            model: this.state.model,
            manufacturer: this.state.manufacturer,
            org: this.state.org,
        })
            .then(res => {
                if (res.kind === "Success") {
                    this.setState((s: NewAPState) => ({ messageType: "success", messageValue: "Added " + s.serialNumber }));
                } else {
                    this.setState({ messageType: "danger", messageValue: res.description });
                }
            })
    }

    private hideAlert = () => this.setState({ messageType: undefined });

    render() {

        const serialNumberChange = (s?: string) => this.setState({ serialNumber: s });
        const certificationIdNraChange = (s?: string) => this.setState({ certificationIdNra: s });
        const certificationIdIdChange = (s?: string) => this.setState({ certificationIdId: s });
        const modelChange = (s?: string) => this.setState({ model: s });
        const manufacturerChange = (s?: string) => this.setState({ manufacturer: s });
        const orgChange = (s?: string) => this.setState({ org: s });

        return (
            <>{
                this.state.messageType &&
                <Alert
                    variant={this.state.messageType}
                    title={this.state.messageValue}
                    action={<AlertActionCloseButton onClose={this.hideAlert} />}
                />
            }
                <br />
                <Gallery gutter="sm">
                    <GalleryItem>
                        <FormGroup
                            label="Serial Number"
                            isRequired={true}
                            fieldId="serial-number-form"
                        >
                            <TextInput
                                type="text"
                                id="serial-number-form"
                                name="serial-number-form"
                                aria-describedby="serial-number-form-helper"
                                value={this.state.serialNumber}
                                onChange={serialNumberChange}
                            />
                        </FormGroup>
                    </GalleryItem>
                    <GalleryItem>
                        <FormGroup
                            label="Certification ID"
                            fieldId="certification-id-form"
                        >
                            <div>
                                <TextInput
                                    label="NRA"
                                    value={this.state.certificationIdNra}
                                    onChange={certificationIdNraChange}
                                    type="text"
                                    step="any"
                                    id="horizontal-form-certification-nra"
                                    name="horizontal-form-certification-nra"
                                    style={{ textAlign: "left" }}
                                    placeholder="NRA"
                                />

                                <TextInput
                                    label="Id"
                                    value={this.state.certificationIdId}
                                    onChange={certificationIdIdChange}
                                    type="text"
                                    step="any"
                                    id="horizontal-form-certification-list"
                                    name="horizontal-form-certification-list"
                                    style={{ textAlign: "left" }}
                                    placeholder="Id"
                                />
                            </div>
                        </FormGroup>
                    </GalleryItem>
                  
                    {hasRole("Super") &&
                        (<GalleryItem>
                            <FormGroup
                                label="Org"
                                isRequired={true}
                                fieldId="org-form"
                            >
                                <TextInput
                                    type="text"
                                    id="org-form"
                                    name="org-form"
                                    aria-describedby="org-form-helper"
                                    value={this.state.org}
                                    onChange={orgChange}
                                />
                            </FormGroup>
                        </GalleryItem>)
                    }

                    <GalleryItem>
                        <Button variant="primary" icon={<PlusCircleIcon />} onClick={() => this.submit()}>Add</Button>
                    </GalleryItem>
                </Gallery></>
        )
    }
}
