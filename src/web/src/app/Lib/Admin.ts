import { guiConfig, getCSRF } from "./RatApi";
import { UserModel, success, error, AccessPointModel, AccessPointListModel, FreqRange, DeniedRegion, ExclusionCircle, ExclusionTwoRect, ExclusionRect, ExclusionHorizon } from "./RatApiTypes";
import { logger } from "./Logger";
import { Role, retrieveUserData } from "./User";
import { Rect } from "react-konva";
import { Circle } from "konva/types/shapes/Circle";
import { RatResponse } from "./RatApiTypes";


/** 
* Admin.ts: Functions for Admin API. User and account management, and permissions
* author: Sam Smucny
*/

export class Limit {
    enforce: boolean;
    limit: number;
    constructor(enforce: boolean, limit: number) {
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
export const setMinimumEIRP = async (limit: number | boolean) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.admin_url.replace("-1", "eirp_min"), {
        method: "PUT",
        headers: { "Content-Type": "application/json",
            'X-CSRF-Token': csrf_token},
        body: JSON.stringify(limit)
    })
        .then(async res => {
            if (res.ok) {
                return success((await res.json()).limit as number | boolean)
            } else {
                return error(res.statusText, res.status, res);
            }
        })
        .catch(err => error("An error was encountered #1", undefined, err));
}

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
export const updateUser = async (user: { email: string, password: string, id: number, active: boolean }) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.admin_url.replace("-1", String(user.id)), {
        method: "POST",
        body: JSON.stringify(Object.assign(user, { setProps: true })),
        headers: { "Content-Type": "application/json",
             'X-CSRF-Token': csrf_token},
    }).then(res =>
        res.ok ?
            success(res.statusText) :
            error(res.statusText, res.status, res));
}

/**
 * Give a user a role
 * @param id user's Id
 * @param role role to add
 */
export const addUserRole = async (id: number, role: Role) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.admin_url.replace("-1", String(id)), {
        method: "POST",
        body: JSON.stringify({ addRole: role }),
        headers: { "Content-Type": "application/json",
            'X-CSRF-Token': csrf_token}
    }).then(res => {
        if (res.ok) {
            return success(res.status);
        } else {
            return error(res.statusText, res.status, res);
        }
    }).catch(err => error("An error was encountered #2", undefined, err));
}

/**
 * Remove a role from a user
 * @param id user's Id
 * @param role role to remove
 */
export const removeUserRole = async (id: number, role: Role) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.admin_url.replace("-1", id.toString()), {
        method: "POST",
        body: JSON.stringify({ removeRole: role }),
        headers: { "Content-Type": "application/json",
            'X-CSRF-Token': csrf_token}
    }).then(res => {
        if (res.ok) {
            return success(res.status);
        } else {
            return error(res.statusText, res.status, res);
        }
    }).catch(err => error("An error was encountered #3", undefined, err));
}


/**
 * Delete a user from the system
 * @param id user'd Id
 */
export const deleteUser = async (id: number) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.admin_url.replace("-1", String(id)), {
        method: "DELETE",
        headers: {'X-CSRF-Token': csrf_token},
    }).then(res => {
        if (res.ok) {
            return success(res.status);
        } else {
            return error(res.statusText, res.status, res);
        }
    }).catch(err => error("An error was encountered #4", undefined, err));
}


/**
 * Get access points. If `userId` is provided then only return access
 * points owned by the user. If no `userId` is provided then return all
 * access points (must be `Admin`).
 * @param userId (optional) user's Id
 * @returns list of access points if successful, error otherwise
 */
export const getAccessPointsDeny = (userId?: number) =>
    fetch(guiConfig.ap_deny_admin_url.replace("-1", String(userId || 0)), {
        method: "GET",
    }).then(async res => {
        if (res.ok) {
            return success(await res.text());
        } else {
            return error("Unable to load access points", res.status, res);
        }
    }).catch(err => error("An error was encountered #8", undefined, err));

/**
 * Register an access point with a user.
 * @param ap Access point to add
 * @param userId owner of new access point
 */
export const addAccessPointDeny = async (ap: AccessPointModel, userId: number) => {
    let csrf_token = await(getCSRF());

    fetch(guiConfig.ap_deny_admin_url.replace("-1", String(userId)), {
        method: "PUT",
        headers: { "Content-Type": "application/json", 
             'X-CSRF-Token': csrf_token},
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
    .catch(err => error("An error was encountered #9", undefined, err));
}

/**
 * Post a new deny access point file
 * @param ap Access point to add
 * @param userId owner of new access point
 */
export const putAccessPointDenyList = async (ap: AccessPointListModel, userId: number) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.ap_deny_admin_url.replace("-1", String(userId)), {
        method: "POST",
        headers: { "Content-Type": "application/json",
            'X-CSRF-Token': csrf_token},
        body: JSON.stringify(ap)
    })
    .then(res => {
        if (res.ok) {
            return success(res.status)
        } else if (res.status === 400) {
            return error("Invalid AP data", res.status, res);
        } else {
            return error(res.statusText, res.status, res);
        }
    })
    .catch(err => error("An error was encountered #10", undefined, err));
}

