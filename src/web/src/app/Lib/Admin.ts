import { guiConfig } from "./RatApi";
import { UserModel, success, error, AccessPointModel, FreqRange } from "./RatApiTypes";
import { logger } from "./Logger";
import { Role} from "./User";

/** 
* Admin.ts: Functions for Admin API. User and account management, and permissions
* author: Sam Smucny
*/

export class Limit {
    enforce: boolean;
    limit: number;
    constructor(enforce : boolean, limit : number) {
        this.enforce = enforce;
        this.limit = limit;
    }
}

/**
 * Gets the current Minimum EIRP value
 * @returns object indicating minimum value and whether or not it's enforced if successful, error otherwise
 */
export const getMinimumEIRP = () => 
    fetch(guiConfig.admin_url.replace("-1", "eirp_min"), {
        method: "GET",
    }).then(async res => {
        if (res.ok) {
            return success((await res.json()) as Limit);
        } else {
            return error("Unable to load limits", res.status, res);
        }
    }).catch(e => {
        logger.error(e);
        return error("Request failed");
    })


/**
 * Sets the Minimum EIRP value.
 * @param limit the new EIRP value 
 */
export const setMinimumEIRP = (limit: number | boolean) =>
    fetch(guiConfig.admin_url.replace("-1", "eirp_min"), {
        method: "PUT",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(limit)
    })
    .then(async res => {
        if (res.ok) {
            return success((await res.json()).limit as number | boolean)
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered", undefined, err));

/**
 * Return list of all users. Must be Admin
 * There is current no support for queried searches/filters/etc. Just returns all.
 * @returns list of users if successful, error otherwise
 */
export const getUsers = () => 
    fetch(guiConfig.admin_url.replace("-1", "0"), {
        method: "GET",
    }).then(async res => {
        if (res.ok) {
            return success((await res.json()).users as UserModel[]);
        } else {
            return error("Unable to load users", res.status, res);
        }
    }).catch(e => {
        logger.error(e);
        return error("Request failed");
    })

/** 
 * gets a single user by id
 * @param id User Id
 * @return The user if found, error otherwise
 */
export const getUser = (id: number) =>
    fetch(guiConfig.admin_url.replace("-1", String(id)), {
        method: "GET",
    }).then(async res => 
        res.ok ?
        success((await res.json()).user as UserModel) :
        error("Unable to load user", res.status, res)
    ).catch(e => {
        logger.error(e);
        return error("Request failed");
    })

/**
 * Update a user's data
 * @param user User to replace with
 */
export const updateUser = (user: { email: string, password: string, id: number, active: boolean }) => 
    fetch(guiConfig.admin_url.replace("-1", String(user.id)), {
        method: "POST",
        body: JSON.stringify(Object.assign(user, { setProps: true })),
        headers: { "Content-Type": "application/json" }
    }).then(res => 
        res.ok ? 
            success(res.statusText) : 
            error(res.statusText, res.status, res));

/**
 * Give a user a role
 * @param id user's Id
 * @param role role to add
 */
export const addUserRole = (id: number, role: Role) =>
    fetch(guiConfig.admin_url.replace("-1", String(id)), {
        method: "POST",
        body: JSON.stringify({ addRole: role }),
        headers: { "Content-Type": "application/json" }
    }).then(res => {
        if (res.ok) {
            return success(res.status);
        } else {
            return error(res.statusText, res.status, res);
        }
    }).catch(err => error("An error was encountered", undefined, err));

/**
 * Remove a role from a user
 * @param id user's Id
 * @param role role to remove
 */
export const removeUserRole = (id: number, role: Role) =>
fetch(guiConfig.admin_url.replace("-1", id.toString()), {
    method: "POST",
    body: JSON.stringify({ removeRole: role }),
    headers: { "Content-Type": "application/json" }
}).then(res => {
    if (res.ok) {
        return success(res.status);
    } else {
        return error(res.statusText, res.status, res);
    }
}).catch(err => error("An error was encountered", undefined, err));

/**
 * Delete a user from the system
 * @param id user'd Id
 */
export const deleteUser = (id: number) => 
    fetch(guiConfig.admin_url.replace("-1", String(id)), {
       method: "DELETE",
    }).then(res => {
        if (res.ok) {
            return success(res.status);
        } else {
            return error(res.statusText, res.status, res);
        }
    }).catch(err => error("An error was encountered", undefined, err));

/**
 * Get access points. If `userId` is provided then only return access
 * points owned by the user. If no `userId` is provided then return all
 * access points (must be `Admin`).
 * @param userId (optional) user's Id
 * @returns list of access points if successful, error otherwise
 */
export const getAccessPoints = (userId?: number) =>
    fetch(guiConfig.ap_admin_url.replace("-1", String(userId || 0)), {
        method: "GET",
    }).then(async res => {
        if (res.ok) {
            return success((await res.json()).accessPoints as AccessPointModel[]);
        } else {
            return error("Unable to load access points", res.status, res);
        }
    }).catch(err => error("An error was encountered", undefined, err));

/**
 * Register an access point with a user.
 * @param ap Access point to add
 * @param userId owner of new access point
 */
export const addAccessPoint = (ap: AccessPointModel, userId: number) =>
    fetch(guiConfig.ap_admin_url.replace("-1", String(userId)), {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(ap)
    })
    .then(async res => {
        if (res.ok) {
            return success((await res.json()).id as number)
        } else if (res.status === 400) {
            return error("Invalid AP data", res.status, res);
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered", undefined, err));

/**
 * Delete an access point from the system.
 * @param id access point id
 */
export const deleteAccessPoint = (id: number) =>
    // here the id in the url is the ap id, not the user id
    fetch(guiConfig.ap_admin_url.replace("-1", String(id)), {
        method: "DELETE",
    })
    .then(res => {
        if (res.ok) {
            return success(undefined);
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered", undefined, err));

/**
 * Register an mtls certificate
 * @param mtls cert to add
 * @param userId who creates the new mtls cert
 */
export const addMTLS = (mtls: MTLSModel, userId: number) =>
    fetch(guiConfig.mtls_admin_url.replace("-1", String(userId)), {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(mtls)
    })
    .then(async res => {
        if (res.ok) {
            return success((await res.json()).id as number)
        } else if (res.status === 400) {
            return error("Unable to add new certificate", res.status, res);
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered", undefined, err));


/**
 * Delete an mtls cert from the system.
 * @param id mtls cert id
 */
export const deleteMTLSCert = (id: number) =>
    // here the id in the url is the mtls id, not the user id
    fetch(guiConfig.mtls_admin_url.replace("-1", String(id)), {
        method: "DELETE",
    })
    .then(res => {
        if (res.ok) {
            return success(undefined);
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered", undefined, err));

/**
 * Get mtls cert.  If `userId` is 0, then return all certificates (super)
 * or all certificates in the same org as the user (`Admin`).
 * 'userId' non zero is not currently supported as certificate do not belong
 * a single user.
 * @param userId user's Id
 * @returns list of mtls certs if successful, error otherwise
 */
export const getMTLS = (userId?: number) =>
    fetch(guiConfig.mtls_admin_url.replace("-1", String(userId || 0)), {
        method: "GET",
    }).then(async res => {
        if (res.ok) {
            return success((await res.json()).mtls as MTLSModel[]);
        } else {
            return error("Unable to load mtls", res.status, res);
        }
    }).catch(err => error("An error was encountered", undefined, err));





