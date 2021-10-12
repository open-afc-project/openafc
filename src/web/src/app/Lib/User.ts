import { createContext } from "react";
import { guiConfig } from "./RatApi";
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
        token: string,
        email: string,
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
export type Role = "AP" | "Analysis" | "Admin";

/**
 * Create React context. This is used to provide a global service so that any component can access user state.
 * Use this from root component to initialize user state before configuring user API
 */
export const UserContext = createContext<UserState>({ data: { loggedIn: false } });

/**
 * update the current user. To be overridden by configure.
 * @param user The user to set as the current user in the application
 */
var setUser: (user: UserState) => void = () => {};

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
 * Executes a series of transactions to login a user with credentials and update user state if successful.
 * @param un email of user
 * @param ps user password
 * @param rememberMe indicates browser should store token in cookie that lasts for 30 days
 * @returns result of login
 */
export const login = async (un: string, ps: string, rememberMe?: boolean): Promise<RatResponse<string>> => {

    try {
        const loginResult = await fetch(guiConfig.login_url, {
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ email: un, password: ps }),
            method: "POST"
        });

        if (!loginResult.ok) return error(loginResult.statusText, loginResult.status);

        const { message, status, token } = await loginResult.json();
        if (status !== "success") return error(message, loginResult.status, status);

        return await retrieveUserData(token, rememberMe);
    } catch (e) {
        logger.error(e);
        return error("An error occured");
    }
}

/**
 * Logout the current user. 
 * 
 * This blacklists the token and clears the cookie being used so the 
 * user will need to log back in to regain access to the site.
 * @returns result of logout
 */
export const logout = async (): Promise<RatResponse<string>> => {
    const user = getUser();
    if (!user.data.loggedIn) return error("Already logged out");

    const logoutResult = await fetch(guiConfig.login_url.replace("login", "logout"), {
        headers: {
            "Content-Type": "application/json",
            "Authorization": user.data.token
        },
        "method": "POST"
    });
    if (logoutResult.ok) {
        document.cookie = "";
        setUser({ data: { loggedIn: false }});
        window.location.hash = "#";
        logger.info("User logged out");
        return success((await logoutResult.json()).message);
    }

    return error(logoutResult.statusText, logoutResult.status, await logoutResult.json());
}

/**
 * Call when page is first loaded. Attempts to load the user into the user state. 
 * Checks if cookie is present and uses that to retrieve user state.
 * If no cookie is present then user is redirected to login screen
 * @returns result of loading user data
 */
export const loadUserData = async (): Promise<RatResponse<string>> => {
    const user = getUser();
    if (user.data.loggedIn) return success("User already loaded");

    const token = getCookie();
    if (token) {
        // user retrieve user data but don't remember me because cookie is already stored
        return await retrieveUserData(token);
    } else {
        // redirect to login page
        window.location.hash = "#/login"; // don't do login right now
        return error("User not logged in");
    }
}

/**
 * Gets user state from server and set user state.
 * @param token authentication token for user
 * @param rememberMe indicates that token should be stored in a cookie for 30 days
 * @returns result of getting user data
 */
const retrieveUserData = async (token: string, rememberMe?: boolean): Promise<RatResponse<string>> => {
    // get user information
    const userInfoResult = await fetch(guiConfig.login_url.replace("login", "status"), {
        headers: {
            "Content-Type": "application/json",
            "Authorization": token,
        },
        "method": "GET"
    });

    if (!userInfoResult.ok) return error(userInfoResult.statusText, userInfoResult.status);

    const userInfo = await userInfoResult.json();
    if (userInfo.status !== "success") return error(status, userInfoResult.status, userInfoResult);

    logger.info("User is logged in");
    // set the user session
    setUser({ data: Object.assign({
        loggedIn: true,
        token: token
    }, userInfo.data) });

    if (rememberMe) putCookie(token);
    logger.info("User: ", getUser().data);

    return success("User loaded");
}

/**
 * Adds authentication header to header object in HTTP request.
 * If user is not logged in then headers are not modified.
 * 
 * example usage:
 * ```
 * fetch("/url/path", {
 *    method: "POST",
 *    headers: addAuth({
 *      "Content-Type": "appllication/json"
 *      }),
 *    body: JSON.stringify({ prop: value })
 * })
 * ```
 * @param headers headers to which authentication will be added
 * @returns Updated headers
 */
export const addAuth = (headers: Record<string, string>): Record<string, string> => {
    const user = getUser();
    if (user.data.loggedIn) {
        headers["Authorization"] = user.data.token;
    }
    return headers;
};

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
export const isAdmin = () => hasRole("Admin")

/**
 * Store cookie in browser for 30 days
 * @param value token value to store
 */
const putCookie = (value: string) => {
    document.cookie = "fbrat=" + value + "; expires=" + (new Date(Date.now() + 1000 * 3600 * 24 * 30)).toUTCString() + "; ";
}

/**
 * Attempts to retrieve the user token from the browser cookie.
 * @returns If cookie is present then authentication token is returned. If no cookie is present then `undefined` is returned
 */
const getCookie = (): string | undefined => {
    if (document.cookie) {
        const value = document.cookie;
        const terms: [string, string][] = value.split("; ").map(x => x.split("=") as [string, string]);
        const token = terms.find(x => x[0] === "fbrat")![1];
        return token;
    }
    return undefined;
}