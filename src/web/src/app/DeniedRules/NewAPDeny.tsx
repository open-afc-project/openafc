import * as React from "react";
import { AccessPointModel, RatResponse } from "../Lib/RatApiTypes";
import { Gallery, GalleryItem, FormGroup, TextInput, Button, Alert, AlertActionCloseButton, FormSelect, FormSelectOption, InputGroup } from "@patternfly/react-core";
import { PlusCircleIcon } from "@patternfly/react-icons";
import { UserModel } from "../Lib/RatApiTypes";
import { hasRole} from "../Lib/User";

/**
 * NewAPDeny.tsx: Form for creating a new access point
 * author: Sam Smucny
 */

/**
 * Interface definition of `NewAPDeny` properties
 */
interface NewAPDenyProps {
    org: string,
    rulesetIds: string[],
    onAdd: (ap: AccessPointModel) => Promise<RatResponse<string>>
}

interface NewAPDenyState {
    serialNumber?: string,
    certificationId?: string,
    messageType?: "danger" | "success",
    messageValue: string,
    org: string,
    rulesetId: string,
}

/**
 * Component with form for user to register a new access point.
 * 
 */
export class NewAPDeny extends React.Component<NewAPDenyProps, NewAPDenyState> {
    constructor(props: NewAPDenyProps) {
        super(props);
        this.state = {
            messageValue: "",
            serialNumber: "",
            certificationId: "",
            org: this.props.org,
            rulesetId: "",
        };
    }

    private setRulesetId = (n: string) => this.setState({rulesetId:n});
 

    private submit() {
        if (!this.state.serialNumber) {
            this.setState({ messageType: "danger", messageValue: "Enter * for serial number wildcard" });
            return;
        }

        if (!this.state.org) {
            this.setState({ messageType: "danger", messageValue: "Org must be specified" });
            return;
        }

        if (!this.state.certificationId) {
            this.setState({ messageType: "danger", messageValue: "certification id must be specified" });
            return;
        }

        if (!this.state.rulesetId) {
            this.setState({ messageType: "danger", messageValue: "rulesetId must be specified" });
            return;
        }

        this.props.onAdd({
            id: 0,
            serialNumber: this.state.serialNumber,
            certificationId: this.state.certificationId,
            org: this.state.org,
            rulesetId: this.state.rulesetId,
        })
            .then(res => {
                if (res.kind === "Success") {
                    this.setState((s: NewAPDenyState) => ({ messageType: "success", messageValue: "Added Deny AP serial " + s.serialNumber + " Cert ID " + s.certificationId}));
                } else {
                    this.setState({ messageType: "danger", messageValue: res.description });
                }
            })
    }

    private hideAlert = () => this.setState({ messageType: undefined });

    render() {

        const serialNumberChange = (s?: string) => this.setState({ serialNumber: s });
        const certificationIdChange = (s?: string) => this.setState({ certificationId: s });
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
                            <FormGroup label="Ruleset" fieldId="horizontal-form-ruleset">
                                <FormSelect
                                    value={this.state.rulesetId}
                                    onChange={x => this.setRulesetId(x)}
                                    id="horizontal-form-ruleset"
                                    name="horizontal-form-ruleset"
                                    isValid={!!this.state.rulesetId}
                                    style={{ textAlign: "right" }}
                                >
                                    <FormSelectOption key={undefined} value={undefined} label="Select a Ruleset" />
                                    {this.props.rulesetIds.map((option: string) => (
                                        <FormSelectOption key={option} value={option} label={option} />
                                    ))}
                                </FormSelect>
                            </FormGroup>
                    </GalleryItem>
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
                                    label="Id"
                                    value={this.state.certificationId}
                                    onChange={certificationIdChange}
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
