import { GuiConfig, AFCConfigFile, PAWSRequest, PAWSResponse, AnalysisResults, RatResponse, ResSuccess, ResError, success, error, AFCEngineException, ExclusionZoneRequest, HeatMapRequest, ExclusionZoneResult, HeatMapResult, FreqRange } from "./RatApiTypes";
import { logger } from "./Logger";
import { delay } from "./Utils";
import { addAuth } from "./User";

/**
 * RatApi.ts: member values and functions for utilizing server ratpi services
 * author: Sam Smucny
 */

// Member Definitions

/**
 * Global application index of URLs for API and other configuration variables
 */
export var guiConfig: GuiConfig = Object.freeze({
    paws_url: "",
    uls_url: "",
    antenna_url: "",
    history_url: "",
    afcconfig_defaults: "",
    google_apikey: "",
    rat_api_analysis: "",
    uls_convert_url: "",
    uls_daily_url: "",
    allowed_freq_url: "",
    login_url: "",
    user_url: "",
    admin_url: "",
    ap_admin_url: "",
    lidar_bounds: "",
    ras_bounds: "",
    rat_afc: "",
    version: "API NOT LOADED"
});

/**
 * Storage object to save page state in copied object when travelling between pages.
 * The coppied object is a deep copy and breaks any references as the react component is destroyed.
 * On component mount/dismount it is the responsibility of the component to correctly load/save state.
 * 
 * The key for a cache item must be unique. Either use top level page name or combination of component name and parent component name
 */
const applicationCache: { [k: string]: any } = {};

/**
 * If when the user opens the AFC config page there is no saved config on the server this is used instead.
 * This is a last fallback to prevent undefined exceptions. The server can decide to provide a global default
 * if current user does not have a saved config.
 * @returns Default AFC config object
 */
const defaultAfcConf: () => AFCConfigFile = () => ({
    "freqBands": [           
        {       
            "name" : "UNII-5",
            "startFreqMHz" : 5925,
            "stopFreqMHz" : 6425
        },      
        {       
            "name" : "UNII-7",
            "startFreqMHz" : 6525,
            "stopFreqMHz" : 6875
        }               
    ],
    "antennaPattern":{"kind":'None',"value":''},
    "ulsDefaultAntennaType": 'WINNF-AIP-07',
    "scanPointBelowGroundMethod": "discard",
    "polarizationMismatchLoss":{"kind":"Fixed Value","value":3},
    "bodyLoss":{"kind":"Fixed Value","valueIndoor":0,"valueOutdoor":0},
    "buildingPenetrationLoss":{"kind":"Fixed Value","value":20.5},
    "receiverFeederLoss":{"UNII5":3,"UNII7":2.5,"other":2},
    "fsReceiverNoise":{"UNII5":-110,"UNII7":-109.5,"other":-109},
    "threshold":-6,"maxLinkDistance":50,"maxEIRP":36,"minEIRP":18,
    "propagationModel":{"kind":"FCC 6GHz Report & Order","win2Confidence":50,"itmConfidence":50,"p2108Confidence":50,"buildingSource":"None","terrainSource":"3DEP (30m)"},
    "propagationEnv":"NLCD Point",
    "ulsDatabase":"CONUS_ULS_2022-03-02T03_34_41.097782_fixedBPS_sorted.sqlite3",
    "regionStr":"CONUS",
    "rasDatabase":"RASdatabase.csv",
    "APUncertainty":{"horizontal":30,"height":5},
    "ITMParameters":{"polarization":"Vertical","ground":"Good Ground","dielectricConst":25,"conductivity":0.02,"minSpacing":3,"maxPoints":2000},
    "rlanITMTxClutterMethod": 'FORCE_TRUE' ,
    "clutterAtFS":false,
    "version": guiConfig.version});

// API Calls

/**
 * Retrive basic configuration options used across app
 * and sets the [[guiConfig]] object.
 */
export async function getGuiConfig() {
    await fetch("../ratapi/v1/guiconfig", {
        method: "GET"
    }).then(res => {
        return res.json() as Promise<GuiConfig>;
    }).then(conf => {
        guiConfig = Object.freeze(conf);
        logger.info("server configuration loaded");
    }).catch(err => {
        logger.error(err)
    });
}

