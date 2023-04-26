import * as React from "react";
import { DeniedRegion, error, success, RatResponse } from "../Lib/RatApiTypes";
import { getDeniedRegions, updateDeniedRegions } from "../Lib/Admin";
import { Card, CardHead, CardHeader, CardBody, PageSection, InputGroup, Select, SelectOption, FormSelect, FormSelectOption, Button, Modal, Alert, AlertActionCloseButton, GalleryItem, FormGroup } from "@patternfly/react-core";
import { NewDR } from "./NewDR";
import { DRTable } from "./DRTable";
import { logger } from "../Lib/Logger";
import { UserContext, UserState, isLoggedIn, hasRole } from "../Lib/User";
import { mapRegionCodeToName } from "../Lib/Utils";

/**
 * DRList.tsx: List of denied Regions (by regionStr) to add/remove
 * author: mgelman@rkf-eng.com
 */

/**
 * Interface definition of `DRList` properties
 */
export interface DRListProps {
    regions: string[]
}

interface DRListState {
    deniedRegions: DeniedRegion[],
    regionStr: string,
    allRegions: string[],
    deniedRegionsNeedSaving: boolean,
    showingSaveWarning: boolean,
    newRegionString?: string,
    messageSuccess?: string,
    messageError?: string,
    isEditorOpen: boolean
}

/**
 * Page level component to list a user's registerd access points. Users use these 
 * credentials to utilize the PAWS interface.
 */
export class DRList extends React.Component<DRListProps, DRListState> {
    constructor(props: DRListProps) {
        super(props);

        this.state = {
            deniedRegions: [],
            regionStr: "US",
            deniedRegionsNeedSaving: false,
            showingSaveWarning: false,
            allRegions: this.props.regions,
            isEditorOpen: false
        }
        this.loadCurrentDeniedRegions();
    }

    loadCurrentDeniedRegions = () => {
        getDeniedRegions(this.state.regionStr).then(res => {
            if (res.kind === "Success") {
                this.setState({ deniedRegions: res.result, deniedRegionsNeedSaving: false });
            }
        })
    }

    onAdd = (dr: DeniedRegion) => {
        this.setState({ deniedRegions: this.state.deniedRegions.concat(dr), deniedRegionsNeedSaving: true, isEditorOpen: false })

    }

    onEdit = (dr: DeniedRegion, prevName: string, prevZoneType: string) => {
        //Need to make changes here
        this.setState({ deniedRegions: this.state.deniedRegions.concat(dr), deniedRegionsNeedSaving: true })
    }

    deleteDR = (id: string) => {
        this.setState({
            deniedRegions: this.state.deniedRegions.filter(x => !(x.regionStr == this.state.regionStr && x.name + x.zoneType == id)),
            deniedRegionsNeedSaving: true
        })
    }

    setUlsRegion = (newRegion: string): void => {
        if (newRegion != this.state.regionStr) {
            if (this.state.deniedRegionsNeedSaving) {
                this.setState({ showingSaveWarning: true, newRegionString: newRegion })
            } else {
                this.setState({ regionStr: newRegion, deniedRegionsNeedSaving: false }, () => { this.loadCurrentDeniedRegions(); });

            }
        }
    }

    closeSaveWarning = () => {
        this.setState({ showingSaveWarning: false })
    }

    forceChangeRegion = () => {
        this.setState({
            showingSaveWarning: false, regionStr: this.state.newRegionString!,
            newRegionString: undefined, deniedRegionsNeedSaving: false
        }, () => { this.loadCurrentDeniedRegions(); });

    }

    putDeniedRegions = () => {
        updateDeniedRegions(this.state.deniedRegions.filter(x => x.regionStr == this.state.regionStr), this.state.regionStr)
            .then(res => {
                if (res.kind === "Success") {
                    this.setState({ messageSuccess: "Changes saved", deniedRegionsNeedSaving: false });
                } else {
                    logger.error("Could not save denied regions",
                        "error code: ", res.errorCode,
                        "description: ", res.description)
                    this.setState({ messageError: "Error saving " + res.description });
                }
            })
    }

    closeEditor = () => {
        this.setState({ isEditorOpen: false })
    }

