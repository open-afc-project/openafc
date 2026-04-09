import { guiConfig, getCSRF, adminEirpMinUrl, adminUserByIdUrl, apDenyAdminUrl, mtlsAdminUrl } from './RatApi';
import {
  UserModel,
  success,
  error,
  AccessPointModel,
  AccessPointListModel,
  FreqRange,
  DeniedRegion,
  ExclusionCircle,
  ExclusionTwoRect,
  ExclusionRect,
  ExclusionHorizon,
  MTLSModel,
} from './RatApiTypes';
import { logger } from './Logger';
import { Role, retrieveUserData } from './User';
import { Rect } from 'react-konva';
import { RatResponse } from './RatApiTypes';

/**
 * Admin.ts: Functions for Admin API. User and account management, and permissions
 * author: Sam Smucny
 */

export class Limit {
  indoorEnforce: boolean;
  outdoorEnforce: boolean;
  indoorLimit: number;
  outdoorLimit: number;
  /** False when Rat DB has no Limit rows; server returns suggested defaults. */
  limitsConfigured = true;

  constructor(enforceIndoor: boolean, enforceOutdoor: boolean, indoorLimit: number, outdoorLimit: number) {
    this.indoorEnforce = enforceIndoor;
    this.outdoorEnforce = enforceOutdoor;
    this.indoorLimit = indoorLimit;
    this.outdoorLimit = outdoorLimit;
  }
}

/**
 * Gets the current Minimum EIRP value
 * @returns object indicating minimum value and whether or not it's enforced if successful, error otherwise
 */
export const getMinimumEIRP = () =>
  fetch(adminEirpMinUrl(), {
    method: 'GET',
  })
    .then(async (res) => {
      if (res.ok) {
        const data = (await res.json()) as Limit & { limitsConfigured?: boolean };
        const lim = new Limit(data.indoorEnforce, data.outdoorEnforce, data.indoorLimit, data.outdoorLimit);
        if (typeof data.limitsConfigured === 'boolean') {
          lim.limitsConfigured = data.limitsConfigured;
        }
        return success(lim);
      } else {
        return error('Unable to load limits', res.status, res);
      }
    })
    .catch((e) => {
      logger.error(e);
      return error('Request failed');
    });

/**
 * Sets the Minimum EIRP value.
 * @param limit the new EIRP value
 */