/**
 * Return a copy of the hard coded afc confic used as the default
 * @returns The default AFC Configuration
 */
export const getDefaultAfcConf = () => defaultAfcConf();

/** 
 * Return the current afc config that is stored on the server.
 * The config will be scoped to the current user
 * @returns this user's current AFC Config or error
 */
export const getAfcConfigFile = (): Promise<RatResponse<AFCConfigFile>> => (
    fetch(guiConfig.afcconfig_defaults, {
        method: "GET",
        headers: addAuth({})
    }).then(async (res: Response) => {
        if (res.ok) {
            const config = await (res.json() as Promise<AFCConfigFile>);
            return success(config);
        } else {
            logger.error("could not load afc configuration so falling back to dev default");
            return error(res.statusText, res.status, res.body);
        }
    }).catch((err: any) => {
        logger.error(err);
        logger.error("could not load afc configuration so falling back to dev default");
        return error("unable to load afc configuration");
    })
)

/** 
 * Update the afc config on the server with the one created by the user
 * @param conf The AFC Config that will overwrite the server
 * @returns success message or error
 */
export const putAfcConfigFile = (conf: AFCConfigFile): Promise<RatResponse<string>> => (
    fetch(guiConfig.afcconfig_defaults, {
        method: "PUT",
        headers: addAuth({
            "Content-Type": "application/json"
        }),
        body: JSON.stringify(conf,undefined,3)
    }).then(res => {
        if (res.status === 204) {
            return success("AFC configuration updated");
        }
        else {
            return error(res.statusText, res.status);
        }
    }).catch(err => {
        logger.error(err);
        return error("Unable to update configuration");
    })
);

/**
 * Helper method in request polling
 * @param url URL to fetch
 * @param method HTTP method to use
 */
const analysisUpdate = (url: string, method: string) =>
    fetch(url, {
        method: method,
        headers: addAuth({
            "Content-Type": "application/json",
            "Content-Encoding": "gzip"
        })
    })

/**
 * Continues a long running task by polling for status updates every 3 seconds.
 * automatically handles cleanup of the task when completed barring any network interruption.
 * optional parameters can be used to control the process while it is running.
 * @typeparam T The type of result to be returned in promise when task completes
 * @param isCanceled If this callback function exists it will be polled before each status update. If it returns true then the task will be aborted
 * @param status Callback function that is used to update the caller of the task progress until completion
 * @param setKml Callback function that is used to set KML file object if this request generates a KML file output
 * @returns On sucess: The response type T. On failure: error message
 */
function analysisContinuation<T>(isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void, setKml?: (kml: Blob) => void) {
    return async (startResponse: Response) => {
        // use this to monitor and delete the task
        const task = await startResponse.json() as any
        const url: string = task.statusUrl;
        const kml: string | undefined = task.kmlUrl;
        
        // enter polling loop
        while (true) {
            await delay(3000); // wait 3 seconds before polling
            if (isCanceled && isCanceled()) {
                await analysisUpdate(url, "DELETE");

                // exit and return
                return error("Task canceled");
            }
            const res = await analysisUpdate(url, "GET");
            if (res.status === 202 || res.status == 503) {
                // task still in progress
                if (!status || res.status == 503) continue;
                const info = (await res.json() as { percent: number, message: string });
                status(info);
            } else if (res.status === 200 || res.status == 503) {
                if (status)
                    status({ percent: 100, message: "Loading..." })

                // get the result data
                const data = (await res.json() as T);
                if (kml && setKml) {
                    // get kml in background since it is huge
                    analysisUpdate(kml, "GET")
                        .then(async kmlResp => {
                            if (kmlResp.ok)
                                setKml(await kmlResp.blob());
                            await analysisUpdate(url, "DELETE")
                        });
                } else {
                    // delete resource since we are finished
                    await analysisUpdate(url, "DELETE");
                }

                // exit and return
                return success(data);

            } else if (res.status === 550) {

                // AFC Engine encoutered error
                logger.error(res);
                const exception = (await res.json() as AFCEngineException)
                await analysisUpdate(url, "DELETE");

                // exit and return
                return error(exception.description, exception.exitCode, exception.env);
            } else {
                logger.error(res);

                // exit and return
                return error(res.statusText, res.status);
            }
        }
    }
}


