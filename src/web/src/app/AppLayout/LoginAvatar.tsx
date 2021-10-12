import * as React from "react";
import { Button } from "@patternfly/react-core";
import { Route } from "react-router";
import { isLoggedIn, logout, UserContext } from "../Lib/User";
import { UserIcon } from "@patternfly/react-icons";
import { UnknownIcon } from "@patternfly/react-icons";

/**
 * LoginAvatar.ts: component for top right of page to login/logout
 * author: Sam Smucny
 */

/**
 * Icon that indicates login status. If clicked, it either
 * redirects to login page or logs out current user.
 */
export class LoginAvatar extends React.Component {

    render() {

        const showLogin = <Route render={
            ({ history }) => (
                <Button variant="plain" onClick={() => { history.push("/login") }}>
                    <UnknownIcon />{" Login"}
                </Button>
            )
            } 
            />;

        const showLogout = <Button variant="link" onClick={() => { logout() }}>
            <UserIcon />{" Logout"}
        </Button>;

        return (<UserContext.Consumer> 
            {user => isLoggedIn() ? showLogout : showLogin }
        </UserContext.Consumer>
        )
    }
}