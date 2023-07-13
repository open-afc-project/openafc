import * as React from "react";
import { DeniedRegion, error, success, RatResponse } from "../Lib/RatApiTypes";
import { getDeniedRegions, getDeniedRegionsCsvFile, updateDeniedRegions, putAccessPointDenyList, getAccessPointsDeny, addAccessPointDeny } from "../Lib/Admin";
import { Card, CardHead, CardHeader, CardBody, PageSection, InputGroup, Select, SelectOption, FormSelect, FormSelectOption, Button, Modal, Alert, AlertActionCloseButton, GalleryItem, FormGroup } from "@patternfly/react-core";
import { NewDR } from "./NewDR";
import { DRTable } from "./DRTable";
import { NewAPDeny } from "./NewAPDeny";
import { logger } from "../Lib/Logger";
import { UserContext, UserState, isLoggedIn, hasRole } from "../Lib/User";
import { mapRegionCodeToName } from "../Lib/Utils";
import { exportCache, getRulesetIds } from "../Lib/RatApi";

/**
 * DRList.tsx: List of denied rules by regionStr and by AP cert id and/or serial number
 * to add/remove
 * author: mgelman@rkf-eng.com
 */

/**
 * Interface definition of `DRList` properties
 */
export interface DRListProps {
    regions: string[],
    userId: number,
    org: string
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
    rulesetIds: [],
    apMessageSuccess?: string,
    apMessageError?: string,
    drForEditing?: DeniedRegion
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
            isEditorOpen: false,
            rulesetIds: [],
            drForEditing: undefined
        }
        this.loadCurrentDeniedRegions();

        getRulesetIds()
            .then(res => {
                if (res.kind === "Success") {
                    this.setState({ rulesetIds: res.result });
                } else {
                    alert(res.description);
                }
            })

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

    onOpenEdit = (id: string) => {
        let drToEdit = this.state.deniedRegions.find(x => (x.regionStr == this.state.regionStr && x.name + "===" + x.zoneType == id));
        this.setState({ drForEditing: drToEdit, isEditorOpen: true })
    }

    onCloseEdit = (dr: DeniedRegion, prevName: string, prevZoneType: string) => {
        if (!!dr) {
            let oldId = prevName + "===" + prevZoneType
            let drToEdit = this.state.deniedRegions.findIndex(x => (x.regionStr == this.state.regionStr && x.name + "===" + x.zoneType == oldId));
            if (drToEdit >= 0) {
                //Need to copy the array so that the updated state will trigger
                let newDrs = Array.from(this.state.deniedRegions);
                newDrs[drToEdit] = dr;
                this.setState({ deniedRegions: newDrs, deniedRegionsNeedSaving: true, isEditorOpen: false, drForEditing: undefined })
            }
        } else {
            this.setState({ isEditorOpen: false, drForEditing: undefined })
        }

    }

    deleteDR = (id: string) => {
        this.setState({
            deniedRegions: this.state.deniedRegions.filter(x => !(x.regionStr == this.state.regionStr && x.name + "===" + x.zoneType == id)),
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

    downloadBlob = (b: Blob, filename: string) => {
        const element = document.createElement("a");
        element.href = URL.createObjectURL(b);
        element.download = filename;
        document.body.appendChild(element); // Required for this to work in FireFox
        element.click();
    }

    downloadCSVFile = () => {
        getDeniedRegionsCsvFile(this.state.regionStr).then(res => {
            if (res.kind === "Success") {
                let file = new Blob([res.result], {
                    type: "text/csv"
                });
                this.downloadBlob(file, this.state.regionStr + "_denied_regions.csv");
            }
        });
    }

    closeEditor = () => {
        this.setState({ isEditorOpen: false, drForEditing: undefined })
    }

    openEditor = () => {
        this.setState({ isEditorOpen: true })
    }


    importList(ev) {
        // @ts-ignore
        const file = ev.target.files[0];
        const reader = new FileReader();
        try {
            reader.onload = async () => {
                try {
                    const value: AccessPointListModel = { accessPoints: reader.result as string }
                    const putResp = await putAccessPointDenyList(value, this.props.userId);
                    if (putResp.kind === "Error") {
                        this.setState({ apMessageError: putResp.description, apMessageSuccess: undefined });
                        return;
                    } else {
                        this.setState({ apMessageError: undefined, apMessageSuccess: "Import successful!" });
                    }
                } catch (e) {
                    this.setState({ apMessageError: "Unable to read file", apMessageSuccess: undefined })
                }
            }

            reader.readAsText(file);
            ev.target.value = "";
        } catch (e) {
            logger.error("Failed read file ", e);
            this.setState({ apMessageError: "Failed to read read file", apMessageSuccess: undefined });
        }
    }

    async onAddAPDeny(ap: AccessPointModel) {
        const res = await addAccessPointDeny(ap, this.props.userId);
        if (res.kind === "Success") {
            return success("Added")
        } else {
            return res;
        }
    }

    async exportList() {
        const res = await getAccessPointsDeny(this.props.userId);
        if (res.kind == "Success") {
            let b = new Blob([res.result], {
                type: "text/csv"
            });
            this.downloadBlob(b, "denied_aps.json")
        }
    }

    render() {
        return (
            <PageSection>
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
                                    Close without saving 
                                </Button>
                            ]}>
                            <NewDR onAdd={(dr) => this.onAdd(dr)} onCloseEdit={(dr, prevName, prevZone) => this.onCloseEdit(dr, prevName, prevZone)} currentRegionStr={this.state.regionStr} drToEdit={this.state.drForEditing} />
                        </Modal>


                        <GalleryItem>
                            <FormGroup label="Country" fieldId="form-region" style={{ width: "25%" }}>
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
                            currentRegionStr={this.state.regionStr}
                            onOpenEdit={(id: string) => this.onOpenEdit(id)} />
                        <br />
                        {hasRole("Super") &&
                            <Button key="AddNew" variant="primary" onClick={() => this.openEditor()} >
                                Add New Denied Region
                            </Button>
                        }
                        <br />
                        {hasRole("Super") &&
                            (<Button onClick={() => this.putDeniedRegions()}
                                isDisabled={!this.state.deniedRegionsNeedSaving}>Submit Denied Regions
                            </Button>)}
                        <br />
                        {hasRole("Super") &&
                            (<Button onClick={() => this.downloadCSVFile()}>Download Denied Regions File</Button>)}



                    </CardBody>
                </Card >

                <Card>
                    <CardHead><CardHeader> Denied Access Points</CardHeader></CardHead>
                    <CardBody>
                        {hasRole("Admin") &&
                            (<NewAPDeny onAdd={(ap: AccessPointModel) => this.onAddAPDeny(ap)} rulesetIds={this.state.rulesetIds} org={this.props.org} />)
                        }

                        <br />
                        {this.state.apMessageError !== undefined && (
                            <Alert
                                variant="danger"
                                title="Error"
                                action={<AlertActionCloseButton onClose={() => this.setState({ apMessageError: undefined })} />}
                            >{this.state.apMessageError}</Alert>
                        )}

                        {this.state.apMessageSuccess !== undefined && (
                            <Alert
                                variant="success"
                                title="Success"
                                action={<AlertActionCloseButton onClose={() => this.setState({ apMessageSuccess: undefined })} />}
                            >{this.state.apMessageSuccess}</Alert>
                        )}

                        <FormGroup
                            label="Import Deny AP List"
                            fieldId="import-ap-deny-list"
                        >
                            <input
                                // @ts-ignore
                                type="file"
                                name="import deny ap list"
                                // @ts-ignore
                                onChange={(ev) => this.importList(ev)}
                            />
                        </FormGroup>

                        <br />
                        {hasRole("Admin") &&
                            (<Button onClick={() => this.exportList()}>Download Denied APs</Button>)}
                        <br />

                    </CardBody>
                </Card>

            </PageSection>
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
            <PageSection id="ap-deny-list-page">
                <UserContext.Consumer>{(u: UserState) => u.data.loggedIn &&
                    <DRList regions={this.state.regions} userId={u.data.userId} org={u.data.org} />}
                </UserContext.Consumer>
            </PageSection>
        )
    }
}
