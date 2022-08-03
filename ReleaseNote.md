# Release Note
|Version|3.3.22.1|
| :- | :- |
|**Date**|**07/20/2022**|
|compiled server's version is TBD |git tag 3.3.22.1|

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