/**
 * Register an mtls certificate
 * @param mtls cert to add
 * @param userId who creates the new mtls cert
 */
export const addMTLS = async (mtls: MTLSModel, userId: number) => {
    let csrf_token = await(getCSRF());
    fetch(guiConfig.mtls_admin_url.replace("-1", String(userId)), {
        method: "POST",
        headers: { "Content-Type": "application/json",
            'X-CSRF-Token': csrf_token},
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
        .catch(err => error("An error was encountered #11", undefined, err));
}


/**
 * Delete an mtls cert from the system.
 * @param id mtls cert id
 */
export const deleteMTLSCert = async (id: number) => {
    // here the id in the url is the mtls id, not the user id
    let csrf_token = await(getCSRF());
    fetch(guiConfig.mtls_admin_url.replace("-1", String(id)), {
        method: "DELETE",
        headers: {'X-CSRF-Token': csrf_token},
    })
        .then(res => {
            if (res.ok) {
                return success(undefined);
            } else {
                return error(res.statusText, res.status, res);
            }
        })
        .catch(err => error("An error was encountered #12", undefined, err));
}

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
    }).catch(err => error("An error was encountered #13", undefined, err));


export const getDeniedRegions = (regionStr: string) => {
    return fetch(guiConfig.dr_admin_url.replace("XX", regionStr), {
        method: "GET",
        headers: {
            'content-type': 'text/csv',
        }
    }).then(async res => {
        if (res.ok) {

            return success(mapDeniedRegionFromCsv(await res.text(), regionStr));
        } else if (res.status == 404) {
            return success([])
        } else {
            return error("Unable to get denied regions for " + regionStr, res.status, res);
        }
    }).catch(err => error("An error was encountered #14", undefined, err));
}

export const getDeniedRegionsCsvFile = (regionStr: string) => {
    return fetch(guiConfig.dr_admin_url.replace("XX", regionStr), {
        method: "GET",
        headers: {
            'content-type': 'text/csv',
        }
    }).then(async res => {
        if (res.ok) {
            return success(await res.text())
        } else {
            return error("Unable to get denied regions for " + regionStr, res.status, res);
        }
    }).catch(err => error("An error was encountered #15", undefined, err));
}

// Update the denied regions for a given region
export const updateDeniedRegions = async (records: DeniedRegion[], regionStr: string) => {
    let body = mapDeniedRegionToCsv(records, regionStr, true);
    let csrf_token = await(getCSRF());

    return fetch(guiConfig.dr_admin_url.replace("XX", regionStr), {
        method: "PUT",
        headers: { "Content-Type": "text/csv",
           'X-CSRF-Token': csrf_token},
        body: body
    }).then(async res => {
        if (res.status === 204) {
            return success("Denied regions updated.");
        }
        else {
            return error(res.statusText, res.status);
        }
    }).catch(err => {
        logger.error(err);
        return error("Unable to update denied regions.");
    })
};

function parseCSV(str: string, headers = true) {
    const arr: string[][] = [];
    let quote = false;  // 'true' means we're inside a quoted field

    // Iterate over each character, keep track of current row and column (of the returned array)
    for (let row = 0, col = 0, c = 0; c < str.length; c++) {
        let cc = str[c], nc = str[c + 1];        // Current character, next character
        arr[row] = arr[row] || [];             // Create a new row if necessary
        arr[row][col] = arr[row][col] || '';   // Create a new column (start with empty string) if necessary

        // If the current character is a quotation mark, and we're inside a
        // quoted field, and the next character is also a quotation mark,
        // add a quotation mark to the current column and skip the next character
        if (cc == '"' && quote && nc == '"') { arr[row][col] += cc; ++c; continue; }

        // If it's just one quotation mark, begin/end quoted field
        if (cc == '"') { quote = !quote; continue; }

        // If it's a comma and we're not in a quoted field, move on to the next column
        if (cc == ',' && !quote) { ++col; continue; }

        // If it's a newline (CRLF) and we're not in a quoted field, skip the next character
        // and move on to the next row and move to column 0 of that new row
        if (cc == '\r' && nc == '\n' && !quote) { ++row; col = 0; ++c; continue; }

        // If it's a newline (LF or CR) and we're not in a quoted field,
        // move on to the next row and move to column 0 of that new row
        if (cc == '\n' && !quote) { ++row; col = 0; continue; }
        if (cc == '\r' && !quote) { ++row; col = 0; continue; }

        // Otherwise, append the current character to the current column
        arr[row][col] += cc;
    }

    if (headers) {
        let headerRow = arr[0];


    }

    return arr;
}

function parseCSVtoObjects(csvString: string) {
    var csvRows = parseCSV(csvString);

    var columnNames = csvRows[0];
    var firstDataRow = 1;


    var result = [];
    for (var i = firstDataRow, n = csvRows.length; i < n; i++) {
        var rowObject: any = {};
        var row = csvRows[i];
        for (var j = 0, m = Math.min(row.length, columnNames.length); j < m; j++) {
            var columnName = columnNames[j];
            var columnValue = row[j];
            rowObject[columnName] = columnValue;
        }
        result.push(rowObject);
    }
    return result;
}

