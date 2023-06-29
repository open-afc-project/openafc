import { guiConfig } from "./RatApi";
import { RatResponse, success, error } from "./RatApiTypes";

/**
 * FileApi.ts: Application API to access Web Dav resources on server
 * author: Sam Smucny
 */

/**
 * Gets all ULS files that have a specific file extension.
 * @param ext file extension to filter on.
 * @returns Promise with list of file or error
 */
function getFilesOfType(url: string, ext: string){
    return (): Promise<RatResponse<string[]>> => (
    fetch(guiConfig.uls_url,{
        method: "GET",
    }).then(async (res: Response) => {
        if (res.ok) {
            const el = document.createElement('html');
            el.innerHTML = await(res.text());
            const td = el.getElementsByTagName("td");
            const len = td.length;
            let names = [];
            for (let i = 0; i < len; i++) {
                if (td[i].children.length > 0 && td[i].textContent.endsWith(ext)) {
                    console.log("Getuls push ", td[i].textContent);
                    names.push(td[i].textContent);
                }
            }
            console.log("Getuls gives success");
            console.log(names[0]);
            return success(names);
        } else {
           return error(res.statusText, res.status, res.body);
        }
    }).catch((err: any) => error(err.message, err.statusCode, err))
)}


/**
 * Get ULS files that can be used by the AFC Engine
 * @returns ULS files of type `.sqlite3` or error
 */
export const getUlsFiles = getFilesOfType(guiConfig.uls_url, ".sqlite3");

/**
 * Get ULS files that cannot be used by the AFC Engine but can be converted to
 * the compatible sqlite format.
 * @returns ULS files of type `.csv` or error
 */
export const getUlsFilesCsv = getFilesOfType(guiConfig.uls_url, ".csv");

/**
 * Gets all antenna patterns which can be used by the AFC Engine
 * @returns List of antenna pattern names or error
 */
export const getAntennaPatterns = getFilesOfType(guiConfig.antenna_url, ".csv");

