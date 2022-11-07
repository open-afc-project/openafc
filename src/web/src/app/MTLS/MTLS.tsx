import * as React from "react";
import { MTLSModel, error, success } from "../Lib/RatApiTypes";
import { getMTLS, addMTLS, deleteMTLSCert} from "../Lib/Admin";
import { Card, CardHead, CardHeader, CardBody, PageSection } from "@patternfly/react-core";
import { NewMTLS } from "./NewMTLS";
import { MTLSTable } from "./MTLSTable";
import { logger } from "../Lib/Logger";
import { UserContext, UserState, isLoggedIn, hasRole } from "../Lib/User";
import { UserModel } from "../Lib/RatApiTypes";

/**
 * MTLSList.tsx: List of MTLS certs to add/remove
 */

/**
 * Interface definition of `MTLS` properties
 */
export interface MTLSProps {
    userId: number, //the one created this certificate
    filterId: number, //the userid to filter
    org: string,
    users?: UserModel[]
}

interface MTLSState {
    mtlsList: MTLSModel[]
}

/**
 * Page level component to list a user's registerd access points. Users use these 
 * credentials to utilize the PAWS interface.
 */
export class MTLS extends React.Component<MTLSProps, MTLSState> {
    constructor(props: MTLSProps) {
        super(props);

        this.state = {
            mtlsList: []
        }

        getMTLS(props.filterId)
        .then(res => {
            if (res.kind === "Success") {
                this.setState({ mtlsList: res.result });
            } else {
                alert(res.description);
            }
        })
    }

    private async onAdd(mtls: MTLSModel) {
        const res = await addMTLS(mtls, this.props.userId);
        if (res.kind === "Success") {
            const newMTLSList = await getMTLS(this.props.filterId);
            if (newMTLSList.kind === "Success") {
                this.setState({ mtlsList: newMTLSList.result });
                return success("Added")
            }
            return error("Could not refresh list")
        } else {
            return res;
        }
    }

    private async deleteMTLS(id: number) {
        console.log('deleteMTLS:', String(id));
        const res = await deleteMTLSCert(id);
        if (res.kind === "Success") {
            const newMTLSList = await getMTLS(this.props.filterId);
            if (newMTLSList.kind === "Success") {
                this.setState({ mtlsList: newMTLSList.result });
            }
        } else {
            logger.error(res);
        }
    }

    render() {
        return (
            <Card>
                <CardHead><CardHeader>MTLS</CardHeader></CardHead>
                <CardBody>
                    <NewMTLS onAdd={(mtls: MTLSModel) => this.onAdd(mtls)} userId={this.props.userId} filterId={this.props.filterId} />
                    <br/>
                    <MTLSTable mtlsList={this.state.mtlsList} onDelete={(id: number) => this.deleteMTLS(id)} filterId={this.props.filterId} />
                </CardBody>
            </Card>
        )
    }
}

/**
 * wrapper for mtls when it is not embedded in another page
 */
export const MTLSPage: React.FunctionComponent = () => (
    <PageSection id="mtls-page">
        <UserContext.Consumer>{(u: UserState) => u.data.loggedIn &&
            <MTLS filterId={0} userId={u.data.userId} />}
        </UserContext.Consumer>
    </PageSection>
);
