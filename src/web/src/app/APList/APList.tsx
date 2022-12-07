import * as React from "react";
import { AccessPointModel, error, success } from "../Lib/RatApiTypes";
import { getAccessPoints, addAccessPoint, deleteAccessPoint } from "../Lib/Admin";
import { Card, CardHead, CardHeader, CardBody, PageSection } from "@patternfly/react-core";
import { NewAP } from "./NewAP";
import { APTable } from "./APTable";
import { logger } from "../Lib/Logger";
import { UserContext, UserState, isLoggedIn, hasRole } from "../Lib/User";
import { UserModel } from "../Lib/RatApiTypes";

/**
 * APList.tsx: List of access point to add/remove
 * author: Sam Smucny
 */

/**
 * Interface definition of `APList` properties
 */
export interface APListProps {
    userId: number,
    filterId: number,
    org: string,
}

interface APListState {
    accessPoints: AccessPointModel[]
}

/**
 * Page level component to list a user's registerd access points. Users use these 
 * credentials to utilize the PAWS interface.
 */
export class APList extends React.Component<APListProps, APListState> {
    constructor(props: APListProps) {
        super(props);

        this.state = {
            accessPoints: []
        }

        getAccessPoints(props.filterId)
        .then(res => {
            if (res.kind === "Success") {
                this.setState({ accessPoints: res.result });
            } else {
                alert(res.description);
            }
        })
    }

    private async onAdd(ap: AccessPointModel) {
        const res = await addAccessPoint(ap, this.props.userId);
        if (res.kind === "Success") {
            const newAPs = await getAccessPoints(this.props.userId);
            if (newAPs.kind === "Success") {
                this.setState({ accessPoints: newAPs.result });
                return success("Added")
            }
            return error("Could not refresh list")
        } else {
            return res;
        }
    }

    private async deleteAP(id: number) {
        const res = await deleteAccessPoint(id);
        if (res.kind === "Success") {
            const newAPs = await getAccessPoints(this.props.userId);
            if (newAPs.kind === "Success") {
                this.setState({ accessPoints: newAPs.result });
            }
        } else {
            logger.error(res);
        }
    }

    render() {
        return (
            <Card>
                <CardHead><CardHeader>Access Points</CardHeader></CardHead>
                <CardBody>
                    {hasRole("Admin") &&
                    (<NewAP onAdd={(ap: AccessPointModel) => this.onAdd(ap)} userId={this.props.userId} org={this.props.org}/>)
                    }
                    <br/>
                    <APTable accessPoints={this.state.accessPoints} onDelete={(id: number) => this.deleteAP(id)} filterId={this.props.filterId} />
                </CardBody>
            </Card>
        )
    }
}

/**
 * wrapper for ap list when it is not embedded in another page
 */
export const APListPage: React.FunctionComponent = () => (
    <PageSection id="ap-list-page">
        <UserContext.Consumer>{(u: UserState) => u.data.loggedIn &&
            <APList userId={u.data.userId} filterId={0} org={u.data.org}/>}
        </UserContext.Consumer>
    </PageSection>
);
