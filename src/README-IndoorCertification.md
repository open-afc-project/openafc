# Overview

This document discusses the interaction with how the indoor deployment field in the Request interacts with the Indoor or Outdoor location that is kept in the Certification Id database.  For reference, the Certification Id database is managed by means of the "sweep" functionality that is performed by the cert_id service that connects to country regulatory organizations (for US that is the FCC, for CA that is ISED) and downloads information about certified installations.

# Fields
There are two places to specify the location (by "location" I mean indoor or outdoor here)

In the request in the "indoorDeployment" field

* Indoor: 1
* Outdoor: 2
* Undefined: 0

In the Certification Id database table inside ratdb there is a bit flag

* CERT_ID_LOCATION_UNKNOWN = 0
* CERT_ID_LOCATION_INDOOR = 1
* CERT_ID_LOCATION_OUTDOOR = 2

This can have a value of 3 (which is both INDOOR and OUTDOOR)
 
In the request, the indoorDeployment field is set by the requestor.
 
# Certification Id Database

For the certification database, the Indoor/Outdoor value is set in the Cert ID Sweep function (src/ratapi/ratapi/manage.py:868 and following)
 
## For US sweeps:
The service queries for items on the 6SD list:

* equipment_class=250
* equipment_class_description=6SD-15E 6 GHz Standard Power Access Point
 
If the ID is already in the database, it gets the refresh time updated, and the OUTDOOR bit is set. If the INDOOR bit was set previously, both are present.
If the ID is not in the database, it gets added with the OUTDOOR location.
 
Service next queries FCC for the 6ID list:

 * equipment_class=243
 * equipment_class_description=6ID-15E 6 GHz Low Power Indoor Access Point
 
If the cert ID is already in the database, the INDOOR bit is set.   If the OUTDOOR bit was set previously, both are present.
 
If the cert ID is not in the database, nothing happens. Note that this is different than the 6SD entries.
 
## For CA sweeps
The SD6 CSV file is downloaded and the service adds everything in the SD6 list that has a known location code in column 7:

 * For 103, location is set to OUTDOOR (2)
 * For 111, location  is set to INDOOR | OUTDOOR (both flags, 3)
 * For all others, location is UKNOWN (0) but the Cert ID is not added to the database
 
## Request processing
 
When a request is made, the python API processor looks up the Certification Id in the database and checks the location field.
If the location does not have the  OUTDOOR bit set (location = 0 or 1, UNKNOWN or just INDOOR), the request is rejected with the error "Outdoor operation not allowed".
 
If the location has the INDOOR bit set  (INDOOR (1) or INDOOR and OUTDOOR (3)) , the indoor_certified API flag is set to True.  Note that if the location is only INDOOR, the request would have been rejected above, so only locations of both INDOOR and OUTDOOR will set the indoor_certified API flag.
 
The indoor_certified API flag is used to set the RNTM_OPT_CERT_ID (32) flag which gets passed to the engine via the runtime_opt parameter. Inside the engine, that sets the engine flag AfcManager::_certifiedIndoor to true;
 
# AFC Engine

There are three parameters passed to the afc-engine:
 
(1) _certifiedIndoor  boolean, 'true' or 'false'

This parameter is passed as a command line option when the afc-engine is executed.  This is part of the bitmask of the runtime_opt command line parameter.  This parameter is set to 'true' of "runtime_opt & 0x20" is non-zero, and 'false' if zero.  If runtime_opt is not specified at the command line, _certifiedIndoor is set to false.
 
(2) indoorDeployment  integer 0, 1, or 2 This parameter is set in the analysis request json file.  If set to a value other than 0, 1, or 2, then afc-engine exits with an invalid parameter error message.  It is also possible this parameter is not specified.
 
(3) _rulesetID string This parameter is set in the analysis request json file.
 
 
Below is the logic for determining if the RLAN is considered INDOOR or OUTDOOR, see lines 1840 - 1881 of AfcManager.cpp:
 
    If (indoorDeployment == 0) and (_certifiedIndoor == 'true')

    RLAN_TYPE = INDOOR
    
    If (indoorDeployment == 0) and (_certifiedIndoor == 'false')

    RLAN_TYPE = OUTDOOR
    
    If (indoorDeployment == 1) and (_rulesetID == "US_47_CFR_PART_15_SUBPART_E") and (_certifiedIndoor == 'true')

    RLAN_TYPE = INDOOR
    
    If (indoorDeployment == 1) and (_rulesetID == "US_47_CFR_PART_15_SUBPART_E") and (_certifiedIndoor == 'false')

    RLAN_TYPE = OUTDOOR
    
    If (indoorDeployment == 1) and (_rulesetID != "US_47_CFR_PART_15_SUBPART_E") 

    RLAN_TYPE = INDOOR
    
    If (indoorDeployment == 2) 

    RLAN_TYPE = OUTDOOR
    
    If (indoorDeployment not specified) and (_certifiedIndoor == 'true')

    RLAN_TYPE = INDOOR
    
    If (indoorDeployment not specified) and (_certifiedIndoor == 'false')

    RLAN_TYPE = OUTDOOR

 
 

	
 
	
 
	
 
 
 