export const setMinimumEIRP = async (limit: Limit) => {
  const csrf_token = getCSRF();
  return fetch(adminEirpMinUrl(), {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
    body: JSON.stringify(limit),
  })
    .then(async (res) => {
      if (res.ok) {
        return success((await res.json()).limit as Limit);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #1', undefined, err));
};

/**
 * Return list of all users. Must be Admin
 * There is current no support for queried searches/filters/etc. Just returns all.
 * @returns list of users if successful, error otherwise
 */
export const getUsers = () =>
  fetch(adminUserByIdUrl('0'), {
    method: 'GET',
  })
    .then(async (res) => {
      if (res.ok) {
        return success((await res.json()).users as UserModel[]);
      } else {
        return error('Unable to load users', res.status, res);
      }
    })
    .catch((e) => {
      logger.error(e);
      return error('Request failed');
    });

/**
 * gets a single user by id
 * @param id User Id
 * @return The user if found, error otherwise
 */
export const getUser = (id: number) =>
  fetch(adminUserByIdUrl(id), {
    method: 'GET',
  })
    .then(async (res) =>
      res.ok ? success((await res.json()).user as UserModel) : error('Unable to load user', res.status, res),
    )
    .catch((e) => {
      logger.error(e);
      return error('Request failed');
    });

/**
 * Update a user's data
 * @param user User to replace with
 */
export const updateUser = async (user: { email: string; password: string; id: number; active: boolean }) => {
  const csrf_token = getCSRF();
  return fetch(adminUserByIdUrl(user.id), {
    method: 'POST',
    body: JSON.stringify(Object.assign(user, { setProps: true })),
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
  }).then((res) => (res.ok ? success(res.statusText) : error(res.statusText, res.status, res)));
};

/**
 * Give a user a role
 * @param id user's Id
 * @param role role to add
 */
export const addUserRole = async (id: number, role: Role) => {
  const csrf_token = getCSRF();
  return fetch(adminUserByIdUrl(id), {
    method: 'POST',
    body: JSON.stringify({ addRole: role }),
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
  })
    .then((res) => {
      if (res.ok) {
        return success(res.status);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #2', undefined, err));
};

/**
 * Remove a role from a user
 * @param id user's Id
 * @param role role to remove
 */
export const removeUserRole = async (id: number, role: Role) => {
  const csrf_token = getCSRF();
  return fetch(adminUserByIdUrl(id), {
    method: 'POST',
    body: JSON.stringify({ removeRole: role }),
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
  })
    .then((res) => {
      if (res.ok) {
        return success(res.status);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #3', undefined, err));
};

/**
 * Delete a user from the system
 * @param id user'd Id
 */
export const deleteUser = async (id: number) => {
  const csrf_token = getCSRF();
  return fetch(adminUserByIdUrl(id), {
    method: 'DELETE',
    headers: { 'X-CSRF-Token': csrf_token },
  })
    .then((res) => {
      if (res.ok) {
        return success(res.status);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #4', undefined, err));
};

/**
 * Get access points. If `userId` is provided then only return access
 * points owned by the user. If no `userId` is provided then return all
 * access points (must be `Admin`).
 * @param userId (optional) user's Id
 * @returns list of access points if successful, error otherwise
 */
export const getAccessPointsDeny = (userId?: number) =>
  fetch(apDenyAdminUrl(userId || 0), {
    method: 'GET',
  })
    .then(async (res) => {
      if (res.ok) {
        return success(await res.text());
      } else {
        return error('Unable to load access points', res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #8', undefined, err));

/**
 * Register an access point with a user.
 * @param ap Access point to add
 * @param userId owner of new access point
 */
export const addAccessPointDeny = async (ap: AccessPointModel, userId: number) => {
  const csrf_token = getCSRF();

  return fetch(apDenyAdminUrl(userId), {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
    body: JSON.stringify(ap),
  })
    .then(async (res) => {
      if (res.ok) {
        return success((await res.json()).id as number);
      } else if (res.status === 400) {
        return error('Invalid AP data', res.status, res);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #9', undefined, err));
};

/**
 * Post a new deny access point file
 * @param ap Access point to add
 * @param userId owner of new access point
 */
export const putAccessPointDenyList = async (ap: AccessPointListModel, userId: number) => {
  const csrf_token = getCSRF();
  return fetch(apDenyAdminUrl(userId), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
    body: JSON.stringify(ap),
  })
    .then((res) => {
      if (res.ok) {
        return success(res.status);
      } else if (res.status === 400) {
        return error('Invalid AP data', res.status, res);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #10', undefined, err));
};

/**
 * Register an mtls certificate
 * @param mtls cert to add
 * @param userId who creates the new mtls cert
 */
export const addMTLS = async (mtls: MTLSModel, userId: number) => {
  const csrf_token = getCSRF();
  return fetch(mtlsAdminUrl(userId), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': csrf_token },
    body: JSON.stringify(mtls),
  })
    .then(async (res) => {
      if (res.ok) {
        return success((await res.json()).id as number);
      } else if (res.status === 400) {
        return error('Unable to add new certificate', res.status, res);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #11', undefined, err));
};

/**
 * Delete an mtls cert from the system.
 * @param id mtls cert id
 */
export const deleteMTLSCert = async (id: number) => {
  // here the id in the url is the mtls id, not the user id
  const csrf_token = getCSRF();
  return fetch(mtlsAdminUrl(id), {
    method: 'DELETE',
    headers: { 'X-CSRF-Token': csrf_token },
  })
    .then((res) => {
      if (res.ok) {
        return success(undefined);
      } else {
        return error(res.statusText, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #12', undefined, err));
};

/**
 * Get mtls cert.  If `userId` is 0, then return all certificates (super)
 * or all certificates in the same org as the user (`Admin`).
 * 'userId' non zero is not currently supported as certificate do not belong
 * a single user.
 * @param userId user's Id
 * @returns list of mtls certs if successful, error otherwise
 */
export const getMTLS = (userId?: number) =>
  fetch(mtlsAdminUrl(userId || 0), {
    method: 'GET',
  })
    .then(async (res) => {
      if (res.ok) {
        return success((await res.json()).mtls as MTLSModel[]);
      } else {
        return error('Unable to load mtls', res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #13', undefined, err));

export const getDeniedRegions = (regionStr: string) => {
  return fetch(guiConfig.dr_admin_url.replace('XX', regionStr), {
    method: 'GET',
    headers: {
      'content-type': 'text/csv',
    },
  })
    .then(async (res) => {
      if (res.ok) {
        return success(mapDeniedRegionFromCsv(await res.text(), regionStr));
      } else if (res.status == 404) {
        return success([]);
      } else {
        return error('Unable to get denied regions for ' + regionStr, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #14', undefined, err));
};

export const getDeniedRegionsCsvFile = (regionStr: string) => {
  return fetch(guiConfig.dr_admin_url.replace('XX', regionStr), {
    method: 'GET',
    headers: {
      'content-type': 'text/csv',
    },
  })
    .then(async (res) => {
      if (res.ok) {
        return success(await res.text());
      } else {
        return error('Unable to get denied regions for ' + regionStr, res.status, res);
      }
    })
    .catch((err) => error('An error was encountered #15', undefined, err));
};

// Update the denied regions for a given region
export const updateDeniedRegions = async (records: DeniedRegion[], regionStr: string) => {
  const body = mapDeniedRegionToCsv(records, regionStr, true);
  const csrf_token = getCSRF();

  return fetch(guiConfig.dr_admin_url.replace('XX', regionStr), {
    method: 'PUT',
    headers: { 'Content-Type': 'text/csv', 'X-CSRF-Token': csrf_token },
    body: body,
  })
    .then(async (res) => {
      if (res.status === 204) {
        return success('Denied regions updated.');
      } else {
        return error(res.statusText, res.status);
      }
    })
    .catch((err) => {
      logger.error(err);
      return error('Unable to update denied regions.');
    });
};

function parseCSV(str: string, headers = true) {
  const arr: string[][] = [];
  let quote = false; // 'true' means we're inside a quoted field

  // Iterate over each character, keep track of current row and column (of the returned array)
  for (let row = 0, col = 0, c = 0; c < str.length; c++) {
    const cc = str[c],
      nc = str[c + 1]; // Current character, next character
    arr[row] = arr[row] || []; // Create a new row if necessary
    arr[row][col] = arr[row][col] || ''; // Create a new column (start with empty string) if necessary

    // If the current character is a quotation mark, and we're inside a
    // quoted field, and the next character is also a quotation mark,
    // add a quotation mark to the current column and skip the next character
    if (cc == '"' && quote && nc == '"') {
      arr[row][col] += cc;
      ++c;
      continue;
    }

    // If it's just one quotation mark, begin/end quoted field
    if (cc == '"') {
      quote = !quote;
      continue;
    }

    // If it's a comma and we're not in a quoted field, move on to the next column
    if (cc == ',' && !quote) {
      ++col;
      continue;
    }

    // If it's a newline (CRLF) and we're not in a quoted field, skip the next character
    // and move on to the next row and move to column 0 of that new row
    if (cc == '\r' && nc == '\n' && !quote) {
      ++row;
      col = 0;
      ++c;
      continue;
    }

    // If it's a newline (LF or CR) and we're not in a quoted field,
    // move on to the next row and move to column 0 of that new row
    if (cc == '\n' && !quote) {
      ++row;
      col = 0;
      continue;
    }
    if (cc == '\r' && !quote) {
      ++row;
      col = 0;
      continue;
    }

    // Otherwise, append the current character to the current column
    arr[row][col] += cc;
  }

  if (headers) {
    const headerRow = arr[0];
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
  const records = parseCSVtoObjects(data);
  const objects = records.map((x) => {
    const newRegion: DeniedRegion = {
      regionStr: regionStr,
      name: x['Location'],
      endFreq: Number(x['Stop Freq (MHz)']),
      startFreq: Number(x['Start Freq (MHz)']),
      exclusionZone: dummyExclusionZone,
      zoneType: 'Circle',
    };

    //Is a one or two rect if it has a rect1 lat 1
    if (!!x['Rectangle1 Lat 1']) {
      //Is a two rect if has a rect 2 lat 1
      if (!!x['Rectangle2 Lat 1']) {
        const rect: ExclusionTwoRect = {
          rectangleOne: {
            topLat: Number(x['Rectangle1 Lat 1']),
            leftLong: Number(x['Rectangle1 Lon 1']),
            bottomLat: Number(x['Rectangle1 Lat 2']),
            rightLong: Number(x['Rectangle1 Lon 2']),
          },
          rectangleTwo: {
            topLat: Number(x['Rectangle2 Lat 1']),
            leftLong: Number(x['Rectangle2 Lon 1']),
            bottomLat: Number(x['Rectangle2 Lat 2']),
            rightLong: Number(x['Rectangle2 Lon 2']),
          },
        };
        newRegion.exclusionZone = rect;
        newRegion.zoneType = 'Two Rectangles';
      } else {
        const rect: ExclusionRect = {
          topLat: Number(x['Rectangle1 Lat 1']),
          leftLong: Number(x['Rectangle1 Lon 1']),
          bottomLat: Number(x['Rectangle1 Lat 2']),
          rightLong: Number(x['Rectangle1 Lon 2']),
        };
        newRegion.exclusionZone = rect;
        newRegion.zoneType = 'One Rectangle';
      }
    } else if (!!x['Circle Radius (km)']) {
      const circ: ExclusionCircle = {
        latitude: Number(x['Circle center Lat']),
        longitude: Number(x['Circle center Lon']),
        radiusKm: Number(x['Circle Radius (km)']),
      };
      newRegion.exclusionZone = circ;
      newRegion.zoneType = 'Circle';
    } else {
      const horz: ExclusionHorizon = {
        latitude: Number(x['Circle center Lat']),
        longitude: Number(x['Circle center Lon']),
        aglHeightM: Number(x['Antenna AGL height (m)']),
      };
      newRegion.exclusionZone = horz;
      newRegion.zoneType = 'Horizon Distance';
    }
    return newRegion;
  });
  return objects;
};

// Neutralise formula-leading chars and RFC4180-quote string CSV cells
const csvCell = (v: string): string => {
  let s = String(v ?? '');
  if (s.length > 0 && '=+-@\t\r'.indexOf(s.charAt(0)) >= 0) {
    s = "'" + s;
  }
  if (/[",\r\n]/.test(s)) {
    s = '"' + s.replace(/"/g, '""') + '"';
  }
  return s;
};

// Numeric CSV cell: coerce and reject NaN so only a validated numeric
// literal is ever emitted (cannot carry formula or delimiter characters).
// csvCell()'s leading '-' neutralisation is deliberately not applied here
// because it would corrupt legitimate negative coordinates.
const numCell = (v: string | number): string => {
  const n = Number(v);
  if (Number.isNaN(n)) {
    throw 'Bad numeric data in mapDeniedRegionToCsv: ' + JSON.stringify(v);
  }
  return String(n);
};

const mapDeniedRegionToCsv = (records: DeniedRegion[], regionStr: string, includeHeader: boolean = true) => {
  let result: string[] = [];
  if (includeHeader) {
    result.push(defaultDeniedRegionHeaders);
  }
  const strings = records
    .filter((x) => x.regionStr == regionStr)
    .map((rec) => {
      const header = `${csvCell(rec.name)},${numCell(rec.startFreq)},${numCell(rec.endFreq)},${csvCell(rec.zoneType)},`;
      let excl = '';
      switch (rec.zoneType) {
        case 'Circle':
          {
            const x = rec.exclusionZone as ExclusionCircle;
            excl = `,,,,,,,,${numCell(x.radiusKm)},${numCell(x.latitude)},${numCell(x.longitude)},`;
          }
          break;
        case 'One Rectangle':
          {
            const x = rec.exclusionZone as ExclusionRect;
            excl = `${numCell(x.topLat)},${numCell(x.bottomLat)},${numCell(x.leftLong)},${numCell(x.rightLong)},,,,,,,,`;
          }
          break;
        case 'Two Rectangles':
          {
            const x = rec.exclusionZone as ExclusionTwoRect;
            excl = `${numCell(x.rectangleOne.topLat)},${numCell(x.rectangleOne.bottomLat)},${numCell(x.rectangleOne.leftLong)},${numCell(x.rectangleOne.rightLong)},${numCell(x.rectangleTwo.topLat)},${numCell(x.rectangleTwo.bottomLat)},${numCell(x.rectangleTwo.leftLong)},${numCell(x.rectangleTwo.rightLong)},,,,`;
          }
          break;
        case 'Horizon Distance':
          {
            const x = rec.exclusionZone as ExclusionHorizon;
            excl = `,,,,,,,,,${numCell(x.latitude)},${numCell(x.longitude)},${numCell(x.aglHeightM)}`;
          }
          break;
        default:
          throw 'Bad data in mapDeniedRegionToCsv: ' + JSON.stringify(rec);
      }
      return header + excl;
    });
  result = result.concat(strings);
  return result.join('\n');
};

const dummyExclusionZone: ExclusionCircle = { latitude: 0, longitude: 0, radiusKm: 0 };
const defaultDeniedRegionHeaders =
  'Location,Start Freq (MHz),Stop Freq (MHz),Exclusion Zone,Rectangle1 Lat 1,Rectangle1 Lat 2,Rectangle1 Lon 1,Rectangle1 Lon 2,Rectangle2 Lat 1,Rectangle2 Lat 2,Rectangle2 Lon 1,Rectangle2 Lon 2,Circle Radius (km),Circle center Lat,Circle center Lon,Antenna AGL height (m)';
export const BlankDeniedRegion: DeniedRegion = {
  regionStr: 'US',
  name: 'Placeholder',
  endFreq: 5298,
  startFreq: 5298,
  exclusionZone: dummyExclusionZone,
  zoneType: 'Circle',
};
