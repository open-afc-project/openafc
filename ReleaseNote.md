# Release Note

## **Version and Date**
|Version|**OA-685*|
| :- | :- |
|**Date**|**06/13/2023**|

## **Issues Addressed**
 * Jira OA-722: Finish Open Issues from OA-685 
 * Jira OA-676: RLAN scan point spacing knob in AFC Config not working properly

## **Interface Changes**
 * OA-722: There are changes in AFC Config and afc-config.json for the ISED DSB-06 propagation model as detailed in OA-722
 * OA-676: There are changes in AFC Config GUI and json file as detailed in OA-676.

## **Testing Done**
 * OA-722: Repeated OA-685 test cases and confirmed that in presence of CDSM data, WinnerII LoS/NLoS is used and in the absence of CDSM data, WinnerII Combined is used.
 * OA-676: ran tests with 3600 and 1800 points-per-deg (30m and 60m spacing respectively) and confirmed in the KMZ files that the scan points were placed correctly. Tests are attached to the ticket.
 * Confirmed that the default AFC Config for all regions sets the Horizontal Plane points per degree to 3600.

## **Open Issues** 
 * 

## **Version and Date**
|Version|**OA-685*|
| :- | :- |
|**Date**|**06/08/2023**|

## **Issues Addressed**
 * Jira OA-685: Canada Propagation Model with CDSM

## **Interface Changes**
 * Make sure the 3/4-arcsec CDSM database converted to wgs84 is at rat_transfer/cdsm/3ov4_arcsec_wgs84
 * Make sure the 1-arcsec 3DEP database converted to wgs84 is at rat_transfer/3dep/1_arcsec_wgs84
 * Change "winner2LOSOption" from "BLDG_DATA_REQ_TX" to "CDSM"

## **Testing Done**
 * Validated that when RLAN is within 1-km of FS and there is CDSM data available, that data is used. Test is attached to the jira ticket with the afc-config.json file.
 * The 'PATH LOSS MODEL' in exc_thr file would show fraction of the path points (b/w RLAN to FS Rx) where there was cdsm and whether WinnerII LOS or NLOS was used. An example is: W2C1_SUBURBAN_LOS cdsmFrac = 1.000000.

## **Open Issues**
 * A new parameter "cdsmLOSThr" needs to be added to afc-config default file.
 * This parameter determines what % of points in ITM path profile need to have surface database in order for the surface database to be used.
 * It takes values between 0 and 1 and default is set to 0.5.
 * When the ISED propagation model is chosen, afc-config.json should have "winner2LOSOption":"CDSM" (in place of "BLDG_DATA_REQ_TX"). The immediate remedy is to change this in the .json file manually before doing a run.

 # Release Note
## **Version and Date**
|Version|3.8.2.0|
| :- | :- |
|**Date**|**06/7/2023**|
|compiled server's version is 51930bc | git tag 3.8.2.0|