const mapDeniedRegionFromCsv = (data: string, regionStr: string) => {
    let records = parseCSVtoObjects(data);
    let objects = records.map(x => {
        let newRegion: DeniedRegion = {
            regionStr: regionStr,
            name: x["Location"],
            endFreq: x["Stop Freq (MHz)"],
            startFreq: x["Start Freq (MHz)"],
            exclusionZone: dummyExclusionZone,
            zoneType: "Circle"
        }

        //Is a one or two rect if it has a rect1 lat 1
        if (!!x["Rectangle1 Lat 1"]) {
            //Is a two rect if has a rect 2 lat 1
            if (!!x["Rectangle2 Lat 1"]) {
                let rect: ExclusionTwoRect = {
                    rectangleOne: {
                        topLat: x["Rectangle1 Lat 1"],
                        leftLong: x["Rectangle1 Lon 1"],
                        bottomLat: x["Rectangle1 Lat 2"],
                        rightLong: x["Rectangle1 Lon 2"]
                    },
                    rectangleTwo: {
                        topLat: x["Rectangle2 Lat 1"],
                        leftLong: x["Rectangle2 Lon 1"],
                        bottomLat: x["Rectangle2 Lat 2"],
                        rightLong: x["Rectangle2 Lon 2"]
                    }
                }
                newRegion.exclusionZone = rect;
                newRegion.zoneType = "Two Rectangles";
            } else {
                let rect: ExclusionRect = {
                    topLat: x["Rectangle1 Lat 1"],
                    leftLong: x["Rectangle1 Lon 1"],
                    bottomLat: x["Rectangle1 Lat 2"],
                    rightLong: x["Rectangle1 Lon 2"]
                }
                newRegion.exclusionZone = rect;
                newRegion.zoneType = "One Rectangle";

            }
        } else if (!!x["Circle Radius (km)"]) {
            let circ: ExclusionCircle = {
                latitude: x["Circle center Lat"],
                longitude: x["Circle center Lon"],
                radiusKm: x["Circle Radius (km)"],
            }
            newRegion.exclusionZone = circ;
            newRegion.zoneType = "Circle";
        } else {
            let horz: ExclusionHorizon = {
                latitude: x["Circle center Lat"],
                longitude: x["Circle center Lon"],
                aglHeightM: x["Antenna AGL height (m)"],
            }
            newRegion.exclusionZone = horz;
            newRegion.zoneType = "Horizon Distance"
        }
        return newRegion;
    })
    return objects;
}

const mapDeniedRegionToCsv = (records: DeniedRegion[], regionStr: string, includeHeader: boolean = true) => {
    let result: string[] = [];
    if (includeHeader) {
        result.push(defaultDeniedRegionHeaders);
    }
    let strings = records.filter(x => x.regionStr == regionStr).map((rec) => {
        let header = `${rec.name},${rec.startFreq},${rec.endFreq},${rec.zoneType},`;
        let excl = "";
        switch (rec.zoneType) {
            case "Circle":
                {
                    let x = rec.exclusionZone as ExclusionCircle;
                    excl = `,,,,,,,,${x.radiusKm},${x.latitude},${x.longitude},`;
                }
                break;
            case "One Rectangle":
                {
                    let x = rec.exclusionZone as ExclusionRect;
                    excl = `${x.topLat},${x.bottomLat},${x.leftLong},${x.rightLong},,,,,,,,`;
                }
                break
            case "Two Rectangles":
                {
                    let x = rec.exclusionZone as ExclusionTwoRect;
                    excl = `${x.rectangleOne.topLat},${x.rectangleOne.bottomLat},${x.rectangleOne.leftLong},${x.rectangleOne.rightLong},${x.rectangleTwo.topLat},${x.rectangleTwo.bottomLat},${x.rectangleTwo.leftLong},${x.rectangleTwo.rightLong},,,,`;
                }
                break;
            case "Horizon Distance": {
                let x = rec.exclusionZone as ExclusionHorizon;
                excl = `,,,,,,,,,${x.latitude},${x.longitude},${x.aglHeightM}`;
            }
                break;
            default:
                throw "Bad data in mapDeniedRegionToCsv: " + JSON.stringify(rec)
        }
        return header + excl;
    });
    result = result.concat(strings);
    return result.join("\n");

}



const dummyExclusionZone: ExclusionCircle = { latitude: 0, longitude: 0, radiusKm: 0 }
const defaultDeniedRegionHeaders = "Location,Start Freq (MHz),Stop Freq (MHz),Exclusion Zone,Rectangle1 Lat 1,Rectangle1 Lat 2,Rectangle1 Lon 1,Rectangle1 Lon 2,Rectangle2 Lat 1,Rectangle2 Lat 2,Rectangle2 Lon 1,Rectangle2 Lon 2,Circle Radius (km),Circle center Lat,Circle center Lon,Antenna AGL height (m)"
export const BlankDeniedRegion: DeniedRegion = {
    regionStr: "US",
    name: "Placeholder",
    endFreq: 5298,
    startFreq: 5298,
    exclusionZone: dummyExclusionZone,
    zoneType: "Circle"
}
