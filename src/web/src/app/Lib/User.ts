/**
 * Portions copyright © 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate
 * affiliate that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy
 * of which is included with this software program.
 */

import { createContext } from 'react';
import { clearCache, guiConfig, importCache } from './RatApi';
import { logger } from './Logger';
import { RatResponse, error, success } from './RatApiTypes';
import { letin } from './Utils';
import { apiFetch, setSessionExpiredHandler } from './HttpClient';

/**
 * User.ts: Module for handling user state and login
 * author: Sam Smuncy
 */

/**
 * Storage object of current user of application
 */
export interface UserState {
  data:
    | {
        loggedIn: false;
      }
    | {
        loggedIn: true;
        email: string;
        org: string;
        userId: number;
        roles: Role[];
        firstName?: string;
        lastName?: string;
        active: boolean;
        /** False when OIDC / IdP owns email & password (see UserAPI guiconfig). */
        editCredential?: boolean;
      };
}

/**
 * Sum type of possible roles a user can have
 */
export type Role = 'AP' | 'Analysis' | 'Admin' | 'Super' | 'Trial';

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
};

/**
 * Handle session expiration: resets local authentication state and routes to the login screen.
 */
export const handleSessionExpired = () => {
  if (getUser().data.loggedIn) {
    logger.warn('Session has expired on backend. Resetting user state.');
    setUser({ data: { loggedIn: false } });
    if (guiConfig.about_url) {
      window.location.hash = '#/login';
    } else if (guiConfig.login_url) {
      window.location.href = guiConfig.login_url;
    }
  }
};

setSessionExpiredHandler(handleSessionExpired);

/**
 * Gets user state from server and set user state.
 * @param token authentication token for user
 * @returns result of getting user data
 */
export const retrieveUserData = async (): Promise<RatResponse<string>> => {
  // get user information
  try {
    const userInfoResult = await apiFetch(guiConfig.status_url, {
      headers: {
        'Content-Type': 'application/json',
      },
      method: 'GET',
    });

    if (!userInfoResult.ok) {
      if (getUser().data.loggedIn) {
        logger.warn('User status endpoint returned not OK. Marking session as logged out.');
        setUser({ data: { loggedIn: false } });
      }
      return error(userInfoResult.statusText, userInfoResult.status);
    }

    const userInfo = await userInfoResult.json();
    if (userInfo.status !== 'success') {
      setUser({ data: { loggedIn: false } });
      return error(userInfo.status || 'Authentication failed', userInfoResult.status, userInfoResult);
    }

    logger.info('User is logged in');
    // set the user session
    setUser({
      data: Object.assign(
        {
          loggedIn: true,
        },
        userInfo.data,
      ),
    });

    logger.info('User: ', getUser().data);

    return success('User loaded');
  } catch (err: any) {
    logger.error('Failed to retrieve user data:', err);
    return error(String(err));
  }
};

/**
 * Is user credential is editable
 */
// @ts-ignore
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
export const isActive = () => letin(getUser(), (u) => u.data.loggedIn && u.data.active);

/**
 * Does the user have this role?
 * @param role
 * @returns Tells if user has this role
 */
export const hasRole = (role: Role) => letin(getUser(), (u) => u.data.loggedIn && u.data.roles.includes(role));

/**
 * Is the current user an admin?
 * @returns Tells if user is an admin
 */
export const isAdmin = () => hasRole('Admin') || hasRole('Super');
