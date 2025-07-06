import { guiConfig } from './RatApi';
import { RatResponse, success, error } from './RatApiTypes';

/**
 * FileApi.ts: Application API to access Web Dav resources on server
 * author: Sam Smucny
 */

/**
 * Gets all ULS files that have a specific file extension.
 * @param ext file extension to filter on.
 * @returns Promise with list of file or error
 */
const getFilesOfType = (url: string, ext: string): Promise<RatResponse<string[]>> =>
  fetch(url, {
    method: 'GET',
  })
    .then(async (res: Response) => {
      if (res.ok) {
        const el = document.createElement('html');
        el.innerHTML = await res.text();
        const td = el.getElementsByTagName('td');
        const len = td.length;
        let names = [];
        for (let i = 0; i < len; i++) {
          const trimmed_text = (td[i].textContent ?? '').trim();
          if (trimmed_text.endsWith(ext)) {
            names.push(trimmed_text);
          }
        }
        return success(names);
      } else {
        return error(res.statusText, res.status, res.body);
      }
    })
    .catch((err: any) => error(err.message, err.statusCode, err));

/**
 * Get ULS files that can be used by the AFC Engine
 * @returns ULS files of type `.sqlite3` or error
 */
export const getUlsFiles = (): Promise<RatResponse<string[]>> => {
  return getFilesOfType(guiConfig.uls_url, '.sqlite3');
};

/**
 * Get ULS files that cannot be used by the AFC Engine but can be converted to
 * the compatible sqlite format.
 * @returns ULS files of type `.csv` or error
 */
export const getUlsFilesCsv = (): Promise<RatResponse<string[]>> => {
  return getFilesOfType(guiConfig.uls_url, '.csv');
};

/**
 * Gets all antenna patterns which can be used by the AFC Engine
 * @returns List of antenna pattern names or error
 */
export const getAntennaPatterns = (): Promise<RatResponse<string[]>> => {
  return getFilesOfType(guiConfig.antenna_url, '.csv');
};
