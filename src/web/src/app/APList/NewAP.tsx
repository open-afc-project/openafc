import * as React from "react";
import { AccessPointModel, RatResponse } from "../Lib/RatApiTypes";
import { Gallery, GalleryItem, FormGroup, TextInput, Button, Alert, AlertActionCloseButton, FormSelect, FormSelectOption, InputGroup } from "@patternfly/react-core";
import { PlusCircleIcon } from "@patternfly/react-icons";
import { UserModel } from "../Lib/RatApiTypes";

/**
 * NewAP.tsx: Form for creating a new access point
 * author: Sam Smucny
 */

/**
 * Interface definition of `NewAP` properties
 */
interface NewAPProps {
    ownerId: number,
    /**
     * Used to populate Owner dropdown if user is an admin
     */
    users?: UserModel[],
    onAdd: (ap: AccessPointModel) => Promise<RatResponse<string>>
}

interface NewAPState {
    serialNumber?: string,
    certificationId?: string,
    model?: string,
    manufacturer?: string,
    messageType?: "danger" | "success",
    messageValue: string,
    userId?: number
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
            certificationId: undefined
        };
    }

    private submit() {
        if (!this.state.serialNumber) {
            this.setState({ messageType: "danger", messageValue: "Serial number must be specified" });
            return;
        }
        if (this.props.ownerId === 0 && !this.state.userId) {
            this.setState({ messageType: "danger", messageValue: "Owner must be specified" });
            return;
        }

        this.props.onAdd({
            id: 0,
            serialNumber: this.state.serialNumber,
            certificationId: this.state.certificationId,
            model: this.state.model,
            manufacturer: this.state.manufacturer,
            ownerId: this.props.ownerId || this.state.userId // default is owner, but if admin then use the userId from the form
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
        const certificationIdChange = (s?: string) => this.setState({ certificationId: s });
        const modelChange = (s?: string) => this.setState({ model: s });
        const manufacturerChange = (s?: string) => this.setState({ manufacturer: s });
        const setUserId = (s?: string) => s && this.setState({ userId: Number(s) })

        return (
            <>{
                this.state.messageType && 
                <Alert 
                    variant={this.state.messageType} 
                    title={this.state.messageValue} 
                    action={<AlertActionCloseButton onClose={this.hideAlert} />} 
                />
            }
            <br/>
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
                        <TextInput
                            type="text"
                            id="certification-id-form"
                            name="certification-id-form"
                            aria-describedby="certification-id-form-helper"
                            value={this.state.certificationId}
                            onChange={certificationIdChange}
                        />
                    </FormGroup>
                </GalleryItem>
                <GalleryItem>
                    <FormGroup
                        label="Model"
                        fieldId="model-form"
                    >
                        <TextInput
                            type="text"
                            id="model-form"
                            name="model-form"
                            aria-describedby="model-form-helper"
                            value={this.state.model}
                            onChange={modelChange}
                        />
                    </FormGroup>
                </GalleryItem>
                <GalleryItem>
                    <FormGroup
                        label="Manufacturer"
                        fieldId="manufacturer-form"
                    >
                        <TextInput
                            type="text"
                            id="manufacturer-form"
                            name="manufacturer-form"
                            aria-describedby="manufacturer-form-helper"
                            value={this.state.manufacturer}
                            onChange={manufacturerChange}
                        />
                    </FormGroup>
                </GalleryItem>
                {this.props.ownerId === 0 && this.props.users &&
                    <GalleryItem>
                    <FormGroup
                        label="Owner"
                        fieldId="owner-form"
                        isRequired={true}
                    >
                        <InputGroup>
                        <FormSelect
                            value={this.state.userId}
                            onChange={setUserId}
                            id="owner-form"
                            name="owner-form"
                            style={{ textAlign: "right" }}
                        >
                        <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select an owner" />
                        {this.props.users.map((option: UserModel) => (
                        <FormSelectOption key={option.id} value={option.id} label={option.email} />
                        ))}
                    </FormSelect>
            </InputGroup>
                    </FormGroup>
                    </GalleryItem>
                }
                <GalleryItem>
                    <Button variant="primary" icon={<PlusCircleIcon />} onClick={() => this.submit()}>Add</Button>
                </GalleryItem>
            </Gallery></>
        )
    }
}