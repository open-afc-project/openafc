import * as React from "react";
import {
    PageSection,
    Title,
    Card,
    CardHeader,
    CardBody,
    FormGroup,
    FormSelect,
    FormSelectOption,
    Button,
    Alert,
    AlertActionCloseButton
} from "@patternfly/react-core";
import { RatResponse } from "../Lib/RatApiTypes";
import { logger } from "../Lib/Logger";
import { ulsFileConvert } from "../Lib/RatApi";
import { Timer } from "../Components/Timer";

/**
 * Convert.tsx: Page where use can convert files on server
 * author: Sam Smucny
 */

 /**
  * Interface definition of `Convert` state
  */
interface ConvertState {
    ulsSelect?: string,
    ulsFilesCsv: string[],
    ulsMessageType?: "info" | "warning" | "danger" | "success",
    ulsMessage?: string
}

/**
 * Page level component that allows a user to convert file formats
 */
class Convert extends React.Component<{ 

    /**
     * List of csv files that can be converted
     */
    ulsFilesCsv: RatResponse<string[]>, 
}, ConvertState> {

    constructor(props: Readonly<{ ulsFilesCsv: RatResponse<string[]>; }>){
        super(props);

        if (props.ulsFilesCsv.kind === "Error") {
            throw new Error("Uls files could not be retreived")
        }

        this.state = {
            ulsFilesCsv: props.ulsFilesCsv.result
        };

    }

    private updateUls(s: string | undefined) {
        if (s) {
            this.setState({ ulsSelect: s })
        }
    }

    private convertUls() {
        if (this.state.ulsSelect) {
            logger.info("converting uls file", this.state.ulsSelect);
            this.setState({ ulsMessageType: "info", ulsMessage: "processing..." });
            ulsFileConvert(this.state.ulsSelect).then(res => {
                if (res.kind === "Success") {
                    if (res.result.invalidRows > 0) {
                        this.setState({ 
                            ulsMessageType: "warning", 
                            ulsMessage: String(res.result.invalidRows) + " invalid rows in processing:\n\n" + res.result.errors.join("\n") });
                    } else {
                        this.setState({ ulsMessage: "Conversion successful", ulsMessageType: "success" });
                    }
                } else {
                    this.setState({ ulsMessage: res.description, ulsMessageType: "danger" });
                }
            })
        } else {
            this.setState({ ulsMessage: "Select a file", ulsMessageType: "danger" });
        }
    }

    render() {

        return (
            <PageSection>
                <Title size={"lg"}>File Conversion</Title>
                <Card>
                    <CardHeader>ULS Conversion</CardHeader>
                    <CardBody>
                        <FormGroup label="ULS File" fieldId="horizontal-form-uls-db">
                            <FormSelect
                                value={this.state.ulsSelect}
                                onChange={(s: string | undefined) => this.updateUls(s)}
                                id="horizontal-form-uls-db"
                                name="horizontal-form-uls-db"
                                style={{ textAlign: "right" }}
                            >
                                <FormSelectOption isDisabled={true} key={undefined} value={undefined} label="Select a ULS file to convert" />
                                {this.state.ulsFilesCsv.map((option: string) => (
                                    <FormSelectOption key={option} value={option} label={option} />
                                ))}
                            </FormSelect>
                        </FormGroup>
                        <br />
                        <>
                            {this.state.ulsMessageType && (
                                <Alert
                                    variant={this.state.ulsMessageType}
                                    title={this.state.ulsMessageType.toUpperCase()}
                                    action={this.state.ulsMessageType !== "info" && <AlertActionCloseButton onClose={() => this.setState({ ulsMessageType: undefined })} />}
                                >{this.state.ulsMessage}
                                    {this.state.ulsMessageType === "info" && <Timer />}
                                </Alert>
                            )}
                        </>
                        <br/>
                        <Button variant="primary" isDisabled={!this.state.ulsSelect || this.state.ulsMessageType === "info" } onClick={() => this.convertUls()}>Convert</Button>
                    </CardBody>
                </Card>
            </PageSection>
        );
    }
}

export { Convert };