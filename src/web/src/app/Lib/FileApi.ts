import { axios, createClient, getPatcher } from "webdav";
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
function getUlsFilesOfType(ext: string){
    return (): Promise<RatResponse<string[]>> => 
    createClient(guiConfig.uls_url).getDirectoryContents("")
    .then((files: any) => {
        const names: string[] = files
            .filter((x: any) => x.type === "file")
            .filter((x: any) => x.basename.endsWith(ext))
            .map((x: any) => x.basename);
        return success(names);
    })
    .catch((err: any) => error(err.message, err.statusCode, err))
}

/**
 * Get ULS files that can be used by the AFC Engine
 * @returns ULS files of type `.sqlite3` or error
 */
export const getUlsFiles = getUlsFilesOfType(".sqlite3");

/**
 * Get ULS files that cannot be used by the AFC Engine but can be converted to
 * the compatible sqlite format.
 * @returns ULS files of type `.csv` or error
 */
export const getUlsFilesCsv = getUlsFilesOfType(".csv");

/**
 * Gets all antenna patterns which can be used by the AFC Engine
 * @returns List of antenna pattern names or error
 */
export const getAntennaPatterns = (): Promise<RatResponse<string[]>> => 
    createClient(guiConfig.antenna_url).getDirectoryContents("")
    .then((files: any) => 
    {
        const names: string[] = files
            .filter((x: any) => x.type === "file")
            .filter((x: any) => x.basename.endsWith(".csv"))
            .map((x: any) => x.basename);
        return success(names);
    })
    .catch((err: any) => error(err.message, err.statusCode, err))
    