/**
 * Run PointAnalysis
 * @param params request parameters
 * @param isCanceled callback which indicates if the task should be terminated
 * @param status callback which is used to notify caller of status of task. The PointAnalysis request provides minimal progress updates
 * @returns Analysis results or error
 */
export const phase1Analysis = (params: PAWSRequest, isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void, setKml?: (kml: Blob) => void): Promise<RatResponse<AnalysisResults>> => (
    fetch(guiConfig.rat_api_analysis.replace("p_request_type", "PointAnalysis"), {
        method: "POST",
        headers: addAuth({
            "Content-Type": "application/json"
        }),
        body: JSON.stringify(params)
    }).then(analysisContinuation<AnalysisResults>(isCanceled, status, setKml))
        .catch(
            err => {
                logger.error(err);
                return error("Your request was unable to be processed", undefined, err);
            })
)

/**
 * Run ExclusionZoneAnalysis
 * @param params request parameters
 * @param isCanceled callback which indicates if the task should be terminated
 * @param status callback which is used to notify caller of status of task. The ExclusionZoneAnalysis request provides minimal progress updates
 * @param setKml Callback function that is used to set KML file object that is generated by the exclusion zone
 * @returns Exclusion zone result or error
 */
export const runExclusionZone = (params: ExclusionZoneRequest, isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void, setKml?: (kml: Blob) => void): Promise<RatResponse<ExclusionZoneResult>> =>
    fetch(guiConfig.rat_api_analysis.replace("p_request_type", "ExclusionZoneAnalysis"), {
        method: "POST",
        headers: addAuth({
            "Content-Type": "application/json"
        }),
        body: JSON.stringify(params)
    })
        .then(analysisContinuation<ExclusionZoneResult>(isCanceled, status, setKml))
        .catch(err => {
            logger.error(err);
            return error("Your request was unable to be processed", undefined, err);
        });

/**
 * Run HeatmapAnalysis
 * @param params request parameters
 * @param isCanceled callback which indicates if the task should be terminated
 * @param status callback which notifies caller of progress updates. Heatmap provides percentages and ETA in message string.
 * @returns Heat map result or error
 */
export const runHeatMap = (params: HeatMapRequest, isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void): Promise<RatResponse<HeatMapResult>> =>
    fetch(guiConfig.rat_api_analysis.replace("p_request_type", "HeatmapAnalysis"), {
        method: "POST",
        headers: addAuth({
            "Content-Type": "application/json"
        }),
        body: JSON.stringify(params)
    })
        .then(analysisContinuation<HeatMapResult>(isCanceled, status))
        .catch(err => {
            logger.error(err);
            return error("Your request was unable to be processed", undefined, err);
        });

/**
 * Convert a ULS file in .csv format to .sqlite3
 * @param fileName `.csv` file to convert
 * @returns Success: number of rows in ULS file that could not be converted. Error: information on why file could not be converted
 */
export const ulsFileConvert = (fileName: string): Promise<RatResponse<{ invalidRows: number, errors: string[] }>> =>
    fetch(guiConfig.uls_convert_url.replace("p_uls_file", fileName), {
        method: "POST",
        headers: addAuth({})
    }).then(
        async res => {
            if (res.ok) {
                const data = (await res.json()) as ({ invalidRows: number, errors: string[] });
                return success(data);
            } else {
                logger.error(res);
                return error(res.statusText, res.status, (await res.json()) as any);
            }
        }
    ).catch(
        err => {
            logger.error(err);
            return error("Your request was unable to be processed", undefined, err);
        }
    )

/**
 * Continues a long running task by polling for status updates every 3 seconds.
 * automatically handles cleanup of the task when completed barring any network interruption.
 * optional parameters can be used to control the process while it is running.
 * @param isCanceled If this callback function exists it will be polled before each status update. If it returns true then the task will be aborted
 * @param status Callback function that is used to update the caller of the task progress until completion
 * @returns On sucess: The result of the parse. On failure: error message
 */