    openEditor = () => {
        this.setState({ isEditorOpen: true })
    }


    render() {
        return (
            <Card>
                <CardHead><CardHeader>Denied Region</CardHeader></CardHead>
                <CardBody>
                    <Modal
                        title="Unsaved Changes"
                        isOpen={this.state.showingSaveWarning}
                        onClose={() => this.closeSaveWarning()}
                        actions={[
                            <Button key="confirm" variant="link" onClick={() => this.forceChangeRegion()}>
                                Confirm
                            </Button>,
                            <Button key="cancel" variant="primary" onClick={() => this.closeSaveWarning()}>
                                Cancel
                            </Button>
                        ]}
                    >
                        You have unsaved changes to the denied regions for the current country. Proceed and lose changes?
                    </Modal>
                    {this.state.messageError !== undefined && (
                        <Alert
                            variant="danger"
                            title="Error"
                            action={<AlertActionCloseButton onClose={() => this.setState({ messageError: undefined })} />}
                        >{this.state.messageError}</Alert>
                    )}

                    {this.state.messageSuccess !== undefined && (
                        <Alert
                            variant="success"
                            title="Success"
                            action={<AlertActionCloseButton onClose={() => this.setState({ messageSuccess: undefined })} />}
                        >{this.state.messageSuccess}</Alert>
                    )}
                    <Modal title="Add Denied Region"
                        isOpen={this.state.isEditorOpen}
                        onClose={() => this.closeEditor()}
                        actions={[

                            <Button key="cancel" variant="primary" onClick={() => this.closeEditor()}>
                                Close
                            </Button>
                        ]}>
                        <NewDR onAdd={(dr) => this.onAdd(dr)} onEdit={(dr) => this.onEdit(dr, dr.name, dr.zoneType)} currentRegionStr={this.state.regionStr} drToEdit={null} isEdit={false} />
                    </Modal>


                    <GalleryItem>
                        <FormGroup label="Country" fieldId="form-region" style={{width: "25%"}}>
                            <FormSelect
                                value={this.state.regionStr}
                                onChange={x => this.setUlsRegion(x)}
                                id="horizontal-form-uls-region"
                                name="horizontal-form-uls-region"
                                isValid={!!this.state.regionStr}
                                style={{ textAlign: "right" }}
                            >
                                <FormSelectOption key={undefined} value={undefined} label="Select a Country" />
                                {this.state.allRegions.map((option: string) => (
                                    <FormSelectOption key={option} value={option} label={mapRegionCodeToName(option)} />
                                ))}
                            </FormSelect>
                        </FormGroup>
                    </GalleryItem>
                    <br />
                    <DRTable
                        deniedRegions={this.state.deniedRegions}
                        onDelete={(id: string) => this.deleteDR(id)}
                        currentRegionStr={this.state.regionStr} />
                    <br />
                    {hasRole("Admin") &&
                        (<Button onClick={() => this.putDeniedRegions()}
                            isDisabled={!this.state.deniedRegionsNeedSaving}>Submit Denied Regions
                        </Button>)}
                    <br />
                    {hasRole("Admin") &&
                        <Button key="AddNew" variant="primary" onClick={() => this.openEditor()} >
                            Add New Denied Region
                        </Button>
                    }

                </CardBody>
            </Card >
        )
    }


}

/**
 * wrapper for ap list when it is not embedded in another page
 */
export class DRListPage extends React.Component<{ regions: RatResponse<string[]> }, { regions: string[] }> {
    constructor(props) {
        super(props);

        this.state = { regions: [] };
        if (props.regions.kind === "Success") {
            Object.assign(this.state, { regions: props.regions.result });
        } else {
            logger.error("Could not load regions",
                "error code: ", props.regions.errorCode,
                "description: ", props.regions.description)
            Object.assign(this.state, { regions: [] });
        }

    }

    render() {
        return (
            <PageSection id="ap-list-page">
                <UserContext.Consumer>{(u: UserState) => u.data.loggedIn &&
                    <DRList regions={this.state.regions} />}
                </UserContext.Consumer>
            </PageSection>
        )
    }
}
