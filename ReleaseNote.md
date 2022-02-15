# Release Note

# **Version and Date**
|Version|**v3.3.11**|
| :- | :- |
|**Date**|**02/14/2022**|


## **Issues Addressed**
Update of WFA interface specification to 1.1 (latest version) from 0.0.6
Jira OA-7: Add rulsetId object
Jira OA-8: Exclusion of frequencies/channels in the response where emission is prohibited
Jira OA-16: Add "version" to request message
Jira OA-17: Add globalOperatingClass in response message
Jira OA-18: If AP sends an incorrect globalOperatingClass, the request will be rejected as invalid
Jira OA-19: AFC to send a JSON response message in response to UN-successful requests
Jira OA-25: Reject message if semi-major axis < semi-minor axis

ULS-Parser enhancements:
Jira OA-12: Convert .sqlite3 ULS to .csv so that it contains FS ID
Jira OA-39: Improve ULS Processor Speed


## **Feature Additions/Changes**

### 1. Updated the WFA interface specification to 1.1
**Reason for feature addition/change:** Compatible with the latest specification

### 2. Enhanced ULS-Parser 
**Reason for feature addition/change:** speed-up ULS-parser significantly (from hours to few min) and presence of FS-ID in the .csv file.


## **Interface Changes**

**WFA interface specification:** updated to latest version (1.1) from 0.0.6
**ULS Parser:** No changes


## **Bug Fixes**
* None


## **Testing Done**
* End2End testing (tests 1 through 6) confirms the WFA interface specification and the RAS database change. 
In addition, specific tests were done to trigger and confirm the expected Error code.

* For RAS database change, the new antenna height is confirmed in the exc_thr file.

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