function ulsParseContinuation(isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void) {
    return async (startResponse: Response) => {
        // use this to monitor and delete the task
        const task = await startResponse.json() as any
        const url: string = task.statusUrl;
        // enter polling loop
        while (true) {
            await delay(3000); // wait 3 seconds before polling
            if (isCanceled && isCanceled()) {
                await analysisUpdate(url, "DELETE");

                // exit and return
                return error("Task canceled");
            }
            const res = await analysisUpdate(url, "GET");
            if (res.status === 202) {
                //still running
                //todo : add progress tracking
            } else if (res.status === 200) {
                // get the result data
                const data = (await res.json() as { entriesUpdated: number, entriesAdded: number, finishTime: string });
                // exit and return
                return success(data);
            } else if(res.status == 503) {
                return error('Manual parse already in progress')
            }
            else {
                logger.error(res);

                // exit and return
                return error(res.statusText, res.status);
            }
        }
    }
}


/**
 * Kick off a daily uls parse to create a new ULS database for that day. 
 * @returns Success: number of ULSs updated and number of entries inserted. E
 *          Error: information on why file could not be converted
 */
//(params: HeatMapRequest, isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void): Promise<RatResponse<HeatMapResult>> 
 export const ulsDailyParse = (isCanceled?: () => boolean, status?: (progress: { percent: number, message: string }) => void): Promise<RatResponse<{ entriesUpdated: number, entriesAdded: number, finishTime: string }>> =>
    fetch(guiConfig.uls_daily_url, {
        method: "POST",
        headers: addAuth({})
    }).then(ulsParseContinuation(isCanceled, status))
    .catch(err => {
        logger.error(err);
        return error("Your request was unable to be processed", undefined, err);
    });
        // async res => {
        //     if (res.ok) {
        //         const data = (await res.json()) as { entriesUpdated: number, entriesAdded: number };
        //         return success(data);
        //     } else {
        //         logger.error(res);
        //         return error(res.statusText, res.status, (await res.json()) as any);
        //     }
        // }
    
    
    
    
/**
 * Gets the last successful run time of the daily parse. 
 * @returns Success: An ISO string containing the last run time. 
 *          Error: why it failed
 */
 export const ulsLastRuntime = (): Promise<RatResponse<{ lastSuccessfulRun: string }>> =>
 fetch(guiConfig.uls_daily_url, {
        method: "GET",
        headers: addAuth({ "Content-Type": "application/json"})
    }).then(
        async res => {
            if (res.ok) {
                const data = (await res.json()) as { lastSuccessfulRun: string };
                return success(data);
            } else {
                logger.error(res);
                return error(res.statusText, res.status, (await res.json()) as any);
            }
        }
    ).catch(
        err => {
            logger.error(err);
            return error("Your request was unable to be processed", undefined, err);
        }
 )

 /**
 * Updates the backend celery task for daily parsing to the given time
 * @returns Success: An ISO string containing the new time. 
 *          Error: why it failed
 */
  export const setUlsParseTime = (hours: number, minutes: number): Promise<RatResponse<{ newTime: string }>> =>
  fetch(guiConfig.uls_daily_url, {
         method: "PUT",
         headers: addAuth({}),
         body: JSON.stringify({hours: hours, mins: minutes})
     }).then(
         async res => {
             if (res.ok) {
                 const data = (await res.json()) as { newTime: string };
                 return success(data);
             } else {
                 logger.error(res);
                 return error(res.statusText, res.status, (await res.json()) as any);
             }
         }
     ).catch(
         err => {
             logger.error(err);
             return error("Your request was unable to be processed", undefined, err);
         }
  )

/**
 * Cache an item in the global application cache
 * @param key address to store at
 * @param value object to cache
 */
export const cacheItem = (key: string, value: any) => {
    applicationCache[key] = value;
}

/**
 * Retrieve stored item from the global application cache
 * @param key address of object to retrieve
 * @returns Object if `key` could be found, `undefined` otherwise
 */
export const getCacheItem = (key: string): any | undefined =>
    applicationCache.hasOwnProperty(key) ? applicationCache[key] : undefined;

/**
 * Return a deep copy of the cache object that can be exported
 * @returns deep copy of application state cache
 */
export const exportCache = () => JSON.parse(JSON.stringify(applicationCache));

/**
 * Overwrite properties of existing cache with a new cache
 * @param s cache object with new value to overwrite existing cache with
 */
export const importCache = (s: { [k: string]: any }) =>
    Object.assign(applicationCache, s);
