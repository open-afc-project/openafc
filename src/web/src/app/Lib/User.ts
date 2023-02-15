/**
 * Portions copyright © 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate
 * affiliate that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy
 * of which is included with this software program.
 */

import { createContext } from "react";
import { clearCache, guiConfig, importCache } from "./RatApi";
import { logger } from "./Logger";
import { RatResponse, error, success } from "./RatApiTypes";
import { letin } from "./Utils";

/**
 * User.ts: Module for handling user state and login
 * author: Sam Smuncy
 */

/**
 * Storage object of current user of application
 */
export interface UserState {
    data: {
        loggedIn: false
    } | {
        loggedIn: true,
        email: string,
        org: string,
        userId: number,
        roles: Role[],
        firstName?: string,
        lastName?: string,
        active: boolean
    }
}

/**
 * Sum type of possible roles a user can have
 */
export type Role = "AP" | "Analysis" | "Admin" | "Super" | "Trial"; 

/**
 * Create React context. This is used to provide a global service so that any component can access user state.
 * Use this from root component to initialize user state before configuring user API
 */
export const UserContext = createContext<UserState>({ data: { loggedIn: false } });

/**
 * update the current user. To be overridden by configure.
 * @param user The user to set as the current user in the application
 */
var setUser: (user: UserState) => void = () => { };

/**
 * get the current user. To be overridden by configure.
 * @returns The current user
 */
var getUser: () => UserState = () => ({ data: { loggedIn: false } });

/**
 * Configure the User at application startup.
 * Call from root node and pass in hooks so that user API can access global user object.
 * @param get function to get the current user
 * @param set function to update the current user
 */
export const configure = (get: () => UserState, set: (user: UserState) => void) => {
    getUser = get;
    setUser = set;
}

/**
 * Gets user state from server and set user state.
 * @param token authentication token for user
 * @returns result of getting user data
 */
export const retrieveUserData = async (): Promise<RatResponse<string>> => {
    // get user information
    console.log("retrieveUserData: ", guiConfig.status_url);
    const userInfoResult = await fetch(guiConfig.status_url, {
        headers: {
            "Content-Type": "application/json",
        },
        "method": "GET"
    });

    if (!userInfoResult.ok) return error(userInfoResult.statusText, userInfoResult.status);

    const userInfo = await userInfoResult.json();
    if (userInfo.status !== "success") {
        setUser({ data: { loggedIn: false } });
        return error(status, userInfoResult.status, userInfoResult);
    }

    logger.info("User is logged in");
    // set the user session
    setUser({
        data: Object.assign({
            loggedIn: true,
        }, userInfo.data)
    });

    logger.info("User: ", getUser().data);

    return success("User loaded");
}

/**
 * Is user credential is editable
 */
export const isEditCredential = () => getUser().data.editCredential;

/**
 * Is current user logged in?
 * @returns User login status
 */
export const isLoggedIn = () => getUser().data.loggedIn;

/**
 * Does the current user have an active account?
 * ie. have they verified their email.
 * @returns status of active status
 */
export const isActive = () => letin(getUser(), u => u.data.loggedIn && u.data.active);

/**
 * Does the user have this role?
 * @param role
 * @returns Tells if user has this role
 */
export const hasRole = (role: Role) => letin(getUser(), u => u.data.loggedIn && u.data.roles.includes(role));

/**
 * Is the current user an admin?
 * @returns Tells if user is an admin
 */
export const isAdmin = () => (hasRole("Admin") || hasRole("Super"))
