import * as React from "react";
import { Button } from "@patternfly/react-core";
import { Route } from "react-router";
import { isLoggedIn, logout, login, UserContext } from "../Lib/User";
import { UserIcon } from "@patternfly/react-icons";
import { UnknownIcon } from "@patternfly/react-icons";
import { guiConfig } from "../Lib/RatApi";

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
        const showLogin = <a href={guiConfig.login_url} >
            <button className='button-blue' type="button"> Login <UserIcon />
            </button>
            </a>

        const showLogout = <a href={guiConfig.logout_url} >
            <button className='button-blue' type="button"> Logout <UserIcon />
            </button>
            </a>

        return (<UserContext.Consumer> 
            {user => isLoggedIn() ? showLogout : showLogin }
        </UserContext.Consumer>
        )
    }
}
