import * as React from "react";
import { MTLSModel, RatResponse } from "../Lib/RatApiTypes";
import { Gallery, GalleryItem, FormGroup, TextInput, Button, Alert, AlertActionCloseButton, FormSelect, FormSelectOption, InputGroup } from "@patternfly/react-core";
import { PlusCircleIcon } from "@patternfly/react-icons";
import { UserModel } from "../Lib/RatApiTypes";
import { getUser} from "../Lib/Admin";
import { hasRole} from "../Lib/User";

/**
 * NewMTLS.tsx: Form for creating a new MTLS certificate
 * author: Sam Smucny
 */

/**
 * Interface definition of `NewMTLS` properties
 */
interface NewMTLSProps {
    userId: number,
    /**
     * Used to populate Owner dropdown if user is an admin
     */
    users?: UserModel[],
    onAdd: (mtls: MTLSModel) => Promise<RatResponse<string>>
}

interface NewMTLSState {
    note: string,
    cert: string,
    messageType?: "danger" | "success",
    messageValue: string,
    org?: string
}

/**
 * Component with form for user to register a new mtls certificate.
 * 
 */
export class NewMTLS extends React.Component<NewMTLSProps, NewMTLSState> {
    constructor(props: NewMTLSProps) {
        super(props);
        this.state = {
            cert: "",
            note: "",
            org: ""
        }
        getUser(props.userId)
        .then(res => res.kind === "Success" ?
            this.setState({
                org: res.result.org,
             } as NewMTLSState) :
            this.setState({
                messageType: "danger",
                messageValue: res.description,
                org: ""
            }))
    }

    private submit() {
        if (!this.state.org === 0) {
            this.setState({ messageType: "danger", messageValue: "Org must be specified" });
            return;
        }

        this.props.onAdd({
            id: 0,
            cert: this.state.cert,
            note: this.state.note,
            org: this.state.org,
            created: ""
        })
            .then(res => {
                if (res.kind === "Success") {
                    this.setState((s: NewMTLSState) => ({ messageType: "success", messageValue: "Added with Note " + s.note }));
                } else {
                    this.setState({ messageType: "danger", messageValue: res.description });
                }
            })
    }

    private hideAlert = () => this.setState({ messageType: undefined });

    fileChange(e) {
        let files = e.target.files;
        let reader = new FileReader();
        reader.readAsDataURL(files[0]);
        reader.onload=(e)=>{
            console.warn("data file",e.target.result);
            this.setState({cert: e.target.result});
        }
    }

    render() {
        const noteChange = (s?: string) => this.setState({ note: s });
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
                            label="CertificateBrowser"
                            isRequired={true}
                            fieldId="cert-browser-form"
                        >
                            <input type="file" name="certificate file" onChange={(e) => this.fileChange(e)} />
                        </FormGroup>
                    </GalleryItem>
                    <br />

                    <GalleryItem>
                        <FormGroup
                            label="Note"
                            isRequired={true}
                            fieldId="note-form"
                        >
                            <TextInput
                                type="text"
                                id="note-form"
                                name="note-form"
                                aria-describedby="note-form-helper"
                                value={this.state.note}
                                onChange={noteChange}
                            />
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
