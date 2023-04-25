import * as React from "react";
import { AccessPointModel, DeniedRegion, error, success } from "../Lib/RatApiTypes";
import { getAccessPoints, addAccessPoint, deleteAccessPoint } from "../Lib/Admin";
import { Card, CardHead, CardHeader, CardBody, PageSection, InputGroup, Select, SelectOption, FormSelect, FormSelectOption, Button, Modal } from "@patternfly/react-core";
import { NewAP, NewDR } from "./NewDR";
import { APTable, DRTable } from "./DRTable";
import { logger } from "../Lib/Logger";
import { UserContext, UserState, isLoggedIn, hasRole } from "../Lib/User";
import { UserModel } from "../Lib/RatApiTypes";
import { mapRegionCodeToName } from "../Lib/Utils";

/**
 * DRList.tsx: List of denied Regions (by regionStr) to add/remove
 * author: mgelman@rkf-eng.com
 */

/**
 * Interface definition of `DRList` properties
 */
export interface DRListProps {
    regions: string[];
}

interface DRListState {
    deniedRegions: DeniedRegion[],
    regionStr: string,
    deniedRegionsNeedSaving: boolean,
    showingSaveWarning: boolean,
    newRegionString?:string
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
            showingSaveWarning: false
        }

    }

    private  onAdd(dr: DeniedRegion) {
       this.setState({deniedRegions: this.state.deniedRegions.concat(dr), deniedRegionsNeedSaving:true})
        
    }

    private onEdit(dr: DeniedRegion, prevName: string, prevZoneType: string){
        //Need to make changes here
        this.setState({deniedRegions: this.state.deniedRegions.concat(dr), deniedRegionsNeedSaving:true})
    }

    private async deleteDR(id: string) {
        this.setState({
            deniedRegions: this.state.deniedRegions.filter(x => !(x.regionStr == this.state.regionStr && x.name + x.zoneType == id)),
            deniedRegionsNeedSaving: true
        })
    }

   private setUlsRegion(newRegion: string): void {
        if (newRegion != this.state.regionStr) {
            if (this.state.deniedRegionsNeedSaving) {
                this.setState({ showingSaveWarning: true, newRegionString:newRegion })
            } else {
                this.setState({ regionStr: newRegion, deniedRegionsNeedSaving: false });
            }
        }
    }

    private  closeSaveWarning(){
        this.setState({ showingSaveWarning: false })
    }

    private forceChangeRegion(){
        this.setState({ showingSaveWarning: false, regionStr: this.state.newRegionString!, newRegionString: undefined , deniedRegionsNeedSaving: false })
    }

    private putDeniedRegions()  {

    }


    render() {
        return (
            <Card>
                <CardHead><CardHeader>Denied Region</CardHeader></CardHead>
                <CardBody>
                    <Modal
                        title="Unsaved Changes"
                        isOpen={this.state.showingSaveWarning}
                        onClose={this.closeSaveWarning}
                        actions={[
                            <Button key="confirm" variant="primary" onClick={this.forceChangeRegion}>
                                Confirm
                            </Button>,
                            <Button key="cancel" variant="link" onClick={this.closeSaveWarning}>
                                Cancel
                            </Button>
                        ]}
                    >
                        You have unsaved changes to the denied regions for the current country. Proceed and lose changes?
                    </Modal>
                    <InputGroup label="Country" >
                        <FormSelect
                            value={this.state.regionStr}
                            onChange={x => this.setUlsRegion(x)}
                            id="horizontal-form-uls-region"
                            name="horizontal-form-uls-region"
                            isValid={!!this.state.regionStr}
                            style={{ textAlign: "right" }}
                        >
                            <FormSelectOption key={undefined} value={undefined} label="Select a Country" />
                            {this.props.regions.map((option: string) => (
                                <FormSelectOption key={option} value={option} label={mapRegionCodeToName(option)} />
                            ))}
                        </FormSelect>
                    </InputGroup>
                    {hasRole("Admin") &&
                        (<NewDR onAdd={this.onAdd} onEdit={this.onEdit} currentRegionStr={this.state.regionStr} drToEdit={null} isEdit={false} />)
                    }
                    <br />
                    <DRTable
                        deniedRegions={this.state.deniedRegions}
                        onDelete={(id: string) => this.deleteDR(id)}
                        currentRegionStr={this.state.regionStr} />
                    {hasRole("Admin") &&
                        (<Button onClick={this.putDeniedRegions()}
                            isDisabled={!this.state.deniedRegionsNeedSaving}>Submit Denied Regions
                        </Button>)}
                </CardBody>
            </Card>
        )
    }
    

}

/**
 * wrapper for ap list when it is not embedded in another page
 */
export const DRListPage: React.FunctionComponent = () => (
    <PageSection id="ap-list-page">
        <UserContext.Consumer>{(u: UserState) => u.data.loggedIn &&
            <DRList userId={u.data.userId} filterId={0} org={u.data.org} />}
        </UserContext.Consumer>
    </PageSection>
);