## **Issues Addressed**
 * Jira OA-715 Take loglvl from AFC_RATAPI_LOG_LEVEL (#553)
 * Jira OA-709 Use yaml.Loader when yaml.CLoader is unavailable (#552)
 * Jira OA-716 redesigning redirect rules in both apache and nginx (#555)
 * Jira OA-718 adding tzdata (#556)
 * Jira OA-701 Provide mtls certification during regressoin tests (#546)
 * Jira OA-708 Cleaning volumes after compose completion (#551)
 * Jira OA-692 ULS test internal URL (#548)


 # Release Note
## **Version and Date**
|Version|3.8.1.0|
| :- | :- |
|**Date**|**05/29/2023**|
|compiled server's version is bd1bb80 | git tag 3.8.1.0|


## **Version and Date**
|Version|**OA-694*|
| :- | :- |
|**Date**|**05/29/2023**|


## **Issues Addressed**
 * Jira OA-694: put web-gui and msghnd behind nginx saving mTLS auth for API requests

## **Interface Changes**
 * Now all the http services (WebUI (rat_server), msghnd) are to be behind the dispatcher service.
 * From now on the all the HTTP requests by default are redirected to https. To disable this behaviour you should set AFC_ENFORCE_HTTPS variable to FALSE.
 * Removed external ports and VOL_H_APACH VOL_C_APACH variables from apache config.

## **Testing Done**
 * Automated regression testing - passed
 * manual testing from WebUI
 
## **Version and Date**
|Version|**OA-640*|
| :- | :- |
|**Date**|**05/24/2023**|


## **Issues Addressed**
 * Jira OA-640: Generate FS Database for Brazil

## **Interface Changes**
 * A new Brazil FS database is generated for SaoPaulo provided by Anatel. It can be downloaded from: https://drive.google.com/file/d/1Rw8jtgFNkVzQ1xqmwpuby6nPxV65j7yn/view?usp=share_link

## **Testing Done**
 * Validated that the Antenna diameter calculated from F.1245 and F.699 is correct.
 * Ran a test (attached to OA-699) and confirmed in exc_thr file that the data is valid.

## **Open Issues**
 *

## **Version and Date**
|Version|**OA-564*|
| :- | :- |
|**Date**|**05/18/2023**|

## **Issues Addressed**
 * Jira OA-564: RLAN with a directional antenna
 * Jira OA-659: Add Country Boundary for Brazil
 * Jira OA-681: Add Brazilian Propagation Model to AFC Engine
 * Jira OA-638: Add Brazil Region in AFC Config and AFC Engine
 * Jira OA-669: Use Population Density Threshold along with Amazon Rainforest polygon
 * Jira OA-679: Fix the Bug in setting CertificationId in Virtual AP's GUI
 * Jira OA-639: Integrate 1-arcsec SRTM terrain database

## **Interface Changes**
 * OA-638: In AFC Config, there is a BR (Brazil) region with a new 'Brazilian Propagation Model.'
 * In the Access Point and Virtual AP, there is a BRAZIL RULESETID.
 * In Administrator, there's a BR field for Allowed Frequency Range to specify for Brazil.
 * Added "srtmDir":"rat_transfer/srtm3arcsecondv003" in US and Canada AFC Config and "srtmDir":"rat_transfer/srtm1arcsecond_wgs84" in Brazil AFC Config.
 * Note that if "srtmDir" is not specified, it would point to srtm3arcsecond by default.
 * Added "depDir":"rat_transfer/3dep/1_arcsec" in US AFC Config and "depDir":"rat_transfer/3dep/1_arcsec_wgs84" in Canada AFC Config.
 * Removed "LiDAR" and "B-Design3D" from Building data Source options for Brazil and Canada since they are for US only.

 * OA-564: In Virtual AP, there is an ability to add Vendor Extension. This is needed to enter the requried parameters for point-to-point AP.
 * When adding a vendor extension, the parameter field is a copy/paste box so one can open it up if there are a lot of parameters.  
 * It is persnickety about the format so would need to have the "{" and "}" around the entire properties list, put double quotes around the property name, put commas between the parameters, and put double quotes around text values.
 * If there is a problem, it does give the parsing error but it's not always clear what the difficulty is from the error.
 * Note that this is temporary solution to specify the point-to-point AP parameters through Vendor Extension and in a near-future, this will be added to the WFA SDI.
 * In the Vendor Extension for point-to-point, the following parameters need to be specified:
 * "type": "rlanAntenna", "antennaModel": "xyz", "azimuthPointing": -45.0, "elevationPointing": 25.0, "numAOB": 181, "discriminationGainDB": []
 * where the AP pointing direction is specified by the azimuth/elevationPointing parameters. 
 * The antenna pattern is specified as discriminatioGainDB versus angle-off-boresight (AOB) that goes from 0 to 180 degrees.
 * The spacing of the AOB specified is assumed to be constant and based on numAOB. For example, for 1-deg spacing, numAOB is set to 181 and for 0.1 deg spacing, numAOB is set to 1810.
 * The discriminationGainDB is an array that contains numAOB values which are zero or negative numbers. 

 * OA-659: The brazil country boundary (BR.kml) (attached to the jira ticket) is placed at /mnt/nfs/rat_transfer/population/ where the other .kml boundary files are.

 * OA-669: The Amazon Rainforest Polygon (attached to the jira ticket) must be placed at /mnt/nfs/rat_transfer/population/Brazil_AmazonRainForest.kml
 * For BR AFC Config, add "rainForestFile": "rat_transfer/population/Brazil_AmazonRainForest.kml" 

 * OA-639: instructions for downloading the 1arcsec srtm will be provided in github.

## **Testing Done**
 * OA-638: Confirmed that the AFC Config is configured correctly for Brazil (and by default) and the .json has the correct parameters.
 * OA-659: Confirmed that for a Brazil Access point, an AP in US will be rejected based on the BR.kml country boundary.
 * OA-681: Confirmed in the UserInputs file that the Brazilian Propagation Model is mapped to the FCC 6GHz Report & Order for now (as currently, we're using the same model).
 * OA-564: Confirmed in generated AP request message that the Vendor extension for Point-to-Point AP was added successfully in Virtual AP UI.
 * There are 3 new fields in exc_thr.csv: RLAN_ANTENNA_MODEL, RLAN_ANGLE_OFF_BORESIGHT (deg), RLAN_DISCRIMINATION_GAIN (dB)
 * Confirmed in exc_thr file that the I/N level and the EIRP limits are computed correctly based on the RLAN pointing direction and its antenna pattern.
 * Confirmed from exc_thr file and the position of RLAN scan point vs. FS Rx in results.kml that the RLAN to FS AOB is computed correctly.
 * OA-679: Confirmed in GUI that the issue is resovled.
 * Confirmed in afc_config.json that the depDir and srtmDir are present per above for US/CA/BR AFC Configs.
 * Confirmed in engine log that the correct srtm was used for Brazil and the correct 3dep was used for US.
 * OA-669: Ran 2 tests: One with AP inside amazon rainforest and confirmed that tropical rainforest P.452 clutter loss was used correctly.
 * Second test had AP outside amazon rainforest and AP was assigned to Urban correctly. Tests attached to the ticket.

## **Open Issues** 
 * 

## **Version and Date**
|Version|**OA-678*|
| :- | :- |
|**Date**|**05/17/2023**|


## **Issues Addressed**
 * Jira OA-678: channelCfi [] not working inside GUI

## **Interface Changes**
 * 

## **Testing Done**
 * Entered the following AP request in Virtual AP Copy/Paste and confirmed that ALL of channels of Global Operating Class 134 were selected
 * {"inquiredChannels": [{"channelCfi": [],"globalOperatingClass": 134}], "deviceDescriptor": {"rulesetIds": ["US_47_CFR_PART_15_SUBPART_E"], "serialNumber": "REG123", "certificationId": [{"nra": "FCC", "id": "FCCID-REG123"}]}, "inquiredFrequencyRange": [], "location": {"indoorDeployment": 1, "elevation": {"verticalUncertainty": 2, "heightType": "AGL", "height": 3}, "ellipse": {"center": {"latitude": 33.180621, "longitude": -97.560614}, "orientation": 45, "minorAxis": 50, "majorAxis": 100}}, "requestId": "REQ-FSP1"}

## **Open Issues** 
 * 

## **Version and Date**
|Version|3.8.0.0|
| :- | :- |
|**Date**|**05/16/2023**|
|compiled server's version is f38e78d | git tag 3.8.0.0|


## **Version and Date**
|Version|**OA-564*|
| :- | :- |
|**Date**|**05/12/2023**|

## **Issues Addressed**
 * Jira OA-682: Angle off boresight calculation when FS outside but within 1arcsec of ellipse
 * Jira OA-564: RLAN with a directional antenna
 * Jira OA-659: Add Country Boundary for Brazil
 * Jira OA-681: Add Brazilian Propagation Model to AFC Engine

## **Interface Changes**
 * None of OA-682

## **Testing Done**
 * The issue was found in FSP2. This was tested again and got expected results.
 * Also tested FSP1 (where there was no impact) and found same results as before (except a lower PSD over 5930-5990 MHz as expected).
 * Made up linear and radial polygon tests over FSP2 and FSP1 regions.
 * Test results are attached to OA-682

## **Open Issues** 
 * The brazil jira tickets are yet to be tested. We're pushing this branch to get the fix for the 682 bug in ASAP as this is needed for WFA TVs.

## **Version and Date**


|Version|**OA-582**|
|**Date**|**05/09/2023**|
## **Issues Addressed**
 * Jira OA-582: Add support for denied devices.  UI for uploading and download list is under "Denied Rules" tab, which now contains both denied devices and denied regions. Denied devices can be specified by both certification id and serial number, or by just the certification id.  In the latter case, the serial number field can be filled in with * or None.
  Since database version changes, need to run  rat-manage-api db-upgrade once from shell upon first time running container with this new version.

## **Version and Date**

|Version|**OA-595, OA-587**|
|**Date**|**05/15/2023**|
## **Issues Addressed**
 * Jira OA-595: Authorization of FCC 6SD and 6SD+6ID certified devices.  The 6SD query is not currently available
 * Jira OA-585: Device authentication by checking with ISED database per DBS-06.
 * Since database version changes, need to run  rat-manage-api db-upgrade once from shell upon first time running container

## **Interface Changes**
  Removed the AP tab, since the AFC server now no longer maintains a list of AP devices. 

|Version|**OA-582**|
|**Date**|**05/09/2023**|
## **Issues Addressed**
 * Jira OA-582: Add support for denied devices.  UI for uploading and download list is under "Denied Rules" tab, which now contains both denied devices and denied regions. Denied devices can be specified by both certification id and serial numbe r, or by just the certification id.  In the latter case, the serial number field can be filled in with * or None.  Since database version changes, need to run  rat-manage-api db-upgrade once from shell upon first time running container

## **Version and Date**
|Version|**OA-634**|
| :- | :- |
|**Date**|**05/08/2023**|

## **Issues Addressed**
 * Jira OA-634: Update WFA SDI to Version 1.4 from 1.3 with backward compatibility to V1.3

## **Interface Changes**
 *  Changes are done to Virtual AP page for compliance with SDI V1.4
   * Changed the device descriptor to use rulesetId instead of NRA
   * Changed the python API to authenticate against the rulset ID
   * Changed the server path for virtual AP requests to /1.4/availableSpectrumInquiry           
 *  In addition, in Virtual AP, one can copy-in an V1.3 AP request.json (note that the GUI parameters won't be updated) to run a v1.3 SDI.

## **Testing Done**
 * Entered a v1.3 AP request and got same AP response as before with version 1.3 in the response message
 * Ran FSP1 under v1.4 and got identical results as v1.3. 
 * Confirmed that for v1.4, AP request and response message formats are correct and have version 1.4.
 * Ran a test with rulsetId for ISED and another a dummy one and got correct resuts.

## **Open Issues** 
 * Need to confirm with WFA the proper Error code when rulesetId is not set to one of the valid values.
 
|Version|**OA-636**|
| :- | :- |
|**Date**|**04/28/2023**|


## **Issues Addressed**
 * Jira OA-636: Create UI form for denied region


## **Interface Changes**
 * The denied region files are being saved to rat_transfer/denied_regions/<COUNTRY>_denied_regions.csv so US_denied_regions.csv, CA_denied_regions.csv and the config editor is updated to set that path correctly for the region


## **Testing Done**
 * Tested (in both US and CA), the different denied regions (circle, one rectangle, two rectangles and horizon distance). Tests are attached to the jira ticket.

## **Open Issues** 
 * Make the denied region list editable in the UI (OA-667) 

## **Version and Date**
|Version|**OA-657**|
| :- | :- |
|**Date**|**05/03/2023**|


## **Issues Addressed**
 * Jira OA-657: Implement WINNF option 2b where FS inside uncertainty volume, or above/below the uncertainty volume
 * Jira OA-670: Add additional information in engine-log to indicate whether the primary/diversity/passive-site is inside RLAN uncertainty region

## **Interface Changes**
 *  There are 3 new parameters in afc-config.json. These are: "reportUnavailableSpectrum": true, "reportUnavailPSDdBPerMHz": -40, "inquiredFrequencyResolutionMHz": 1
 *  The first 2 parameters allow setting PSD levels to -40 dBm/MHz (instead of no reporting) ONLY when FS is inside the uncertainty volume. To report no PSD/EIRP values (as the previous implementation) set "reportUnavailableSpectrum": false. 
 * The 3rd parameter allows doing the PSD computations per x MHz where x is configurable (previously this was 20 MHz, and now we'll be using 1 MHz).
 * Above are the default values we want to use for WFA testing.
 * In addition, there's a hidden new parameter in afc config (not set in .json) which is: INQUIRED_FREQUENCY_MAX_PSD_DBM_PER_MHZ,22.989700043360187 (this is in the userInputs file generated under Debug folder).

## **Testing Done**
 * Test to use same min-angle-off-boresight for all RLAN scan points to the same FS when FS is outside RLAN uncertainty volume (but inside RLAN's footprint): FSP-4 has 5 FS (IDs: 27752, 29885, 42201, 65560, 65561) that are outside uncertainty volume at heights much higher than RLAN (they're on top of a very tall building with AGL height between 356 to 443m while RLAN max AGL height of 110m). Confirmed exc_thr file that for each FS, min angle off boresight (to the uncertainty volume) was used for all RLAN scan points and as such same FS Rx Gain was applied. Also, confirmed that FSPL path lossed was used for all these points.
* In addition FSP-4 has FS ID 109944 and 109945 that are outside the RLAN uncertainty footprint but within the touching 3DEP tile. Confirmed that the min angle off boresight is used for all RLAN scan points. Note that FS ID 109945 has a diversity antenna so a different min angle off boresight is used for that.
* For FS inside uncertainty volume: ran FSP-13 which has FS IDs 8177, 4140 and 28023 inside, and confirmed that the channels were set to PSD of -40 dBm/MHz.
* Note that above tests were tested before implementing updated psd calculations. Tests below are with the full implementation.
* Tested FSP1 where there are a few FS inside RLAN uncertainty footprint but outside volume. Tested with 20MHz PSD calculation and 1 MHz PSD.
* For 20MHz psd, confirmed that we get same results as before for the channels that are not impacted (also for all of FSP14).
* For 1MHz psd, confirmed that the psd levels were always higher than the case with 20MHz. 
* For 1MHz psd, confirmed getting same results as QCOM for some FSPs (e.g. 14, 1 and 2) over freqs where we have same implementation 

## **Open Issues** 
 *

## **Version and Date**
|Version|3.7.7.0|
| :- | :- |
|**Date**|**04/16/2023**|
|compiled server's version is TBD | git tag 3.7.7.0|
## **Issues Addressed**
 * Jira OA-633-fix-demo-test-use-cases-with-regional-afc-configs
 * Jira OA-650-upon-delete-rebuild-certificate-bundle-file
 * Jira OA-652-geospatial-files-conversion-scripts
 * Jira OA-641-move-configs-to-appcfg-py
 * Jira OA-651-missing-sqlalchemy-in-uls-after-worker-base-cleanup
 * Jira OA-649-rename-nginx-container-to-dispatcher
 * Jira OA-511-ability-to-update-nginx-certificates-file
 * Jira OA-644-define-broker-configuration-in-appcfg-module


## **Version and Date**
|Version|**OA-658**|
| :- | :- |
|**Date**|**04/21/2023**|

## **Issues Addressed**
 * Jira OA-658: bug in daily_uls_parser corrupt fsid_table
 * Download updated fsid_table.csv from https://drive.google.com/drive/folders/1O6bx3uZyinnnKL8Z-04a6DTnU7gX6bxx and place it here: /mnt/nfs/rat_transfer/daily_uls_parse/data_files/fsid_table.csv


## **Interface Changes**
 *  the FS parser was updated.


## **Testing Done**
 * Re-ran the FS parser and validated that the issue is resolved.

## **Open Issues** 
 
## **Version and Date**
|Version|3.7.6.0|
| :- | :- |
|**Date**|**04/16/2023**|
|compiled server's version is TBD | git tag 3.7.6.0|
## **Issues Addressed**
 * JIRA OA-647-update-regression-tests
 * JIRA OA-643-worker-log-redirection-to-docker-log
 * JIRA OA-591-separate-worker-python-module
 * JIRA OA-632-fix-bug-when-indoor-fixed-height-amsl-is-set-to-true
 * JIRA OA-623_AboutPageUpdates
 * JIRA OA-635-missing-response-file-from-history-directory
 * JIRA OA-601-setup-country-specific-allowed-freq-ranges-in-admin-page
 * JIRA OA-631-issue-with-the-latest-default-afc-config
 * JIRA OA-630-prepare-application-configuration-as-independent-module
 * JIRA OA-580-list-of-denied-geographic-areas
 * JIRA OA-613-DEBUG_AFC-bugfix

## **Version and Date**
|Version|**OA-632**|
| :- | :- |
|**Date**|**04/10/2023**|


## **Issues Addressed**
 * Jira OA-632: Fix bug when "indoorFixedHeightAMSL" is set to TRUE


## **Interface Changes**
 * Added a new parameter (it was available to AFC Engine before but not in AFC Config) to afc-config.json: “indoorFixedHeightAMSL”:false (by default this will be set to false)


## **Testing Done**
 * Performed tests with “indoorFixedHeightAMSL”:false and “indoorFixedHeightAMSL”:true and confirmed in exc_thr file that different or same terrain heights are used for RLAN scan points. See test attached to this ticket for when this is set to True.

## **Open Issues** 
* 
## **Version and Date**
|Version|**OA-601**|
| :- | :- |
|**Date**|**03/31/2023**|


## **Issues Addressed**
 * Jira OA-601: Setup Country-specific Allowed Freq Ranges in Admin page


## **Interface Changes**
 * Under the Administoration tab, the admin would need to define the "allowed frequency ranges" (that's used by AFC Config) for each region (i.e. country) separately. The AFC Config then pulls the correct for a given region.
 * added an optional property region to records in the rat_transfer/frequency_bands/allowed_frequencies.json file that specifies the region for the given frequency range.  If not provided, the system defaults to US


## **Testing Done**
 * OA-601: Confirmed after defining the allowed frequency ranges under Admin, the AFC Config shows the correct one for US vs. Canada. Also, when doing an AP request, the correct allowed frequency range is used.

## **Open Issues** 
* 


|Version|**OA-631**|
| :- | :- |
|**Date**|**04/10/2023**|


## **Issues Addressed**
 * Jira OA-631: Issue with the latest default afc-config


## **Interface Changes**
 * OA-631: fixed the bug at the interface.

## **Testing Done**
 * When doing 'reset to default' of AFC Config, confirmed that this parameter is set properly: "nlcdFile":"rat_transfer/nlcd/nlcd_wfa"

## **Open Issues** 
* None


## **Version and Date**
|Version|**OA-580**|
| :- | :- |
|**Date**|**03/30/2023**|


## **Issues Addressed**
 * Jira OA-580: List of denied geographic areas

 * Jira OA-617: Update US RAS Database
 * The only change was the RAS database for the US in the FS database.sqlite3 file. 
 * Please download the updated file from: https://drive.google.com/file/d/1rldFtY89-33q8xBYlKth8AaLp2fYzOMp/view?usp=share_linkusp=share_link
 * This FS database is: FS_2023-03-28T02_12_59.301827_fixedBPS_sorted_param.sqlite3

 * Jira OA-622: RAS exclusion not handled properly

## **Interface Changes**
 * OA-580: A new parameter has been added to afc-config.json. This parameter is "deniedRegionFile":"" which is currently pointing to a blank file since we don't have a denied geographic region. If we do have one, we would have "deniedRegionFile": "rat_transfer/denied_region/filename.csv"
 * In other words, a new directory called "denied_region" needs to be added under rat_transfer and the denied_region file would be placed there. Note that this was a requirement for Canada AFC but it can be used for US AFC as well if desired.


## **Testing Done**
 * OA-617: confirmed in .sqlite3 that the RAS database was updated correctly.
 * OA-622: Confirmed that the WFA TVs that previously had incorrect responses were corrected. These are FSP77 through FSP84.
 * OA-580: Tested a denied region and confirmed that if RLAN lands in that region, the channels overlapping the range per the denied_region_file will be blocked.

## **Open Issues** 


## **Version and Date**
|Version|3.7.5.0|
| :- | :- |
|**Date**|**03/25/2023**|
|compiled server's version is edce4c9 | git tag 3.7.5.0|
## **Issues Addressed**
 * JIRA OA-592-upgrade-postgres
 * JIRA OA-612-runtime_opt-bit-for-heavy-logs
 * JIRA OA-600-size-limit-of-afc-engine-file-cache
 * JIRA OA-610-GzipCsv-merge-fix
 * JIRA OA-465-rat-server-objst-service-discovery-issue
 * JIRA OA-590-engine-computation-comparison-log
 * JIRA OA-574-add-country-boundary-for-canada
 * JIRA OA-606-update-fs-file-in-afc-config
 * JIRA OA-570-fixing-canada-fs-parameters
 * JIRA OA-604_about_fix
 * JIRA OA-602_user_account


## **Version and Date**
|Version|**OA-574**|
| :- | :- |
|**Date**|**03/17/2023**|


## **Issues Addressed**
 * Jira OA-574: Add Country Boundary for Canada
 * This includes a few bug fixes found in OA-570 ("Fixing Canada FS parameters") upon validation
 * There is a new FS Database with a bug fix of matching Canada's Passive Site with Rx.
 * The new FS database can be downloaded from: https://drive.google.com/file/d/1Xv1i1r0StIvmuXyO5d5BcY8yDkmrU5l9/view?usp=share_link
 * This FS database is: FS_2023-03-17T18_37_00.293116_fixedBPS_sorted_param.sqlite3

## **Interface Changes**
 * Changed the way the country boundary is used. The engine now uses the regionStr in afc-config.json that is set to country's 2-letter code (e.g. US, CA) and uses the kml filename that matches that country's code (e.g. US.kml or CA.kml). These files are stored under rat_transfer/population.



## **Testing Done**
 * OA-574: Ran FSP1 but for ISED-certified AP and confirmed that AP in US would get rejected since it's outside of Canadian boundary CA.kml. 
 * Confirmed that when running FCC-certified APs, US AFC is used and when runnig ISED-certified APs, CA AFC is used. See test attached to this ticket.
 * OA-570: Created Canada links with missing parameters (Rx bandwidth, emissionDesignator, Rx gain, Rx antenna model, Rx height, Passive repeater/reflector height, passive reflector horizontal and/or vertical dimension) and confirmed in .sqlite3 FS database that they were set correctly. This file (the .csv version) is attached to this ticket for reference on what was validated.
 * OA-570: For missing feederloss and antenna pattern, confirmed in exc_thr file that 0 dB feederloss and the SRSP antenna pattern was used. See test attached to this ticket.
 * OA-572: Confirmed in exc_thr file that the correct Noise Floor was used

## **Open Issues** 
* We have yet to test Canada's propagation model with Canada's Digital Surface Model Database

## **Version and Date**
|Version|**OA-570**|
| :- | :- |
|**Date**|**03/14/2023**|


## **Issues Addressed**
 * Jira OA-570: Fixing Canada FS parameters
 * Jira OA-603: Bandwidth Issue Resolution
 * This branch includes the following tasks for Canada AFC: OA-570, OA-545 (Generate a new Propagation model for Canada), OA-572 (FS Noise Figure), OA-546 (Setup AFC Config to be able to switch to Canada's configuration), OA-543 (Generate a new defaulg AFC Config for Canada). However, most the Canada tickets have not been tested and we are doing this PR as an emergency for getting OA-603 in. After this, we will start validating the Canada tickets and if we find a bug, we will open a new ticket. In addition, some of this tickets (e.g. OA-572) resulted in a change for US as well. As such all US changes have been validated as part of this PR.

## **Interface Changes**
 * There was a change in the ULS parser which resulted in a new FS database:
 * This new FS database can be downloaded from: https://drive.google.com/file/d/1ogV5MESr0Ml2EE78C6KR03hAOPxvG_cE/view

 * There are AFC Config GUI changes (for FS Noise Floor, use of Land Cover instead of NLCD when referring to this database generically).
 * There are 2 other changes in afc-config.json for US: 1) "fsReceiverNoise": {"freqList": [6425], "noiseFloorList": [-110, -109.5]} 2) "nearFieldAdjFlag": true

## **Testing Done**
 * Ran FSP1, FSP41, SIP10 and IBP3 and got same results as last time. 
 * Manually validated that OA-603 was implemented correctly in the FS database (US).

## **Open Issues** 
* We have yet to validate the following Canada tickets: OA-570, OA-572, OA-545, oA-546 and OA-543

## **Version and Date**
|Version|3.7.4.0|
| :- | :- |
|**Date**|**02/12/2023**|
|compiled server's version is b72f6f6 | git tag 3.7.4.0|
## **Issues Addressed**
 * JIRA OA-542-support-of-unii-6-in-afc-engine
 * JIRA OA-577 new golden resp with new FS/ULS database  which can be downloaded from https://drive.google.com/file/d/1OPBYHZ1iXR_M4NKelVHridK8bVpJSxPQ/view?usp=share_link.
 * JIRA OA-588-remove-outdated-centos-7-worker
 * JIRA OA-509 whitespace and copyright header update
 * JIRA OA-589 Fix request with mTLS option
 * JIRA OA-568 fixing paths for ULS canada based changes and switching updater to Alpine base
 * JIRA OA-593 About Page for new user registration
 * JIRA OA-596 Fix aep env var defaults
 * JIRA OA-597 decrease-num-of-celery-proc to N=2
 * JIRA OA-594_log_afc_config
 * JIRA [dependabot] Bump werkzeug from 2.0.3 to 2.2.3 in /rat_server (#441)
 * JIRA [dependabot] Bump wsgidav from 3.1.1 to 4.1.0 in /msghnd (#351)
 * JIRA [dependabot] Bump werkzeug from 2.0.3 to 2.2.3 in /msghnd (#436)
 * JIRA OA-585 new golden resp with FS_2023-03-10T03_32_19.614444_fixedBPS_sorted_param.sqlite3) from here https://drive.google.com/file/d/1Bfb7Hy2lPbtIJMdalUIEfsH0AWXq_Etj/view
 * JIRA OA-585-fixes-required-in-itm

## **Version and Date**
|Version|**OA-585**|
| :- | :- |
|**Date**|**03/09/2023**|


## **Issues Addressed**
 * Jira OA-585: Fixes required in ITM
 
## **Interface Changes**
 * None

## **Testing Done**
 * Ran FSP1, FSP41, FSP82, SIP10, IBP1 and IBP3 and resutls were as expected. The package is attached to this Jira ticket.

## **Open Issues** 



## **Version and Date**
|Version|**OA-577**|
| :- | :- |
|**Date**|**03/01/2023**|


## **Issues Addressed**
 * Jira OA-577:Use of incorrect WINNF-R2AIP07 pattern
 * Jira OA-578:Need to do exact-match to standardModel of antenna_model_diameteter_gain data
 
## **Interface Changes**
 * There has been changes to the FS parser and as such there's a new FS database. 
 * The new FS database is "FS_2023-03-01T04_39_37.715373_fixedBPS_sorted_param.sqlite3" which can be downloaded from https://drive.google.com/file/d/1OPBYHZ1iXR_M4NKelVHridK8bVpJSxPQ/view?usp=share_link.
 
## **Testing Done**
 * OA-578: Examined FS database for FS ID 2989 and confirmed that the correct antenna was matched (and hence correct Gain was used). Also confirmed the reults in FSP41.
 * OA-577: Confirmed in FS database that when Rx Antenna model is blank, Ant Category is set to B1. Also, ran a test for FS ID 1 and confirmed that the Engine applied Category B1 pattern. See test attached to this ticket.
 * OA-577: Confirmed in FS database that when Rx Antenna model is not in the antenna model-diameter-gain dataset, Ant Category is set to UNKNOWN (as desired) - in which case Category A is applied. An example is FS ID 76591.
 * Ran FSP82, FSP41 and FSP48 and confirmed that the previous issues were resolved.

## **Open Issues** 

## **Version and Date**
|Version|3.7.3.0|
| :- | :- |
|**Date**|**22/02/2023**|
|compiled server's version is TBD | git tag 3.7.2.0|
## **Issues Addressed**
 * JIRA OA-573-use-preinstalled-ca-certificates-in-test-utility
 * JIRA OA-537-create-afc-server-receiver-based-alpine-and-python-3
 * JIRA OA-571-nlcd_to_wgs84.py_improvements
 * JIRA OA-566_ALS_logs_improvements
 * Jira OA-559:Integrate afc_antenna_patterns with FS database.sqlite3 file
 * Jira OA-534:Update RAS Database daily to include Canada RAS


## **Version and Date**
|Version|**OA-559**|
| :- | :- |
|**Date**|**02/14/2023**|


## **Issues Addressed**
 * Jira OA-559:Integrate afc_antenna_patterns with FS database.sqlite3 file
 * Jira OA-534:Update RAS Database daily to include Canada RAS
 
## **Interface Changes**
 * There has been changes to the ULS parser to integrate the RAS database and the afc_antenna_patterns file data with the .sqlite3 since both are processed from the same source as the FS database.
 * Recommend removing the entire Anteanna_Patterns and RAS_Database directories.
 * Removed the rasdatabase and antennapatterns in afc-config.json.
 
## **Testing Done**
 * OA-559: ran FSP1 with ISED patterns used for CA only and got same results as before. (test attached to ticket)
 * OA-559: ran FSP1 with ISED patterns used for CA & US and got same results as r18 (almost) when this was used. Confirmed in exc_thr that the pattern was used correctly. (test attached to ticket)
 * OA-534: ran SIP1 and SIP13 with ISED patterns used for CA only and got same results as before. 
 * OA-559: created a test with back-to-back Passive Repeater (FS ID 546) that uses antenna pattern and it used it correctly. (test attached to ticket)
 
## **Open Issues** 

## **Version and Date**
|Version|3.7.2.0|
| :- | :- |
|**Date**|**15/02/2023**|
|compiled server's version is acfe67b | git tag 3.7.2.0|
## **Issues Addressed**
 * OA-549 Establish sending logs from AFC server to PostgreSQL/PostGIS database
 * OA-562 create-new-test-results-using-new-kml-file-from-oa-536
 * OA-563 Add configurable MSGHND parameters
 * OA-540 Include hash in path to afcconfig cached object.


## **Version and Date**
|Version|3.7.1.0|
| :- | :- |
|**Date**|**09/02/2023**|
|compiled server's version is 5609349 | git tag 3.7.1.0|

## **Issues Addressed**
 * OA-561 rel 3.7.1.0
 * Jira OA-536: Update Feederloss implementation per new agreement b/w WFA and WINNF
 * Jira OA-539: Make use/not-use of ISED antenna patterns for FS in US configurable
 * Jira OA-556: Bug fix in calculation of RAS exclusion zone horizon distance
 * Jira OA-446: Create a parser for Canada's incumbents database (this includes changes required to allow protection of Canadian FS at the border)
 * Jira OA-275: Integrate actual antenna patterns (from ISED and WinnForum) in AFC (this is implemented as part of OA-446)
 * Jira OA-533: Check AP's uncertainty region's center lat/long against country (based on rulsetId) boundaries
 * Jira OA-259: Align with QCOM/Federated to select same 3DEP/NLCD tile when on Boundary. 
 * Jira OA-535: Correction of FS bandwidth from Emission Designator vs. upper-lower freq not matching
 
## **Version and Date**
|Version|**OA-536**|
| :- | :- |
|**Date**|**02/08/2023**|

## **Issues Addressed**
 * Jira OA-536: Update Feederloss implementation per new agreement b/w WFA and WINNF
 * Jira OA-539: Make use/not-use of ISED antenna patterns for FS in US configurable
 * Jira OA-556: Bug fix in calculation of RAS exclusion zone horizon distance
 
## **Interface Changes**
 * Updated AFC Config GUI to enter feederloss for radio type: IDU(IndoorUnit), ODU(OutdoorUnit) or Unknown separately
 * Updated ULS parser and the ULS to include the radio architecture that is used by the AFC Engine to use the appropriate feederloss
 * There is a new WFA test vector ULS that needs to be used is FS_2023-02-07T20_37_35.958370_fixedBPS_sorted_param.sqlite3 which can be downloaded from: https://drive.google.com/file/d/1crjizDwM-aGzfcNFgwbTA2cU4v_XXVr8/view?usp=share_link
 * There is a new file from WinnForum "transmit_radio_unit_architecture.csv" that is read by the ULS parser. This needs to be placed at: /mnt/nfs/rat_transfer/daily_uls_parse/temp/
 * For OA-539, there is an updated afc_antenna_patterns.csv file where the ISED antenna models are prefixed with CA and there are no US ones (as agreed). This should be placed at /mnt/nfs/rat_transfer/Antenna_Patterns/
 
## **Testing Done**
 * OA-536: In the intermediate ULS.csv, the new fields "Tx Model Matched" and "Tx Architecture" are used to validate whether the lookup was done correctly.
 * In the WFA ULS, out of 106,525 US FS, 62.4% are IDU, 19.9% ODU and the remaining 17.7% are UNKNOWN.
 * Ran FSP1 and confirmed in exc_thr file that the correct feederloss (IDU vs. ODU) was used for the FS per AFC Config. 
 * OA-539: Ran FSP1 and confirmed that the correct antenna pattern was used for the US FS. Ran IBP2 and confiremd that the correct antenna pattern was used for the CA and US FS.
 * FSP1 test results are attached to OA-536.
 * IBP2 test results are attached to OA-539.
 * SIP-16 test results are attached to OA-556. 

## **Open Issues** 
 * If an antenna pattern for Canadian FS is missing and the FS channel is in UNII-6, engine would crash since the default R2-AIP-07 does not currently cover this case.

## **Version and Date**
|Version|**OA-446**|
| :- | :- |
|**Date**|**01/27/2023**|


## **Issues Addressed**
 * Jira OA-446: Create a parser for Canada's incumbents database (this includes changes required to allow protection of Canadian FS at the border)
 * Jira OA-275: Integrate actual antenna patterns (from ISED and WinnForum) in AFC (this is implemented as part of OA-446)
 * Jira OA-533: Check AP's uncertainty region's center lat/long against country (based on rulsetId) boundaries
 * Jira OA-259: Align with QCOM/Federated to select same 3DEP/NLCD tile when on Boundary. 
 * Jira OA-535: Correction of FS bandwidth from Emission Designator vs. upper-lower freq not matching
 
## **Interface Changes**
 * There has been significant changes to the ULS-parser to read Canada's incumbent database. 
 * Made changes to AFC Config GUI: changed "CONUS" to "USA" under Country and changed "ULS" to "FS" for database. 
 * Changed ULS database filename to "FS" in place of "CONUS_ULS," since it now contains both US and Canada.
 * WFA Test Vector ULS has been updated to "WFA_testvector_ULS_2023-01-27T17_14_43.095504.sqlite3" that can be downloaded from https://drive.google.com/file/d/1UBOqOmWmX44XUj5f0ZFqlkCV9hj9Gs_k/view?usp=share_link
 * Moved the Antenna_Patterns directory (where there will be afc_antenna_patterns.csv generated daily by the parser from ISED's antenna patterns) to /mnt/nfs/rat_transfer
 * For OA-533, created a KML file containing USA boundaries (CONUS, Alaska, Hawaii, Puerto-Rico and US Virigin Islands) and put it under /mnt/nfs/rat_transfer/population/USA_PRI_VIR.kml
 * Above KML was created from the Shape file (cb_2018_us_nation_5m.zip) under https://www.census.gov/geographies/mapping-files/time-series/geo/carto-boundary-file.html.
 * The county-boundary KML will be used to check whether RLAN's lat/long is inside the US. In future, additional country boundaries will be added to this KML to support Canada and other countries.
 * The changes for OA-259 are done in ULS Parser.
 
## **Testing Done**
 * OA-446 and OA-275: Significant validation was done on correct population of CONUS_ULS database with ISED's database. This included confirming generation of afc_antenna_patterns.csv file. 
 * In exc_thr file, if column FS_ANT_TYPE shows an antenna model, that pattern was used.
 * Confirmed from fs_analysis_list.csv generated from FSP1 that we get same intermediate/end values for passive reflectors as before.
 * Confirmed that we get same results as before for FSP77 which tested back-to-back passive repeaters.
 * OA-533: ran IBP-1 through 4 and confirmed that the border detection is working as expected. If the AP is outside US, Error Code 103 (invalid parameter) is generated.
 * OA-259: Confirmed from exc_thr of FSP-17 for FS ID 4140 with latitude on 3DEP tile border, that the same 3DEP tile as QCOM/Federated is chosen (by design). 
 * OA-535: Confirmed in WFA test vector ULS, that the number of US is higher by 99 (expected - fixed a bug) and the 2 US links with incorrect lower/upper freq were fixed.

## **Open Issues** 
 * We need to discuss the criteria to reject an AP's request with a lat/long outside desired country
 * Update RAS database daily to include Canada's RAS (OA-534)

## **Version and Date**
|Version|3.7.0.0|
| :- | :- |
|**Date**|**15/01/2023**|

## **Issues Addr§essed**
 * OA-525 rel 3.7.0.0
 * OA-518 Reduce docker image size, fix rat-manage-api
 * OA-494 move all external and DB mouns into one folder
 * OA-522 switch afc engine to strlcpy usage, switching all c/cpp files from strcpy/strncpy and strcat/strncat to strlcpy and strlcat accordingly
 
## **Interface Changes**
 * All static geodata migrated into one folder under /mnt/nfs. See README for details

## **Testing Done**
 * Standard regression testing in CI

## **Open Issues** 
 * 

## **Version and Date**
|Version|3.6.0.0|
| :- | :- |
|**Date**|**15/01/2023**|

## **Issues Addr§essed**
 * OA-524 Fix missing parameters while running stress command (#409)
 * OA-523 adding libbsd and libbsd-dev(el) to all build and install containers used for binary compiling/usage
 * OA-521: fix db-upgrade deadlock, Fix afcconfig migration during upgrade (#406)
 * OA-519 Fixed test_res value still remained issue
 * OA-477 fix ULS_updater container build and move it to Python3
 * OA-512 Parallel execution of multipart requests
 * OA-515 Allow only one config per region (#395)
 * OA-492 Validate NRA value before adding AP
 * OA-510 Fix issue Msghnd afc_config hashing issue (Local FS mode) #388 (#389)
 * OA-508 tools/geo_converters - directory for geospatial data converters
 * OA-506 docker-compose cleanup, fix nginx tag, add table with all supported by afc services env vars
 * OA-6 fix import-export test configuration, freq bands link, run_server script
 * OA-507 Remove msghnd check for unneeded directories (#386)
 * OA-490 Move Import/Export under Afc Config tab
 * OA-484 store AFC config in table in postgresql
 * OA-504 Handle error responses during send receive a request
 * OA-503 Base nginx image on alpine, rework error messages
 * OA-505 Remove redundant check for unused files
 * OA-502 Optimize WFA excel parsing procedure
 * OA-501 Remove exeption when AFC_OBJST_PORT is epmty
 * OA-474 Small fixes for objstorage, remove redundant AFC_OBJST_HOST and AFC_OBJST_HIST_HOST from docker-compose
 * OA-499 Support combined request for testing
 * OA-496 docker-compose.yaml dependencies fix, Add RAS_Database softlink, Create softlink /var/lib/fbrat/ULS_Database to /usr/share/fbrat/rat_transfer/ULS_Database
 * OA-495 Set “MaxLinkDistance” to 130 Km (previously 200Km)
 * OA-489 run nginx test even if apach test failed, run apach tests after that ngnx, Optimize command 'ins_reqs' dropping old table of requests, The test db with only test vectors 18.1, changes to export WFA test vectors 18.1
 * OA-493 fix regr test docker build params, fix "HTTP" -> HTTP (w/o quotes) in FILESTORAGE_SCHEME
 * OA-491 update readme with microservices (containers) build info
 * OA-481 introducing clang format for code formatting, adding examples for clang docker
 * OA-483 restore ratapi build version
 * OA-386 use 3DEP files w/o date in the name, alaska-hawaii nlcd terrain
 * OA-478 Ability to configure Nginx proxy connection timeout and Set default value for it
 * OA-470 Port msghnd to python3 on Alpine
 * OA-475 Bump express from 4.17.1 to 4.18.2 in /src/web, pick up dependabot changes
 * OA-473 Bump qs from 6.5.2 to 6.5.3 in /src/web, pick up dependabot changes
 * OA-472 Bump decode-uri-component from 0.2.0 to 0.2.2 in /src/web
 * OA-463 Worker docker dev env and size optimization alpine
 * OA-455 Remove user id from AP, and replace with the organization field. Fix ap create as part of ConfigAdd.
 * OA-471 Fix preinstall image version, revert version of request library (#345)
 * OA-465 Add target Ulsprocessor and remove subversion dependency
 
## **Interface Changes**
 * Possibly. See changes

## **Testing Done**
 * Standard regression testing in CI

## **Open Issues** 
 * 


## **Version and Date**
|Version|**OA-476**|
| :- | :- |
|**Date**|**12/09/2022**|

## **Issues Addressed**
 * Jira OA-476:Bug in computePRTABLE(), table interpolation is incorrect.
 
## **Interface Changes**
 * None

## **Testing Done**
 * Tested FSP1's channel 13 where the near-field value (alpha_NF) was used to compute passive repeater's path segment gain and it was correct. TestCase is attached to the Jira ticket.

## **Open Issues** 
 * 

## **Version and Date**
|Version|**OA-479**|
| :- | :- |
|**Date**|**12/14/2022**|


## **Issues Addressed**
 * Jira OA-479: Update Passive Repeater I/N calculations
 * There was a typo in the spec (subraction instead of addition for single reflector) that is corrected here
 * Jira OA-482: Clamp negative AP heights to min Height (1.5m)
 
## **Interface Changes**
 * For OA-482, a new flag should be added to afc-config to allow negative heights. This flag is “reportErrorRlanHeightLowFlag” = false. 
 
## **Testing Done**
 * OA-479: Tested FSP1 (and FSP3 and FSP14) that had incorrect values previously. Corrected FSP1 results are attached to this ticket.
 * OA-482: Test FSP7 (with negative height). Results are attached to this ticket. Confirmed all RLAN scan points are evaluated at 1.5m. When the flag "reportErrorRlanHeightLowFlag" = true, we get the proper error message. 
 * Also confirmed that we get same FSP1 results with this flag True or False (and same as before, as expected).

## **Open Issues** 
 * 
## **Version and Date**
|Version|3.5.1.1|
| :- | :- |
|**Date**|**12/06/2022**|
|compiled server's version is 2447dfa | git tag 3.5.1.1|

## **Version and Date**
|Version|3.4.7.1|
| :- | :- |
|**Date**|**12/05/2022**|
|compiled server's version is 0600c74|git tag 3.4.7.1|

## **Version and Date**
|Version|3.4.6.1|
| :- | :- |
|**Date**|**11/22/2022**|
|compiled server's version is 8a990ea |git tag 3.4.6.1|

## **Version and Date**
|Version|**OA-442&398&447**|
| :- | :- |
|**Date**|**11/21/2022**|


## **Issues Addressed**
 * Jira OA-442: Create script to generate antenna_model_list.csv
 * Jira OA-398: Implement correcting FS Receiver Bandwidth (WINNF R2-AIP-19)
 * Jira OA-447: EIRP in response set incorrectly when "printSkippedLinksFlag":false [BUG fix]

## **Interface Changes**
 * OA-442: Added a script (processAntennaCSVs.py) in the ULS parser that loads four files from winnforum's github respository https://github.com/Wireless-Innovation-Forum/6-GHz-AFC/tree/main/data/common_data
 * The four input files are: antenna_model_diameter_gain.csv, billboard_reflector.csv, cateogry_b1_antennas.csv and high_performance_antennas.csv
 * And creates antenna_model_list.csv that contains the required data from the four files
 * antenna_model_list.csv is used by the ULS_parser to set the "Ant Model Name Matched,"Ant Category," "Ant Diameter (m)" and "Ant Midband Gain (dB)" for Tx and Rx, and Passive Repeaters in the ULS.csv as well as "Ant Type" for Passive Repeaters.
 * Note that only some of final calculated parameters are in .sqlite3 file.
 * The ULS parser also corrects FS bandwidth (OA-398) in "Bandwidth (MHz)" column.
 * As such, the latest WFA ULS (WFA_testvector_ULS_2022-11-21T19_57_47.697617) shall be used.
 

## **Testing Done**
 * OA-442: Confirmed that antenna_model_list.csv was created correctly from the 4 input files.
 * Ran FSP48 and confirmed that TS 5008 was implemented correctly in the updated antenna_model_list.csv (this is attached to OA-442).
 * OA-447: Ran FSP48 with "printSkippedLinksFlag":false and the response was correct.
 * OA-398: Confirmed in the WFA ULS (both .sqlite3 and .csv) that the 2 links with missing bandwdith [FS ID: 122730, 122731] were fixed per R2-AIP-19-c, 
 * and 4 links with bandwidth > 60 MHz (one link with 500 MHz [FS ID: 113535] and 3 with 80 MHz [FS IDs: 105213, 96137, 104045]) were fixed per R2-AIP-19-b.

## **Open Issues**


## **Version and Date**
|Version|**OA-421&425&430**|
| :- | :- |
|**Date**|**11/18/2022**|


## **Issues Addressed**
 * Jira OA-421: GUI fixes (fixing channel color plan)
 * Jira OA-425: Correct extensionID in response json
 * Jira OA-430: Finish-up Passive Repeater Implementation


## **Interface Changes**
 * OA-425 was done at API level
 * There was minor change in the ULS-parser (for OA-430)
 * There are two new files to use: 1) ULS database on https://drive.google.com/drive/folders/1ZZXQ1ljjgUwR3u3Bb45g08oN_F2btL1E (...2022-11-16T06_25_10.816819.zip) and 
 * +--2) "WINNF-TS-1014-V1.2.0-App02.csv" in /usr/share/fbrat/rat_transfer/pr
 * Update of default afc-config.json to contain "printSkippedLinksFlag": false. For debugging, this parameter can be set to true so that the skipped links (per Fedor's optimization) are printed in exc_thr file.

## **Testing Done**
 * OA-430: In WFA-testvector-ULS, there were 1370 links with at least one passive repeater.
 * Validated all the precomputed passive repeater parameters in the ULS, exc_thr and fs_analysis files.
 * In addition, validated PR calculations for all the currens cases in the ULS: single back-to-back, single reflector, two back-to-backs, 2 single-reflectors and 3 single reflectors.
 * Example test cases are attached to OA-430 jira ticket.
 * OA-425: Ran FSP1 and confirmed in response.json in the GUI that extensionId is used.
 * OA-421: Ran FSP1 and confirmed that 1) there no RED when minEIRP=-100 dBm, 2) for minEIRP=21 dBm, channels with EIRP < minEIRP are colored RED, 
 * 3) when FSP1 is altered to AP height=40m & height uncertainty=40m, the channels overlapping FS inside AP uncertainty volume are colored BLACK.
 * 4) for AP inside RAS, chanenls overlapping with RAS band are colored BLACK.

## **Open Issues**
 * In R2-AIP-31 for back-to-back, we implemented lambda using actual frequency (rather than UNII-band center freq per the spec) as this is more accurate and it is FSPL equation.
## **Version and Date**
|Version|3.4.5.1|
| :- | :- |
|**Date**|**11/06/2022**|
|compiled server's version is 20551e4 |git tag 3.4.5.1|

## **Version and Date**
|Version|**OA-153&196&300**|
| :- | :- |
|**Date**|**11/05/2022**|


## **Issues Addressed**
 * Jira OA-153: Implement WINNF's inclusion of Diversity antennas
 * Jira OA-196: Implement WINNF near-field adjustments in antenna pattern
 * Jira OA-300: Implement Passive Repeaters per R2-AIP-29 thru 32


## **Interface Changes**
 * There are changes in the ULS parser and .sqlite3 file from OA-153, 196 and 300
 * In afc_config.json, added "passiveRepeaterFlag": false will ignore passive repeaters, true will analyze passive repeaters.
 * There is a new file (nfa_table_data.csv) that is used for near-field adjustment factor that is placed in the appropriate directory in the docker.

## **Testing Done**
 * OA-153: Validated that all the ULS data for diversity receivers are populated correctly per R2-AIP-08 - see the ULS attached to this jira ticket.
 * OA-196: Validated that all the ULS data for near-field adjustment factor are populated correctly per R2-AIP-17 - see the ULS attached to this jira ticket.
 * Ran WFA test vector FSP1 which contains both Diversity Rx and NearField and validated that all the intermediate parameters and the final parameters were correct (see the exc_thr files attached to OA-153/196).

## **Open Issues** 
 * None

## **Version and Date**
|Version|3.4.4.1|
| :- | :- |
|**Date**|**10/27/2022**|
|compiled server's version is 8cac863 | git tag 3.4.4.1|

## **Version and Date**
|Version|**OA-426**|
| :- | :- |
|**Date**|**10/26/2022**|

## **Issues Addressed**
 * Jira OA-426: Change the default afc config
 * Removed "Client" from AP/Client Propagation Environment in AFC Config GUI
 
## **Interface Changes**
 * Ensure that the parameters for which we don't have in the GUI appear in the afc-config.json

## **Testing Done**
 * Confirmed that the parameters are configured properly.

## **Open Issues**
## **Version and Date**
|Version|**OA-422**|
| :- | :- |
|**Date**|**10/20/2022**|


## **Issues Addressed**
 * Jira OA-422: Add antenna-look-up function for external files in the ULS parser & Add ability to include UNII-8 FS in the ULS
 * Note that addition of UNII-8 is a feature and will be turned off for OpenAFC

## **Interface Changes**
 * Changes are in the ULS parser only 
 

## **Testing Done**
 * Confirmed that the ULS file generated with UNII-8 has the UNII-8 links (4,444 on 10/20/222).
 * Generated WFA ULS using UNII-5/7 only and validated that it was the same as the last generated WFA ULS from 09/28/2022. 
 * There is no need to update the ULS file.


## **Open Issues**

## **Version and Date**

|Version|**OA-383**|
| :- | :- |
|**Date**|**10/18/2022**|


## **Issues Addressed**
 * Jira OA-383: Remove from response when EIRP is below minDesiredPower or min EIRP in AFC-Config and when PSD is below minPSD
 
## **Interface Changes**
 * None

## **Testing Done**
 * Test1: Ran FSP1, first with minEIRP and minPSD in afc-config set to -100 and no minDesiredPower in AP request message. Confirmed that all EIRP and PSD levels are present.
 * Test2: set minEIRP to 21 dBm (no minDesiredPower) and minPSD to 8 dBm/MHz and confirmed that the EIRP and PSDs below these levels were removed from the response. The remaining results were identical to the baseline test above.
 * Test3: set minEIRP to 20 dBm, minDesiredPower to 30 dBm and minPSD to 20 dBm/MHz and confirmed that the EIRP and PSDs below these levels were removed from the response.
 * Test4: set minEIRP to 21 dBm, minDesiredPower to 0 dBm and minPSD to -100 dBm/MHz and confirmed that EIRPs below 21 dBm were removed and no PSDs were removed.
 

|Version|**OA-413**|
| :- | :- |
|**Date**|**10/14/2022**|

## **Issues Addressed**
 * Jira OA-413: maxPsd plot in Virtual AP is broken
 * The PSD plot became blank when we changed maxPSD to maxPsd in the response message for compliance with WFA SDI (System-Device-Interface).
 
 * Jira OA-414: Add "AP Min. PSD" in AFC Config GUI

## **Interface Changes**
 * A new parameter "AP Min. PSD" is added to AFC Config (GUI and .json).  In the .json file the parameter is "minPSD".

## **Testing Done**
 * OA-413: Ran FSP1 and confirmed that the PSD plot matches the maxPsd levels in the response message.
 * OA-414: Confirmed presence of "AP Min. PSD" in the GUI and after doing an FSP1 run, the parameter "minPSD" is in the afc-config.json and set to the value in the GUI.
 * Note that this parameter is not yet being used by the AFC-Engine. This will be done in a separate jira ticket/branch.

## **Open Issues**

## **Version and Date**
|Version|3.4.3.1|
| :- | :- |
|**Date**|**10/18/2022**|
|compiled server's version is 45668a0 |git tag 3.4.3.1|


## **Issues Addressed**
 * Jira OA-395: Analysis of FS inside RLAN uncertainty region
 
## **Interface Changes**
 * There's a new boolean flag to enable analysis of FS insidde RLAN uncertainty region in afc-config.json which is: "allowScanPtsInUncReg" = true. By default (e.g. when absent), this is false.

## **Testing Done**
 * Ran FSP1 with the "allowScanPtsInUncReg" flag set to true and another test with this flag set to false. 
 * From the google-map confirmed the FS ID of the 3 FS that are inside RLAN uncertainty region. 
 * In exc_thr file, confirmed that all these FS were above RLAN uncertainty volume and as such they were all analyzed (using FSPL path loss) as expected in both tests.
 * In addition, confirmed that all the RLAN channels have max EIRP levels.

 * Next, adjusted FSP1's RLAN height so that the 3 FS inside RLAN uncertainty region would be inside the uncertainty volume.
 * Confirmed that when "allowScanPtsInUncReg" flag is set to false, none of those FS are analyzed and the affected RLAN channels are grayed out.
 * Confirmed that when "allowScanPtsInUncReg" flag is set to true, all of those FS are analyzed and all channels have max EIRP levels.
 
## **Open Issues**

## **Version and Date**
|Version|3.4.2.2|
| :- | :- |
|**Date**|**10/06/2022**|
|compiled server's version is cd7836e |git tag 3.4.2.2|

## **Issues Addressed**
 * Jira OA-415 Disable cache on test response reacquisition
 * Jira OA-412 Simplify build and install at docker container
 * Jira OA-389 Speeding up AFC Engine. AFC Engine performance optimizations. Modifying test DB to cover removal of below-21dBm responses
 * Jira OA-411 Adding support for maxPsd to afc_tests.py
 * Jira OA-409 Provide support for new toolchain in standalone development environment. Minor improve to generalize path to toolchain.
 * Jira OA-400. AFC Engine performance optimizations by direct GDAL access mode and ULS-data resorting
=======
|Version|**OA-395**|
| :- | :- |
|**Date**|**10/06/2022**|

## **Version and Date**
|Version|3.4.2.1|
| :- | :- |
|**Date**|**09/30/2022**|
|compiled server's version is 6e7335b |git tag 3.4.2.1|

## **Version and Date**
|Version|**OA-300**|
| :- | :- |
|**Date**|**09/29/2022**|


## **Issues Addressed**
 * Jira OA-300 partially: Implement Passive Repeaters per R2-AIP-29 thru 32
 * This includes changes to both ULS parser, the Python script and the AFC Engine.
 * Although structures have been added to analyze passive repeaters, this commit bypasses the structures added until this is fully implemented.
 * As such, all the regression tests should pass WITH THE NEW ULS DATABASE.

## **Interface Changes**
 * There are significant changes in the ULS parser to add and determine the values for the passive repeaters' parameters
 * Some of the ULS parser changes need to be updated per the latest WINNF R2-AIP-29/30 specification

## **Testing Done**
 * Ran FSP1 thru FSP5 using the latest ULS and confirmed getting same results as before.

## **Open Issues**
 * Completion of OA-300
## **Version and Date**
|Version|3.4.1.1|
| :- | :- |
|**Date**|**09/10/2022**|
|compiled server's version is TBD |git tag 3.4.1.2|

## **Version and Date**
|Version|**OA-381**|
| :- | :- |
|**Date**|**09/10/2022**|
## **Issues Addressed**
 * Jira OA-381: Disable non OIDC email confirmation for new users

## **Open Issues**


## **Version and Date**
|Version|**OA-358**|
| :- | :- |
|**Date**|**08/30/2022**|


## **Issues Addressed**
 * Jira OA-358: Update supplemental info in AFC-Config GUI

## **Interface Changes**
 * None

## **Testing Done**
 * None since only the text in the GUI has changed. All regression tests should pass.

## **Open Issues**

|Version|3.4.1.1|
| :- | :- |
|**Date**|**08/30/2022**|
|compiled server's version is 2d5319a |git tag 3.4.1.1|

|Version|**OA-314**|
| :- | :- |
|**Date**|**08/30/2022**|

## **Interface Changes**
As part of the login cleanup and preparation for OIDC implementation, implemented in https://github.com/Telecominfraproject/open-afc/pull/212,
user database schema is changed and it is not compatible with the existing user database.
If you already have an AFC service deployed and your deployment is using a user database with old schema,
once the above mentioned pull request is merged, error messages will be displayed during the docker startup, showing the migration steps.
You have two options: to reinitialize your database, or migrate it.
To preserve the user database, please follow the instructions to migrate the existing user database to the new schema.
(These instructions are also included in the README.md file)
**Option 1: Reinitialize the database without users:**
```
rat-manage-api db-drop
rat-manage-api db-create
```
This will wipe out existing users, e.g. users need to register, or be manually recreated again.
**Option 2: Migrate the database with users:**
```
RAT_DBVER=0 rat-manage-api db-export --dst data.json
RAT_DBVER=0 rat-manage-api db-drop
rat-manage-api db-create
rat-manage-api db-import --src data.json
```
This migration will maintain all existing user data, including roles. Steps to migrate:
1. Export the user database to .json file.  Since the database is an older version, use env variable to tell the command the the right schema to use to intepret the database.
2. Delete the old version database.
3. Recreate the database.
4. Import the json file into the new database.


|Version|3.3.23.1|
| :- | :- |
|**Date**|**08/29/2022**|
|compiled server's version is da048f6 |git tag 3.3.23.1|


# **Version and Date**
|Version|**OA-235 & OA-320**|
| :- | :- |
|**Date**|**08/24/2022**|


## **Issues Addressed**
 * Jira OA-235: Implement WFA requirement on setting FS Rx Feederloss
 * Jira OA-320: Remove FS links with the same Tx and Rx Coordinates

## **Interface Changes**
 * OA-235 includes changes to both ULS parser and engine. The ULS parser now has a new column "Rx Line Loss (dB)" that is read by the Engine.
 * OA-320 involves changes to the ULS parser only.
 * Need to use a new WFA ULS for these tests. This is CONUS_ULS_2022-08-24T14_27_06.213483_fixedBPS_sorted_param.sqlite3 that can be downloaded from OpenAFC TIP google drive.

## **Testing Done**
 * OA-235: confirmed that the new column is there. Only 0.3% of the WFA ULS links had a non-blank Rx feederloss as expected.
 * Ran a test and confirmed in the exc_thr file, there were 6 FS with a feederloss other than 3 dB and confirmed these were correctly set per ULS database. (test is attached to the jira ticket)
 * Also confirmed in the same test that all the links with 3 dB feederloss had blank feederloss in the ULS.
 * Did another test to ensure 0 dB feederloss in the ULS is used as 0 dB (this is not attached to the ticket).
 * OA-320: Confirmed that same Tx/Rx coordinates were removed and moved to the anomalous file. There were 51 such links in the WFA ULS (from 24Oct2021 weekly).

## **Open Issues**
 * 
## **Version and Date**
|Version|**OA-354**|
| :- | :- |
|**Date**|**08/24/2022**|


## **Issues Addressed**
 * Jira OA-354: Move RLAN height restriction configurable-parameters to afc-config.json

## **Interface Changes**
 * There are two new parameters in afc-config.json
 * "winner2HgtFlag" to enable/disable use of the height restriciton and 
 * "winner2HgtLOS" to set the height above which Winner2 LOS will be used (when the flag is set to true)

## **Testing Done**
 * TurnOffRLANHgtRestriciton test: Ran FSP10 (where 14 dB of difference was seen with QCOM due to the height restriction) with "winner2HgtFlag" = false and confirmed that the large EIRP differences were resolved.
 * Confirmed in the exc_thr file that for high RLAN heights, WinnerII Combined is used as expected.
 * TurnOnRLANHgtRestriction: created a quick test (to be added to the CICD regression test) where the height restriction is turned on. 
 * In addition, use of ground distance is set to TRUE for both FSPL and WII (OA-348 and OA-337 respectively) and use "power-based" channel response (OA-265) to get these added to the regression test.
 * Confirmed in the exc_thr file that WII-LOS is always used due to the RLAN height restriction setting as expected.

## **Open Issues**

## **Version and Date**
|Version|**OA-355**|
| :- | :- |
|**Date**|**08/24/2022**|


## **Issues Addressed**
 * Jira OA-355: Allow WFA SDI Spec version of 1.3

## **Interface Changes**
 * Engine now allows version 1.3 (to be treated same as 1.1)
 * AP inserts version 1.3 in the AP request message.
 * Engine would allow both 1.1 and 1.3 and treat them equally.

## **Testing Done**
 * Confirmed that request and response messages have version 1.3.
 * Confirmed that version 1.1 works as before.

## **Open Issues**

## **Version and Date**
|Version|**OA-341**|
| :- | :- |
|**Date**|**08/15/2022**|


## **Issues Addressed**
 * Jira OA-341: Debug why SIP9 regression tests fails when OA-337 is used (this is addressed on OA-346)
 * Jira OA-346: Bug in implementation of R2-AIP-05 calculated antenna diameter not properly inserted into sqlite ULS file.
 * In OA-232, which implemented R2-AIP-05, there was a bug in that RX Antenna diameter was not properly included in the sqlite database. OA-346 has the fix.
 * There is a new WFA uls database on google-drive (WFA_testvector_ULS_220811.zip). This ULS needs to be used.
 * The nature of this bug is that the antenna diameter had a value of -1 in the sqlite.  When computing the Rx Antenna gain there was a log of a negative number, resulting in an arithmetic exception, and the gain was never set.  The gain used in subsequent I/N calculations was uninitialized.
 * Note that -1 for Tx Ant Diameter in the ULS is OK as it's not being used in R2-AIP-05 and it just means that that the Tx antenna model in ULS was not matched with any model in our database that has a diameter.
 * The fix is in the ULS parser only.

## **Interface Changes**
 * ULS parser has been updated.


## **Testing Done**
 * Confirmed that in the updated ULS (per above), Rx diameter is never -1.
 * Ran SIP9 test case and found results very different from last time.
 * For Channel 41, confirmed that the Rx diameter was computed correctly (using R2-AIP-07 F.699 and R2-AIP-05(d)) and that the gain was computed correctly.

## **Open Issues**


## **Version and Date**
|Version|**OA-348**|
| :- | :- |
|**Date**|**08/16/2022**|


## **Issues Addressed**
 * Jira OA-348:Note that this is incorrect use of distance in FSPL. This is done only for calibration purposes temporarily. QCOM will be reverting to using LOS distance up to 1km.

## **Interface Changes**
 * A new configurable parameter can be used inside the propagation model in afc-fonig.
 * The new parameter is "fsplUseGroundDistance":true" (it is set true here to allow use of ground distance for FSPL calculation.

## **Testing Done**
 * Tested FSP2 that previously had more than 5 dB difference in max allowed EIRP with QCOM and now they're matching within 0.2 dB and confirmed that we have same FSPL as QCOM.
 * Confirmed in exc_thr file that when GroundDistance is set to true for FSPL, ground distance is used in the FSPL calculation.
 * Confirmed in exc_thr file that when GroundDistance is set to false for FSPL, LOS/path distance is used in the FSPL calculation.

## **Open Issues**

## **Version and Date**
|Version|**OA-350**|
| :- | :- |
|**Date**|**08/22/2022**|

## **Issues Addressed**
 * Jira OA-350: anomalous behavior in uls-script and daily_uls_parse.py

## **Interface Changes**
 * The fix for a bug in the ULS parser that was fixed in a localy commit but didn't go through during Pull Request was put back in.
 * With FCC chnage of "PA" table, the ULS parser uses the old table pre-18Aug and uses the new table post-18Aug. See the description in Jira ticket for more details.
 * Please use latest WFA ULS Database (08/19/22) from OpenAFC TIP's Google Drive.

## **Testing Done**
 * WFA test vector ULS generated (and validated) previously without this bug and another one generated with the latest ULS parser were identical.
 * Ran FSP1-10 and compared results against our previous ULS and QCOM and didn't see any issues.
 * Generated a new ULS from start (download from FCC webiste) to finish (generate .sqlite file) and examined the content. The parser handled the PA tables from pre and post 18-Aug correctly. 

## **Open Issues**


## **Version and Date**
|Version|**OA-90**|
| :- | :- |
|**Date**|**08/08/2022**|


## **Issues Addressed**
 * Jira OA-90:Add exc_thr visibility threshold to AFC Confg

## **Interface Changes**
 * In AFC Config, added visibilityThreshold to be configurable through the GUI. 
 * Note that at the moment, this change applies only to links with distance > 1km. 
 * For distances < 1km, all I/Ns are still recorded.

## **Testing Done**
 * In AFC Config GUI, set the VisibilityThreshold to -6 dB and confirmed that exc_thr only shows I/Ns > -6 dB for distances > 1km.
 * Test is attached to the Jira ticket.

## **Open Issues**

## **Version and Date**
|Version|**OA-337**|
| :- | :- |
|**Date**|**08/08/2022**|


## **Issues Addressed**
 * Jira OA-337: Add a configurable parameter for WINNER-II to use ground distance vs. path distance

## **Interface Changes**
 * OA-323: UI/API changes to allow configuring AFC Config (via json file only) to use ground distance calculation for WinnerII path loss. This is done through setting "win2UseGroundDistance": true in the propagation model.
 

## **Testing Done**
 * All the tests are attached to the Jira ticket.
 * Ran a test (FSP2) and the ground distance in exc_thr file (newly added column: RLAN_FS_RX_GROUND_DIST (km)) matched online ground distance calculator for 2 points (https://keisan.casio.com/exec/system/1224587128) where 6371 km is used for earth radius.
 * Compared the 2 points in Qualcom's FSP2 data (that used ground distance) and confirmed we got same ground distance as Qualcom.  
 * Confirmed that path distance is used for distnace < 30m in LOS computation
 * Confirmed that for ground distance between 30m and 1km, WinnerII is used.
 * Confirmed that for ground distance > 1km, ITM is used. 


## **Open Issues**


|Version|3.3.22.2|
| :- | :- |
|**Date**|**08/04/2022**|
|compiled server's version is f6b36be |git tag 3.3.22.2|

# **Version and Date**
|Version|**OA-323**|
| :- | :- |
|**Date**|**08/02/2022**|


## **Issues Addressed**
 * Jira OA-323: In FCC R&O, WII LOS confidence needs to be set to WII Combined Confidence

## **Interface Changes**
 * OA-323: UI/API changes to allow entering WinnerII LOS Confidence in the absence of building database (since LOS is used for distance of 30 to 50m). The changes are made to FCC 6GHz Report&Order and Custom. 
 

## **Testing Done**
 * All the tests are attached to the Jira ticket.
 * Ran a test without LiDAR with WinnerII Combined and LOS confidence being different.
 * Confirmed in afc_config.json that "win2ConfidenceLOS" is present and set to the value entered by the user.
 * Confirmed in exc_thr file that when PATH_LOSS_MODEL is one of the W2 models, in PATH_LOSS_CDF column, the LOS confidence is used for distances between 30 to 50m and the Combined Confidence is used for distances > 50m.
 * Ran another test with LiDAR and confirmed that the WinnerII LOS and NLOS confidences were set properly and used correctly in exc_thr file.
 * Ran a third test with Custom with WinnerII LoS Option set to Combined LoS/NLoS and confirmed that the WinnerII Combined and LOS confidences were set properly and used correctly in exc_thr file.
 * Ran a fourth test with Custom with WinnerII LoS Option set to LoS/NLoS per building data and confirmed that the WinnerII LOS and NLOS confidences were set properly and used correctly in exc_thr file.

## **Open Issues**


|Version|3.3.22.1|
| :- | :- |
|**Date**|**08/03/2022**|
|compiled server's version is a592094 |git tag 3.3.22.1|

## **Version and Date**

|Version|**OA-321**|
| :- | :- |
|**Date**|**07/29/2022**|


## **Issues Addressed**
 * Jira OA-321: Add "Max Allowed EIRP" to exc_thr file and visibilityThreshold to afc_config.json

## **Interface Changes**
 * The visibilityThreshold is in the AFC_config.json file (see an example attached to the jira ticket) 

## **Testing Done**
 * Confirmed that exc_thr has a new column "EIRP_LIMIT (dBm)" after "FS I/N (dB)" column.
 * Confirmed that the exc_thr file has all links with distance < 1km AND for distances > 1km, only links with I/N > -6 dB are included.
 * See test attached to this jira ticket.

## **Open Issues**


|Version|**OA-317 & OA-232**|
| :- | :- |
|**Date**|**07/25/2022**|

## **Issues Addressed**
 * Jira OA-317: Implement setting FS Rx height per WinnForum R2-AIP-14 (also implemented this for FS Tx height)
 * Jira OA-232: Implement WINNF R2-AIP-05 remaining rules on Antenna Gain and Diameter 

## **Interface Changes**
 * For both tickets, there were changes in the ULS script, as such there's new ULS file that needs to be used.
 * For OA-317, in the "_param.csv" ULS file there's a "return_FSID" column which has a valid value for forward paths with missing Rx height and their return paths. When paths A and B are matched, the return_FSID for A is set to B, and return_FSID for B is set to A, always.
 * There's also "FRN" (FCC Registration Number) that is used for return path matching in "_param.csv" file.
 * For OA-317, there's no new column in the .sqlite. The Rx height to center RAAT column shows the corrected value per R2-AIP-14.
 * For OA-233, there's no new column in the .sqlite. However, there are new columns in the "_param.csv" file: "Tx Ant Model Name Matched", "Tx Ant Category", "Tx Ant Diameter (m)", "Tx Ant Midband Gain", "Rx Ant Midband Gain", "Tx Gain ULS" and "Rx Gain ULS."
 * The "Rx Gain" and "Tx Gain" are the gains calculated per R2-AIP-05/06.
 * Note: If FSID A has a return_FSID of B, then B has a return_FSID = A. In this case A and B are considered to be paired links. If A has return_FSID = -B, then then B has a return_FSID not equal to A, B has already been paired with a different link. I used the '-' to distinguish between a "pair" and using a "candidate" link.

## **Testing Done**
 * OA-317: ULS checks (on WFA Test Vector ULS): in the .csv ULS file with filename appended with "param," the last column "return_FSID" is used for validation.
 * Confirmed in the anomalous file that there's no FS link with invalid/blank Tx/Rx height that was thrown out
 * Confirmed that when "return_FSID" is blank, there was a valid "Rx Height to Center RAAT (m)" (99.8% of the links) by using Rx Height != -1 in _sorted.csv file.
 * Height of 42.5m was set for only 25 links (return_FSID is blank as expected as in this case return path didn't have height either). Validated a few of the links via online ULS that they didn't have a height and nor did its return link (for example WAY91/WAY90 didn't have a return path). Confirmed that none of the 25 links had a return link.
 * Confirmed that all 134 links were matched with return correctly. Only 14 of them didn't match on lat/long (Tx or Rx) perfectly but they were at max 0.000138889 different which is within 1 arcsec accuracy that's checked (2.78e-4 deg).
 * For Tx heights, only 0.01% of the links (=9 links) had invalid Tx height for which R2-AIP-14 was applied. 6 (out of 9) got R2-AIP-14(a-iii)= 42.5m height and the remaining (3 out of 9) got the return path's Rx height.

 * OA-232: ULS checks (on WFA Test Vector ULS):
 * About 88.7% of links get R2-AIP-05(a) applied. Out of these cases, 88.1% got ULS-Gain, 11.8% got Midband Gain and 0.1% got Typical Gain.
 * Next, out of 6107 blank Rx Ant Model, only two had a valid Tx Ant Model and same Gain where R2-AIP-05(b) is applied.
 * Validated majority of R2-AIP-05(c) and (d) cases.
 * There were 0.2% with blank Rx Gain where R2-AIP-05(e) is applied.
 * Validated matching of the Rx Antenna models per the mapping created using the database on WinnForum antenna respository.
 
## **Open Issues**
 * There are 5 FS links with Rx height > 600m that are erroneous. These haven't been fixed yet. R2-AIP-14 most likely will be amended with a recommendation on how to fix these.

## **Version and Date**
|Version|**OA-309**|
| :- | :- |
|**Date**|**08/03/2022**|


## **Issues Addressed**
 * Jira OA-309: Correction of Error Responses for conformation with WFA 1.1 Interface Specification
 
## **Interface Changes**
 * No

## **Testing Done**
 * Tested each of the cases below in Virtual AP and confirmed correct response (as noted in Jira)
 * Note that most of these tests cannot be done throught the GUI and instead can be done by sending the desired AP request json file directly to the engine.
 * When version number is incorrect in the AP request, we get Error Code of 100.
 * When "rulesetId" is missing from AP request message, we get Error Code of 102 (missing parameter).
 * requestId="null" results in Error Code of 103
 * Missing "device descriptor" in AP request message results in Error Code 102 (missing parameter).
 * Missing NRA results in Error Code 102 (missing parameter) with description of missing NRA.

## **Open Issues**

## **Version and Date**
|Version|**OA-317 & OA-232**|
| :- | :- |
|**Date**|**07/25/2022**|

## **Issues Addressed**
 * Jira OA-317: Implement setting FS Rx height per WinnForum R2-AIP-14 (also implemented this for FS Tx height)
 * Jira OA-232: Implement WINNF R2-AIP-05 remaining rules on Antenna Gain and Diameter 

## **Interface Changes**
 * For both tickets, there were changes in the ULS script, as such there's new ULS file that needs to be used.
 * For OA-317, in the "_param.csv" ULS file there's a "return_FSID" column which has a valid value for forward paths with missing Rx height and their return paths. When paths A and B are matched, the return_FSID for A is set to B, and return_FSID for B is set to A, always.
 * There's also "FRN" (FCC Registration Number) that is used for return path matching in "_param.csv" file.
 * For OA-317, there's no new column in the .sqlite. The Rx height to center RAAT column shows the corrected value per R2-AIP-14.
 * For OA-233, there's no new column in the .sqlite. However, there are new columns in the "_param.csv" file: "Tx Ant Model Name Matched", "Tx Ant Category", "Tx Ant Diameter (m)", "Tx Ant Midband Gain", "Rx Ant Midband Gain", "Tx Gain ULS" and "Rx Gain ULS."
 * The "Rx Gain" and "Tx Gain" are the gains calculated per R2-AIP-05/06.
 * Note: If FSID A has a return_FSID of B, then B has a return_FSID = A. In this case A and B are considered to be paired links. If A has return_FSID = -B, then then B has a return_FSID not equal to A, B has already been paired with a different link. I used the '-' to distinguish between a "pair" and using a "candidate" link.

## **Testing Done**
 * OA-317: ULS checks (on WFA Test Vector ULS): in the .csv ULS file with filename appended with "param," the last column "return_FSID" is used for validation.
 * Confirmed in the anomalous file that there's no FS link with invalid/blank Tx/Rx height that was thrown out
 * Confirmed that when "return_FSID" is blank, there was a valid "Rx Height to Center RAAT (m)" (99.8% of the links) by using Rx Height != -1 in _sorted.csv file.
 * Height of 42.5m was set for only 25 links (return_FSID is blank as expected as in this case return path didn't have height either). Validated a few of the links via online ULS that they didn't have a height and nor did its return link (for example WAY91/WAY90 didn't have a return path). Confirmed that none of the 25 links had a return link.
 * Confirmed that all 134 links were matched with return correctly. Only 14 of them didn't match on lat/long (Tx or Rx) perfectly but they were at max 0.000138889 different which is within 1 arcsec accuracy that's checked (2.78e-4 deg).
 * For Tx heights, only 0.01% of the links (=9 links) had invalid Tx height for which R2-AIP-14 was applied. 6 (out of 9) got R2-AIP-14(a-iii)= 42.5m height and the remaining (3 out of 9) got the return path's Rx height.

 * OA-232: ULS checks (on WFA Test Vector ULS):
 * About 88.7% of links get R2-AIP-05(a) applied. Out of these cases, 88.1% got ULS-Gain, 11.8% got Midband Gain and 0.1% got Typical Gain.
 * Next, out of 6107 blank Rx Ant Model, only two had a valid Tx Ant Model and same Gain where R2-AIP-05(b) is applied.
 * Validated majority of R2-AIP-05(c) and (d) cases.
 * There were 0.2% with blank Rx Gain where R2-AIP-05(e) is applied.
 * Validated matching of the Rx Antenna models per the mapping created using the database on WinnForum antenna respository.
 
## **Open Issues**
 * There are 5 FS links with Rx height > 600m that are erroneous. These haven't been fixed yet. R2-AIP-14 most likely will be amended with a recommendation on how to fix these.
## **Version and Date**
|Version|**OA-302**|
| :- | :- |
|**Date**|**07/19/2022**|


## **Issues Addressed**
 * Jira OA-302: Include additional heights for RLANs

## **Interface Changes**
 * None

## **Testing Done**
 * Validated four examples of different height and height uncertainty (per jira ticket). Test configuration and results are attached to the jira ticket.
 * Valdiation is done by confirming expected heights (per Jira ticket) under "RLAN_HEIGHT_ABOVE_TERRAIN (m)" in exc_thr file.

## **Open Issues**

|Version|3.3.21.1|
| :- | :- |
|**Date**|**07/20/2022**|
|compiled server's version is f5ad056 |git tag 3.3.21.1|

## **Version and Date**
|Version|**OA-297**|
| :- | :- |
|**Date**|**07/14/2022**|

## **Issues Addressed**
 * Jira OA-297: Support multiple AP requests

## **Interface Changes**
 * Mapping data was previously passed via a vendorExtension on the entire set of responses, data is now attached as a vendorExtension to the particular availableSpectrumInquiryResponse that it is generated for.

## **Testing Done**
 * Tested against API using SOAPUI tool to generate and review responses for a single request and multiple requests with and without attached mapping data. Project with included inputs is attached to the JIRA ticket.
 * Reviewed UI to verify mapping functions continue to work 

## **Open Issues**

## **Version and Date**
|Version|**OA-296**|
| :- | :- |
|**Date**|**06/30/2022**|

## **Issues Addressed**
 * Jira OA-296: RLAN uncertainty region scan point Bug

## **Interface Changes**
 * None

## **Testing Done**
 * Retested FSP1 that had the extra scan point and the scan point is now gone as expected with all the other scan points same as before. See attached testcase and resulting scan points to the ticket.

## **Open Issues**


## **Version and Date**
|Version|3.3.20.3|
| :- | :- |
|**Date**|**06/16/2022**|
|compiled server's version is 490fe87 |git tag 3.3.20.3|

## **Issues Addressed**
 * OA-159 Eliminate HISTORY_DIR flask setting (#187)
## **Version and Date**
|Version|3.3.20.2|
| :- | :- |
|**Date**|**06/13/2022**|
|compiled server's version is 61bb85b |git tag 3.3.20.2|

## **Issues Addressed**
 * OA-284 rework async connectivity with test utility (#186)
 * OA-285 update docker repo name to the TIPs ECR one (#184)
 * OA-243 New local fs layout; history server (#180)
 * Python IO wrapper supports local fs and http read/write/delete (#183)
 * OA-283 update AFC config records in the test DB (#182)

## **Version and Date**
|Version|3.3.20.1|
| :- | :- |
|**Date**|**06/07/2022**|
|compiled server's version is 1a2a54f |git tag 3.3.20.1|

## **Version and Date**

|Version|**OA-265**|
| :- | :- |
|**Date**|**06/03/2022**|


## **Issues Addressed**
 * Jira OA-265: Enable/Disable consideration of FS channel overlap with RLAN

## **Interface Changes**
 * There's a new configurable parameter in afc-config (both the GUI and hence the .json file). In the .json file, this is called "channelResponseAlgorithm" which is set to either "pwr" or "psd"

## **Testing Done**
 * Ran the test for OA-263 and confirmed getting same results for inquired channel and inquired frequency when "channelResponseAlgorithm" is set to "pwr" as expected. This test is called "pwr" in the test folder attached.
 * Note that this test case covers all cases: 1) RLAN full channel inside FS channel, 2) RLAN channel partially overlapping FS, 3) RLAN channel outside FS but its adjacent band inside FS band, 4) FS channel fully inside RLAN channel
 * Next, ran the same test but with "channelResponseAlgorithm" set to "psd." Confirmed getting same results for inquired frequency as "pwr" test as expected since this only changes the channel response.
 * However, the max allowable EIRP levels are different b/w "pwr" and "psd" with the "psd" always yielding lower EIRP levels (as expected).
 * Using exc_thr file (for psd case using inquired channel only) validated calculation of max allowed EIRP for the psd-based approach using the different scenarios mentioned above and for the 4 bandwidths and the 5 operating classes.
 * Note that the channel responses are the same when the FS channel is fully inside RLAN channel.

## **Open Issues**
 * None

|Version|**OA-278**|
| :- | :- |
|**Date**|**06/03/2022**|

## **Issues Addressed**
 * Jira OA-278: Add the databases' attributions in database_readme file and the GUI's "i" 
 
## **Interface Changes**
 * No Change

## **Testing Done**
 * Reviewed updated content
 
## **Open Issues**
 * Date that the Globe data was retrieved is unknown.  Opened Jira ticket OA-279 to update and review data to generate a new date.

## **Version and Date**
|Version|**OA-263**|
| :- | :- |
|**Date**|**05/27/2022**|


## **Issues Addressed**
 * Jira OA-263: Change PSD calculation to be based on 20-MHz EIRP and without Spectral Overlap Loss
 
## **Interface Changes**
 * No Change

## **Testing Done**
 * Ran a test with both frequency and channel inquiry. Confirmed that the PSD levels made sense - they were following the 20-MHz EIRP levels and they were always as good or worse than max allowable EIRPs - 10*log10(20).
 * Next ran the same test but without channel inquiry and using the exc_thr file (which contains the link budgets for PSD calculations only) derived the PSD levels (see columns AX to BD) and confirmed they matched.
 

## **Open Issues**
 * None

## **Version and Date**
|Version|3.3.19.1|
| :- | :- |
|**Date**|**05/25/2022**|
|compiled server's version is 3.3.19-0563461 |git tag 3.3.19.1|

## **Version and Date**
|Version|**OA-259**|
| :- | :- |
|**Date**|**05/23/2022**|


## **Issues Addressed**
 * Jira OA-259: Custom not setting ITM using Tx Clutter per building data correctly
 
|Version|**OA-257**|
| :- | :- |
|**Date**|**05/19/2022**|

## **Issues Addressed**
 * Jira OA-257: Add configurability of WinnerII confidence (for Combined vs. LoS/NLoS) in AFC Config's FCC R&O
 * When "FCC 6GHz Report & Order" or "Custom" propagation models are selected with building database (e.g. LiDAR), both Winner Combined and LoS/NLoS confidences need to be specified. In this case, if RLAN is in LiDAR region, WinnerII LoS or NLOS path loss is used; else, WinnerII Combined path loss is used.

## **Interface Changes**
 * In AFC-Config UI, added "WinnerII LOS/NLOS Confidence" and "WinnerII Combined Confidence" (in place of "WinnerII Confidence") for "FCC 6GHz Report & Order" and "Custom" Propagation models.
 * Note that only the parameter(s) needed is shown based on the propagation model (e.g. use of LIDAR or not) selected.
 * In addition, these parameters also appear in the updated afc-config.json file.
 <pre>
      propagationModel": {
        "kind": "FCC 6GHz Report & Order",
        <strong>"win2ConfidenceLOS": 30,</strong>
        <strong>"win2ConfidenceNLOS": 40,</strong>
        <strong>"win2ConfidenceCombined": 50,</strong>
        [...]
</pre>
## **Testing Done**
 * Did the following tests (these are attached to the Jira ticket):
 * In "FCC 6GHz R&O with LiDAR", set WII LOS/NLOS Confidence to 5% and confirmed in exc_thr (PATH_LOSS_MODEL and PATH_LOSS_CDF) this was set correctly.
 * In "FCC 6GHz R&O with WII-Combined", set WII Combined Confidence to 95% and confirmed in exc_thr (PATH_LOSS_MODEL and PATH_LOSS) this was used correctly. Also, spectrum availability was alot more than the LiDAR test above as expected.
 * In "Custom WII Combined", set WII Combined Confidence to 95% and confirmed in exc_thr (PATH_LOSS_MODEL and PATH_LOSS) this was set correctly. Also, spectrum availability was similar to above test case (FCC R&O with WII Combined) as expected.
 * In "Custom WII LoS/NLoS based on LiDAR", set WII Combined Confidence to 95% and WII LOS/NLOS confidence to 5% and confirmed in exc_thr (PATH_LOSS_MODEL and PATH_LOSS) this was set correctly. Also, spectrum availability was alot worse than above test case (Custom with WII Combined) as expected.
 * In all cases, confirmed parameters were set correctly in afc-config.json files.

## **Open Issues**

## **Version and Date**


## **Version and Date**
|Version|**OA-254**|
| :- | :- |
|**Date**|**05/23/2022**|

## **Issues Addressed**
 * Jira OA-254: Raw JSON to add back the missing part of the response message

## **Interface Changes**
 * No Change

## **Testing Done**
 * Confirmed in Virtual AP that the missing part of the response message is present in RAW JSON response (at the bottom of the page)

## **Open Issues**

|Version|**OA-247**|
| :- | :- |
|**Date**|**05/18/2022**|

## **Issues Addressed**
 * Jira OA-247: Remove Point Analysis from Dashboard
   * Removed navigation and pages for Point Analysis
   * Removed Point Analysis specific code in engine

## **Interface Changes**
None

## **Testing Done**
 * Exercised remaining functions in UI and end to end tests. 
 
|Version|**OA-246**|
| :- | :- |
|**Date**|**05/17/2022**|

## **Issues Addressed**
 * Jira OA-246: When Virtual AP location type is set to one of the Polygons, can't select Ellipse

## **Interface Changes**
 * None

## **Testing Done**
 * In Virtual AP, was able to go from Ellipse to Radial Polgon to Linear Polygon and back to Ellipse successfully. Also ran some of the end2end tests (1, 3 and 4) with Ellipse, Radial Polygon and Linear Polygon. 

## **Open Issues**
|Version|**OA-204**|
| :- | :- |
|**Date**|**05/16/2022**|

## **Issues Addressed**
 * Jira OA-204: New "Trial User" support - to be used for FCC (and other countries equiv.), AT&T and other demos/testing
   - Added Trial role
   - Added functionality to Virtual AP page based on membership in Trial role to load the trial configuration and fill in Serial and Certification Id 
   - Added server method to load the trial user configuration if Trial use and no configuration is found
   - Modified visibility on naviation entries

## **Version and Date**
|Version|**OA-245**|
| :- | :- |
|**Date**|**05/16/2022**|

## **Issues Addressed**
 * Jira OA-245: Add ITM reliability as configurable parameter to AFC Config

## **Interface Changes**
 * UI and afc_config.json now have itmReliability as a configurable parameter whenever ITM is used in the Propagation Model.

## **Testing Done**
 * Did a test, one with 0.1% Reliability and another with 99.9% Reliability. 
 * Confirmed in engine logs and afc-config.json that the correct value was passed to the engine. 
 * In the exc_thr file attached to JIRA ticket (see columns AC to AG), confirmed that ITM path loss for 99.9% Relability is higher than that at 0.1%. Note that since we clamp path loss lower than FSPL to FSPL, we don't see much change in the results.

## **Open Issues**


## **Version and Date**
|Version|3.3.18.2|
| :- | :- |
|**Date**|**05/10/2022**|
|compiled server's version is 3.3.18-0ca8ecf |git tag 3.3.18.2|


## **Version and Date**
|Version|**OA-239**|
| :- | :- |
|**Date**|**05/09/2022**|

## **Issues Addressed**
 * Jira OA-239: Add additional features to Virtual AP's google map
   - Added functionality to update the coordinates in the location for ellipse center when map is clicked on
   - Changed the map display to update when Send Request is clicked instead of when response is received  

## **Version and Date**
|Version|3.3.18|
| :- | :- |
|**Date**|**05/09/2022**|
|compiled server's version is 3.3.18-1e88408 |git tag 3.3.18.1|
 
## **Version and Date**
|Version|**OA-85**|
| :- | :- |
|**Date**|**05/06/2022**|

## **Issues Addressed**
 * Jira OA-85: Merge Point Analysis and Virtual AP
   - Add EIRP chart and mapping to the Virtual AP page
 
## **Interface Changes**
 * afc_config.json added a new optional property "enableMapInVirtualAp" which if set to true will enable the map control on the virtual AP page and have the engine generate the map data
 * Data files containing map data are provided to the Virtual AP page if the enableMapInVirtualAp is true via a vendor extension on the AvailableSpectrumInquiryResponse using the extension ID openAfc.mapinfo

## **Testing Done**
 * Reviewed modified output pages

## **Open Issues**
* The only location type that is mapped is an Ellipse. Polygon locations will be handled in a later ticket
* Clicking the map to change the location point is not supported and will be handled in a later ticket
* The obsolete code for the Point Analysis pages remains in the project until these two issues are resolved


## **Version and Date**
|Version|**OA-216**|
| :- | :- |
|**Date**|**04/29/2022**|


## **Issues Addressed**
 * Jira OA-216: Log FS Rx that were not analyzed due to being inside uncertainty region
 * Per comments in the ticket, a new log file is generated that contains the following information for each FS that is in-band with at least one of RLAN channels, including FS inside RLAN uncertainty region. 
 * The file contains: FS ID, Rx CallSign, Tx CallSign, FS start and end frequency, Rx Lat/Long, Rx AGL height, Min distance to the scan points. 
 * If the FS is inside RLAN uncertainty region, distance is set to 0 meters. 
 * The file is called "fs_analysis_list.csv” by default but the filename can be changed in the AFC-Config
 
## **Interface Changes**
 * afc_config.json can specify the name of this file (if desired). If it's missing, the default filename is used.

## **Testing Done**
 * Did a test and confirmed list of FS in "fs_analysis_list.csv” with engine logs (that shows FS inside RLAN uncertainy region) and exc_thr (list of other FS) and the .sqlite3 (on the remaining FS data).

## **Open Issues**


## **Version and Date**
|Version|**3.3.17**|
| :- | :- |
|**Date**|**04/27/2022**|


## **Issues Addressed**
 * Jira OA-195: Add RKF and Federated Resampled NLCD database to AFC Config
 
## **Interface Changes**
 * There is a change in AFC Config UI and json file. The NLCD database can now be selected by the user.
 * Note that "WFA Test NLCD" (selected in UI) points to "federated_nlcd.tif" in afc_config.json file. And "Production NLCD" (selected in UI) points to ": "nlcd_2019_land_cover_l48_20210604_resample.tif" in afc_config.json file.
 

## **Testing Done**
 * Did two tests: one with Production NLCD and another with WFA Test NLCD and the response json files and the exc_thr files matched. These are attached to the Jira ticket.


## **Open Issues**
 * More tests are needed (this will be done as part of prepartion of WFA test results) to see whether federated's NLCD matches RKF's at least for the WFA tests.
 * Next, more tests are needed to ensure all tiles over CONUS are equivalent (if desired).  
 * 
## **Version and Date**
|Version|**OA-193**|
| :- | :- |
|**Date**|**04/27/2022**|

## **Issues Addressed**
 * Jira OA-193: Update AFC-Config Default form to WFA test vector configuration
 
## **Interface Changes**
 * The part of the code that sets the default setting (hard-coded) has been updated.

## **Testing Done**
 * Tested that when clicking "Reset to Default" in AFC Config, the default configuration is loaded.
 * Two afc_config json files are attached to this ticket: 1) afc_config_default.json and 2) afc_config_horizontal.json

## **Open Issues**

|Version|**OA-176, OA-188**|
| :- | :- |
|**Date**|**04/22/2022**|

## **Issues Addressed**
 * Jira OA-176: Support for global operating class 135 and 136
 * Jira OA-188: web UI changes to support start frequency of 5925MHz
## **Interface Changes**
 * None

## **Testing Done**
 * Checked in-band EIRP and PSD calculations for a few bands (for the new channel and some of the previous channels) as well as adjacent channel interference levels.
 * See the test case attached to OA-176 ticket. Note this uses the old ULS structure.

## **Open Issues**

|Version|**OA-192**|
| :- | :- |
|**Date**|**04/26/2022**|

## **Issues Addressed**
 * Jira OA-192: For adding clutter at FS Rx, add configurable percentage of location (P.2108) and max FS height

## **Interface Changes**
 * OA-190: UI/API changes to allow selection of P.2108 percentage of locations and max FS AGL height in AFC Config and passing them to AFC engine.

## **Testing Done**
 * OA-192: ran 2 tests and reviewed the exc_thr file. These are attached to the ticket.
 * Test1: Confirmed that P.2108 (see columns "PATH_CLUTTER_RX", "PATH_CLUTTER_RX_MODE") is applied at FS Rx when its AGL height ("FS_RX_HEIGHT_ABOVE_TERRAIN (m)") < 6m, distance ("RLAN_FS_RX_DIST (km)") > 1km and is not in Rural ("FS_RX_PROP_ENV"). Also, 5% is used for P.2108 percentage of locations ("PATH_CLUTTER_RX_CDF").
 * Test2: Confirmed in exc_thr file that a different AFC Config parameters (50% P.2108 and 20m Max FS AGL height) are used correctly.

## **Open Issues**

|Version|**OA-190, OA-191**|
| :- | :- |
|**Date**|**04/26/2022**|


## **Issues Addressed**
 * Jira OA-190: Implement WINNF-TS-1014 R2-AIP-07 FS Radiation Pattern Envelope
 * Jira OA-191: Update OA-127 to 55% antenna efficiency

## **Interface Changes**
 * OA-190: UI/API changes to allow selection of "WINNF-AIP-07" antenna pattern in AFC-Config and passing that to the AFC-Engine.
 * OA-191: one change in ULS-parser (to use antenna efficiency of 55% instead of 54% in calculation of antenna diameter). This results in an updated .sqlite3 ULS.
 

## **Testing Done**
 * OA-190: ran a test and reviewed the exc_thr file.
    - Confirmed that for all cases that categroyB1 was used, antenna model was blank in ULS.
    - Confirmed that the FS antenna discrimination gain (FS_ANT_GAIN_PEAK (dB) - FS_ANT_GAIN_TO_RLAN (dB)) matches Category A, Category B1 or Category B2 per R2-AIP-07. See attached plots in the exc_thr file.
    - Confirmed that when FS Rx peak gain < 38 dBi, for angle-off-boresight < 5 deg, F.699 is used and for angle-off-boresight >= 5 deg, Category B2 is used.
    - and when FS Rx peak gain >= 38 dBi, for angle-off-boresight < 5 deg, F.699 is used. For angle-off-boresight >= 5 deg, if antenna model (in ULS) is blank, Category B1 is used, else Category A is used.
    - Confirmed antenna pattern of Category A, B1 and B2 implemented.

 * OA-191: confirmed in .csv ULS that the antenna diameter was computed correctly (implemented the R2-AIP-05 formula in excel and compared with the value generated by ULS-parser). See attached ULS file columns BN and BO.

## **Open Issues**



## **Version and Date**

|Version|**3.3.16**|
| :- | :- |
|**Date**|**04/21/2022**|

## **Issues Addressed**
 * Jira OA-127: Implement WINNF rules on FS Antenna Diameter and Gain (WINN-21-I-00132 r14)
 
## **Interface Changes**
 * The ULS parser has been updated. This includes computation of 1) FS Rx antenna diameter based on the gain and 2) altering the Rx gain (if missing or below or above a threshold).
 * The sqlite3 has now an additional column (rx_ant_diameter).

## **Testing Done**
In addition to all tests done by the developer making changes, the following additonal tests were done by another person.
 * Confirmed that sqlite3 uls table has the new (rx_ant_diameter) column. 
 * Confirmed in the anomalous_uls.csv file (in the zipped debug folder under ULS Database) that when Rx gain is missing, antenna diamteter is set to 1.83m and the Rx gain is set to 39.5 dBi (when in UNII-7) and set to 38.8 dBi (when in UNII-5). This confirms part (e) of R2-AIP-05 in the WINNF document.
 * Confirmed that the missing Rx gain values (and the corresponding diameters) are set correctly in the .csv ULS file.
 * Confirmed in the .csv ULS file that the diameter is set in accordance with part (d) of WINNF's R2-AIP-05 by implementing the formula in excel separately and comparing the results from the parser. 
 * Confirmed in the .csv ULS file that the Rx gain value is between 32 and 48 dBi per part (d) of WINNF's R2-AIP-05.
 * Confirmed in the anomalous_uls.csv file (based on its last column) that when the Rx gain is below 32, it is set to 32 and when it's above 48, it is set to 48 dBi. An example message is "Rx Gain = 26.4 < 32, Rx Gain set to 32."
 * See attached uls and anomalous files (in .xlsx) to the ticket.

## **Open Issues**
The manual parser not working from the GUI.


## **Version and Date**
|Version|**3.3.15**|
| :- | :- |
|**Date**|**04/19/2022**|

## **Issues Addressed**
 * Jira OA-181: Set the new parameter "rlanITMTxClutterMethod" in AFC Config correctly
 
## **Interface Changes**
 * None

## **Testing Done**
 * Set AFC-Config to default, and other propagation models and confirm that the "rlanITMTxClutterMethod" is set correctly in afc_config.json and the propagation model is chosen correctly in exc_thr file.
 * See the test results attached to this ticket. 
 * Confirmed that "rlanITMTxClutterMethod": "FORCE_TRUE" (in afc_config.json) when default, FSPL and FCC 6GHz Report & Order are selected for the propagation model..
 * Confirmed that "rlanITMTxClutterMethod": "BLDG_DATA" when "ITM with buildign data" is selected for the propagation model.
 * Confirmed that "rlanITMTxClutterMethod": "BLDG_DATA" when "Custom" propagation model with LoS/NLoS based on building data is selected.
 * COnfirmed that "rlanITMTxClutterMethod": "FORCE_FALSE" when "Custom" propagation model with WinnerII LoS and Never add Clutter for > 1km is selected.
## **Open Issues**


## **Version and Date**
|Version|**v3.3.14**|
| :- | :- |
|**Date**|**04/04/2022**|

## **Issues Addressed**
* Jira OA-40: Add ability to select WinnerII LOS/NLOS only (instead of Combined) in AFC Config
    * GUI Updates to support selection that is more user configurable of Winner II (LoS/NLoS/Combined) and ITM (with or without P.2108/P.452).


## **Version and Date**
|Version|**v3.3.13**|
| :- | :- |
|**Date**|**03/17/2022**|



## **Issues Addressed**
* Conformance with WFA interface specification (v1.1):
    *  Jira OA-112: EIRP/PSD reporting for FS inside AP uncertainty region

* Conformance with WFA test vectors:
    * Jira OA-13: Add ability to use F.699 antenna pattern (instead of F.1245) for FS Rx
    * Jira OA-110: Change RLAN Uncertainty Region Scan Points per WFA agreed methodology
    * Jira OA-114: Ability to upload Canadian and Mexican FS database to AFC for US AP channel availability analysis [Note: this is only implemented in the engine; UI implementtation and add'l testing is on hold until the need for this feature is confirmed.]

* ULS-Parser changes:
   * Jira OA-94: How to handle FS links with FS Tx antenna pointing to different Rx

* Other:
   * Jira OA-81: Changes in assignment of AP height (this now includes the UI implementation)
   * Jira OA-40: Add ability to select WinnerII LOS/NLOS only (instead of Combined) in AFC Config [Note: this is only implemented in the engine; UI implementation will be in next week.]


## **Feature Additions/Changes**

### 1. F.699 antenna pattern has been added to be used as default for FS receivers 
### 2. Choose scan points within RLAN uncertainty region to align with 3DEP 1arcsec grid centers


## **Interface Changes**


## **Bug Fixes**
 * Jira OA-112: EIRP/PSD reporting for FS inside AP uncertainty region


## **Testing Done**
In addition to all tests done by the developer making changes, the following additonal tests were done by another person.
* EIRP for FS inside RLAN uncertainty region: tested the configuration in Point Analysis and those NULL EIRP channels were colored in black (instead of Red). In Virtual AP, the NULL EIRPs were not reported in the analysisResponse.JSON as desired. (see OA-112JSONfiles.docs attached to the Jira ticket.)
* ULS Parser: confirmed in the updated ULS (CONUS_ULS_2022-03-10T16_47_26.958955_fixedBPS_sorted.sqlite3) that CallSigns WQMY782(path 2) and WRCJ254(path 2) are now included in the database.
* F.699: Confirmed in "FS_ANT_TYPE" (in exc_thr) that only F.1245 or F.699 is used (per AFC-Config). Next, "FS_ANT_GAIN_TO_RLAN (dB)" vs. "FS_RX_ANGLE_OFF_BORESIGHT (deg)" is plotted from the exc_thr file and compared against ITU-R (F.1245 or F.699) formula implemented in excel. This is done for both F.1245 and F.699 to ensure both are used properly as selected by the user. Also, confirmed that channel availability results got worse when using F.699 as expected (I/Ns were 4 to 6.6 dB higher with F.699) . (see "F.699tests.zip" attached to Jira OA-13.) 
* Scan points: this was tested by opening 3DEP tiles and the scan points (from results.kmz) in QGIS for the three uncertainty region types. (see ScanPointsImplementation3DEPgridCenter.gz attached to this Jira OA-110.)
* Changes in AP height assignment: selections made in UI were confirmed in exc_thr file and results.kmz. Specifically when in AFC-Config "AP Height below Min Allowable AGL Height Behavior" is set to "Truncate AP height to min allowable AGL height," using a height-uncertainty that brings the height to 0m, exc_thr file's "RLAN_HEIGHT_ABOVE_TERRAIN (m)" confirms that 1m is used as minimum AP height. Next, when "AP Height below Min Allowable AGL Height Behavior" is set to "Discard scan point," exc_thr and results.kmz files confirm that the lower scan point has been removed from analysis (have only 2 heights at each AP location instead of 3) (see "APHeightBelowGroundTests_withUI.zip" attached to Jira OA-81). These were confirmed for Indoor AP as well but not included in the attachment.


## **Known Issues**
* As mentioned in Jira OA-21 comment and as agreed with the TIP Maintainer, automatic daily parsing of the ULS is currently not functional. Potential solutions are currently under discussion.
* OA-40 to be implemented in the UI.

## **Potential Improvements** 
*

## **Version and Date**
|Version|**v3.3.12**|
| :- | :- |
|**Date**|**03/04/2022**|



## **Issues Addressed**
* Bug fix in WFA interface specification (v1.1):
 * Jira OA-89: Add "US" to rulesetId in AP request message (Virtual AP)

* GUI enhancements:
 * Jira OA-98: Error runs on Virtual AP to generate engine outputs under Debug folder

* ULS-Parser bug fixes/changes:
 * Jira OA-22: ULS script parsing coordinates incorrectly
 * Jira OA-93: Include FS links marked as "Mobile" in ULS
 * Jira OA-33: Some of the Passive Repeater data populated incorrectly
 * Jira OA-20: ULS Parser to handle links with multiple passive repeaters
 * Jira OA-94: How to handle FS links with FS Tx antenna pointing to different Rx: only partially fixed by using the path number in [AN] table; this is still open question to FCC (see Jira ticket for details)

* Update of RAS Database:
 * Jira OA-15: Update RAS Database (update of antenna AGL heights in RAS database to match WINNF-TS-1014-v0.0.0-r5.0 table 2). See attached updated file. Updated file needs to replace the existing one under /var/lib/fbrat.

* Other:
 * OA-87: RKF to remove the unneeded comments/debug lines from the source-code
 * OA-81: Changes in assignment of AP height (partial - per comment in Jira only Engine portion has been implemented). 


## **Feature Additions/Changes**

### 1. Engine end-to-end test:
* Jira OA-9: Create an end-to-end regression test (for Engine)
**Reason for feature addition/change:** to perform automated regression testing of the engine when making changes (for internal use). This was updated to the latest ULS structure (03/02/2022)
* Please see attached JSONfiles_03March2022.zip that contains the json files for all tests as well as accompanying powerpoint and excel file that describes the configuration of each test.
* For the description of parameters, please refer to WFA interface specification v1.1.

### 2. ULS Parser:
* Added an interative mode for daily_uls_parse to facilitate degugging/development. 

### 3. Height change
* See attached tests


## **Interface Changes**
**ULS Database:** the Sqlite file has two tables: 
* uls table: includes ALL FS links (but no information on their passive repeaters, except num of passive repeaters, for those that have)
* pr table: includes information on all passive repeaters of the relevant FS. Each passive repeater of a given FS will be in a different row. This allows max flexibility and efficiency to handle any number of Passive Repeaters.
* You need to put the CONUS_ULS_2022-03-02T03_34_41.097782_fixedBPS_sorted.sqlite3 in /var/lib/fbrat

**VirtualAP**
VirtualAP analysisResponse.json now has .gz extention consistently.


## **Bug Fixes**
* See **Issues Addressed**


## **Testing Done**
In addition to all tests done by the developer when making changes, the following additonal tests were done by another person.
* End-to-end regression tests (tests 1 through 6) passed.
* ULS Parser: specific issues (per Jira tickets) were confirmed against FCC online database and against Fedor's script.
* Engine pointing to the last passive repeater: this was tested by checking the pointing direction of FS Rx (using results.kmz) against expected direction using ULS data in google-Earth. The test was done for 3 FS links with 1, 2 and 3 passive repeaters. See the json request and response message for each test in attached "EnginePointtoLastPR_220302.zip"
* Handling AP height below ground (OA-81) is tested for the various cases (indoor, outdoor and desired error case as describe in the jira ticket). The tests' Readme, configuration and JSON files are in "APHeightBelowGroundTests.zip" attached.

## **Known Issues**
* As mentioned in Jira OA-21 comment and as agreed with the TIP Maintainer, automatic daily parsing of the ULS is currently not functional. Potential solutions are currently under discussion.


## **Potential Improvements** 

## **Version and Date**
|Version|**v3.3.11**|
| :- | :- |
|**Date**|**02/14/2022**|


## **Issues Addressed**
Update of WFA interface specification to 1.1 (latest version) from 0.0.6
- Jira OA-7: Add rulsetId object
- Jira OA-8: Exclusion of frequencies/channels in the response where emission is prohibited
- Jira OA-16: Add "version" to request message
- Jira OA-17: Add globalOperatingClass in response message
- Jira OA-18: If AP sends an incorrect globalOperatingClass, the request will be rejected as invalid
- Jira OA-19: AFC to send a JSON response message in response to UN-successful requests
- Jira OA-25: Reject message if semi-major axis < semi-minor axis

ULS-Parser enhancements:
- Jira OA-12: Convert .sqlite3 ULS to .csv so that it contains FS ID
- Jira OA-39: Improve ULS Processor Speed


## **Feature Additions/Changes**

### 1. Updated the WFA interface specification to 1.1
**Reason for feature addition/change:** Compatible with the latest specification

### 2. Enhanced ULS-Parser 
**Reason for feature addition/change:** speed-up ULS-parser significantly (from hours to few min) and presence of FS-ID in the .csv file.


## **Interface Changes**

**WFA interface specification:** updated to latest version (1.1) from 0.0.6
**For the new certificationId format (per 1.1 ICD)**,the admin API that adds new APs (/admin/user/ap/<id>), still uses a single certificationId property that is nra+" "+certificationId. This matches the database schema. The Admin and Virtual AP GUI and the AP request JSON message were chagned to accomodate this change.
**ULS Parser:** No changes


## **Bug Fixes**
* None


## **Testing Done**
* End2End testing (tests 1 through 6) confirms the WFA interface specification. 
In addition, specific tests were done to trigger and confirm the expected Error code.

* For the daily uls parser: speed was timed and the .csv was verified to have FS IDs.

## **Known Issues**
* Jira tickets pertaining to the ULS parser: automation and correction of passive repeater information

## **Potential Improvements** 
*

## **Version and Date**
|Version|**v3.3.10**|
| :- | :- |
|**Date**|**12/20/2021**|


## **Issues Addressed**
Jira OA-10: For FS links with passive receiver, FS Rx to point to the passive repeater
Jira OA-11: Testing of ULS Script Parser

## **Feature Additions/Changes**

### 1. Tested the ULS Script Parser that was integrated with the GUI in v3.3.9. This is available to the Administrator in the GUI under the Admin tab.

**Reason for feature addition/change:** FCC R&O Requirement


## **Interface Changes**

**New section in admin tab:** Controls to manually start a ULS parse and a control to update the automatic parse time.


## **Database Changes**
1. Databases used by AFC-Engine (note these are needed for v.3.3.9 build as well)
ITU-R Rec. P.617-3 Climate map (TropoClim.txt)
ITU-R Rec. P.452 Surface Refractivity map (N050.TXT)
These are under rat_transfer > itudata

2. there are new data files for the daily parser that need to be accomodated. 
**Dynamic Data:** 
* Add a directory /var/lib/fbrat/daily\_uls\_parse
* Add a sub directory under that directory called data_files
* Provided with this update are the files that need to live under that directory



## **Bug Fixes**
* For FS links with passive repeater, FS RX is pointed towards the passive repeater (rather than FS Tx).


## **Testing Done**
* For the passive repeater, did a Point analysis where RLAN's ellipse uncertainy region was centered at the Passive Repeater of FS ID 212911. 
Confirmed from the beamcones (in results.kmz) that FS Rx was pointing towards the passive repeater. 
Confirmed from exc_thr that RLAN was in the main beam of this FS Rx.
Confirmed all ULS parameters of this FS (across .csv, .sqlite3 and exc_thr) were used properly.

* For the daily uls parser: manual functionality was run until completion and then analyses were run with the resulting ULS. The automated task was tested by verifying celery logs and more manual testing using the resulting database. 


## **Known Issues**
* None

## **Potential Improvements** 
* Most if not all of the text files under /var/lib/fbrat/daily\_uls\_parse/data_files can be replaced with a simple database
* These files contain only one piece of data other than yesterday's database. 


## **Version and Date**
|Version|**v3.3.9**|
| :- | :- |
|**Date**|**12/13/2021**|


### **Feature Additions/Changes**

### 1. Determine P.452 Clutter Category (for Rural) based on NLCD tile (instead of always assuming Village Center) per Rural-D, Rural-C and Rural-V below.
* Urban: if NLCD tile = 23 or 24
* Suburban: if NLCD tile = 22 or 21
* Rural-D (Deciduous tree): if NLCD tile = 41, 43 or 90
* Rural-C (Coniferous tree): if NLCD tile = 42
* Rural-V (Village Center): otherwise

**Reason for feature addition/change :** FCC R&O Requirement

### 2. Determine RLAN morphology (used to determine WinnerII and Clutter loss model) based on NLCD tile (rather than on population density threshold) per below.
* Urban: if NLCD tile = 23 or 24
* Suburban: if NLCD tile = 22 or 21
* Rural: Otherwise

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 3. Assess Channel availability for all scan points - with uniform spacing - within the RLAN uncertainty region, with scanning resolution configurable from AFC-Config.

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 4. Ability add clutter at FS RX (when FS Rx is at least 1km from RLAN and has AGL height <= 10m). This is configurable from AFC Config.

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 5. Ability to configure the following ITM parameters (from AFC Config):
* Polarization
* Ground Type to set Dielectric Constant and Conductivity
* Points in the Path Profile (minimum spacing and maximum points)

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 6. Derive ITM parameter "Surface refractivity" from ITU-R Recommendation P.452 surface refractivity map (instead of using a constant value). The value is set to that at the mid-point of the path between the AFC Device and FS Receiver.

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 7. Derive ITM parameter "Climate" from ITU-R Recommendation P.617-3 Radio Climate map and set to the smaller value of that at RLAN and FS Rx (per P.617-3).

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 8. Make FS Receiver Feederloss and Noise Floor configurable for UNII-5, UNII-7 and other frequencies in AFC-config.

**Reason for feature addition/change :** Wi-Fi Alliance Test Vector


### 9. The ULS Script Parser has been integrated with the GUI. However, this is not fully functional yet and is still under development.

**Reason for feature addition/change :** ULS Script Parser



## **Interface Changes**
There have been interface (API) changes for all feature changes 1 through 7 in Section 2 above as all of those options have been made configurable from the GUI.

## **Database Changes**
NLCD Database has been added to  /usr/share/fbrat/rat_transfer.

## **Bug Fixes**
* RLAN outside RAS exclusion zone was getting channels overlapping with RAS frequency band blocked.

## **Testing Done**
All changes above have been tested manually in the fashion of previous builds.
This involves setting up tests that check whether the desired configuration (as set in the GUI) has been used by the AFC-Engine (the C++ code) and whether the expected implementation was done - by checking the exc_thr file, QGIS (to lookup NLCD tile), engine logs (to verify ITM parameters) and the kmz file (to verify orientation of scanning points).
In addition, an end-to-end regression test has been done after each change.
Note that as we have started upgrading the code for more rigorous unit-testing, we performed unit-tests on Climate and Surface Refractivity functions as those could not be verified through external means.

## **Known Issues**
* ULS Script Parser is currently under testing.
* For FS links with passive repeater, FS RX is pointed towards FS TX, rather than the passive repeater. This will be fixed in the next release.

