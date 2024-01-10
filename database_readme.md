# Database Readme

## **Database Description**

### **Details of Databases**
#### **FS_Database:**
contains parameters defining the FS links for interference analysis.
For the ULS databaes (for the US) These are:
* FS Tx/Rx CallSign
* FS Tx/Rx Lat/Long and Height above ground level,
* FS Diversity Height above ground level,
* FS Primary and Diversity Rx Gain, Antenna Model, Antenna Diameter,
* FS Start/End Frequencies and Bandwidth,
* FS passive repeater Lat/Long, Height above ground level, dimensions, antenna model, antenna diameter and gain
* FS Rx near-field adjustment factor parameters

contains parameters defining exclusion zone(s) around each RAS antenna that needs to be protected. 

contains parameters defining FS Rx actual antenna pattern (angle-off-boresight vs. discrimination gain).

#### **proc_lidar_2019:**
contains json files that allow showing boundaries of RAS exclusion zones and LiDAR in GUI.
* RAS_ExclusionZone.json
* LiDAR_Bounds.json.gz

This also contains all lidar tiles where each city has a subdirectory with tiles with a .csv that isn’t under the city subdirectory. The lidar zip file contains all of this.

#### **Multiband-BDesign3D:** 
contains building database over Manhattan.
#### **globe:** 
contains NOAA GLOBE (1km resolution) terrain database.
#### **srtm3arcsecondv003:** 
contains 3arcsec (=90m) SRTM terrain database files. These are used in the regions where 1arcsec 3DEP is used in the event a 3DEP tile is missing.
#### **srtm1arcsecond:** 
contains 1arcsec (=30m) SRTM terrain database files. This is used in regions where 1arcsec 3DEP is not available.
#### **3dep:** 
The 1_arcsec subdirectory (one currently used) contains 1arcsec (=30m) 3DEP terrain database files over US, Canada and Mexico.
#### **cdsm:** 
contains the Natural Resources Canada Canadian Digital Surface Model (CDSM), 2000 at the highest available resolution.

#### **nlcd:** 
contains nlcd_2019_land_cover_I48_20210604_resample.zip (referred to as "Production NLCD" in AFC Config UI) and federated_nlcd.zip (referred to as "WFA Test NLCD" in AFC Config UI) files. This is used to determine RLAN/FS morphology (i.e. Urban, Suburban or Rural) to pick the appropriate path/clutter loss model. In addition, it is used to determine the appropriate P.452 Rural clutter category.
#### **landcover-2020-classification:** 
The 2020 Land Cover of Canada produced by Natural Resources Canada.
#### **clc:** 
Corine Land Cover is land categorization over the EU used to determine RLAN/FS morphology. 
#### **population:** 
contains the Gridded Population of the World (GPW), v4.11, population density database. Use of GPW for determination of RLAN morphology is only used in the absence of a land cover database.

#### **US.kml:** 
specifies United States' country boundary where AP access is allowed for that region. 
#### **CA.kml:** 
specifies Canada's country boundary where AP access is allowed for that region. 
#### **GB.kml:** 
specifies the Great Britain country boundary where AP access is allowed for that region. 
#### **BRA.kml:** 
specifies Brazil's country boundary where AP access is allowed for that region.  

#### **itudata:** 
contains two ITU maps that are used by the ITM path loss model. 1) Radio Climate map (TropoClim.txt) and 2) Surface Refractivity map (N050.txt)

#### **winnforum databases:** 
these are WinnForum databases used by the FS Parser (antenna_model_diameter_gain.csv, billboard_reflector.csv, category_b1_antennas.csv, high_performance_antennas.csv, fcc_fixed_service_channelization.csv, transmit_radio_unit_architecture.csv). They provide the data to validate/fix/fill-in the corresponding ULS parameters. Two other WinnForum databases (nfa_table_data.csv and rat_transfer/pr/WINNF-TS-1014-V1.2.0-App02.csv) are used by the AFC Engine for near-field adjustment factor calculation for primary/diversity receivers and passive sites respectively. Note that the nfa_table_data.csv is generated manually as a simplied version of WINNF-TS-1014-V1.2.0-App01.csv. The use of these databases is described in WINNF-TS-1014 and WINNF-TS-5008 documents.

### **Location or procedure to download/acquire these databases**
* **FS_Database:** Created using FS Script Parser from ULS raw data on FCC website (see details in the ULS Script documentation), RAS database from FCC 47CFR Part 15.407, and ISED's 6GHz DataExtract database on https://ised-isde.canada.ca/site/spectrum-management-system/en/spectrum-management-system-data

* **proc_lidar_2019:** raw data obtained from https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/
* **Multiband-BDesign3D:** this was purchased https://www.b-design3d.com/

* **globe:** https://ngdc.noaa.gov/mgg/topo/globe.html
* **srtm3arcsecondv003:** https://www2.jpl.nasa.gov/srtm/
* **srtm1arcsecond:** https://search.earthdata.nasa.gov/search/granules?p=C1000000240-LPDAAC_ECS&pg[0][v]=f&pg[0][gsk]=-start_date&q=srtm&tl=1702926101.019!3!!
* **3dep:** https://data.globalchange.gov/dataset/usgs-national-elevation-dataset-ned-1-arc-second
* **cdsm:"** https://open.canada.ca/data/en/dataset/768570f8-5761-498a-bd6a-315eb6cc023d

* **nlcd:** original file nlcd_2019_land_cover_I48_20210604 was downloaded from [link](https://www.mrlc.gov/data?f%5B0%5D=category%3Aland%20cover) (download NLCD 2019 Land Cover (CONUS)). Using gdal utilties this file was translated to nlcd_2019_land_cover_I48_20210604_resample.zip so that the 1-arcsec tiles matchup with 1-arcsec 3DEP tiles. The federated_nlcd.zip file was obtained by using other gdal utilities to convert federated's many files to one file covering CONUS.
* **landcover-2020-classification:** original file was downloaded from [link](https://open.canada.ca/data/en/dataset/ee1580ab-a23d-4f86-a09b-79763677eb47). Using gdal utilies this file was translated to landcover-2020-classification_resampled.tif so that the 1-arcsec tiles matchup with 1-arcsec 3DEP tiles and the canada landcover classifications are mapped to the equivalent NLCD codes.
* **clc:** original file was downloaded from the [Copernicus](https://land.copernicus.eu/pan-european/corine-land-cover/clc2018) website (download the GeoTIFF data). Login is required. Using gdal utilies this file was translated to landcover-2020-classification_resampled.tif so that the 1-arcsec tiles matchup with 1-arcsec 3DEP tiles and the canada landcover classifications are mapped to the equivalent NLCD codes.
* **population:** https://sedac.ciesin.columbia.edu/data/set/gpw-v4-population-density-rev11

* **US.kml:** https://public.opendatasoft.com/explore/dataset/world-administrative-boundaries/export/?flg=en-us
* **CA.kml:** https://www12.statcan.gc.ca/census-recensement/2021/geo/sip-pis/boundary-limites/index2021-eng.cfm?year=21 (Catrographic Boundary files, selecting 'Provinces/territories' of Administrative boundaries)
* **GB.kml:** https://public.opendatasoft.com/explore/dataset/world-administrative-boundaries/export/?flg=en-us
* **BRA.kml:** https://public.opendatasoft.com/explore/dataset/world-administrative-boundaries/export/?flg=en-us
 
* **itudata:** Radio Climate map from ITU-R Rec, P.617-3 (https://www.itu.int/rec/R-REC-P.617-3-201309-S/en) and Surface Refractivity map from ITU-R Rec, P.452-17 (https://www.itu.int/rec/R-REC-P.452-17-202109-I/en)

* **winnforum databases:** The Winnforum databases used by FS parser can be downloaded from here: Use https://github.com/Wireless-Innovation-Forum/6-GHz-AFC/tree/main/data/common_data to open in browser.  The scripts use: https://raw.githubusercontent.com/Wireless-Innovation-Forum/6-GHz-AFC/main/data/common_data/ for downloading. The near-field adjustment factor databases can be downloaded from: https://6ghz.wirelessinnovation.org/baseline-standards. 

### **Licenses and Source Citations**

#### **proc_lidar_2019**
Available for public use with no restrictions

Disclaimer and quality information is at https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/00_NGA%20133%20US%20Cities%20Data%20Disclaimer%20and%20Explanation%20Readme.pdf 

#### **Globe**
Public domain

NOAA National Geophysical Data Center. 1999: Global Land One-kilometer Base Elevation (GLOBE) v.1. NOAA National Centers for Environmental Information. https://doi.org/10.7289/V52R3PMS. Accessed TBD

#### **3DEP**
Public domain

Data available from U.S. Geological Survey, National Geospatial Program.

#### **srtm1arcsecond**
Public domain

NASA JPL (2013). NASA Shuttle Radar Topography Mission Global 1 arc second [Data set]. NASA EOSDIS Land Processes Distributed Active Archive Center. 

#### **CDSM**
Natural Resource of Canada. (2015). Canada Digital Surface Model [Data set]. https://open.canada.ca/data/en/dataset/768570f8-5761-498a-bd6a-315eb6cc023d. Contains information licensed under the Open Government Licence – Canada.

#### **NLCD**
Public domain

References:

Dewitz, J., and U.S. Geological Survey, 2021, National Land Cover Database (NLCD) 2019 Products (ver. 2.0, June 2021): U.S. Geological Survey data release, https://doi.org/10.5066/P9KZCM54

Wickham, J., Stehman, S.V., Sorenson, D.G., Gass, L., and Dewitz, J.A., 2021, Thematic accuracy assessment of the NLCD 2016 land cover for the conterminous United States: Remote Sensing of Environment, v. 257, art. no. 112357, at https://doi.org/10.1016/j.rse.2021.112357 

Homer, Collin G., Dewitz, Jon A., Jin, Suming, Xian, George, Costello, C., Danielson, Patrick, Gass, L., Funk, M., Wickham, J., Stehman, S., Auch, Roger F., Riitters, K. H., Conterminous United States land cover change patterns 2001–2016 from the 2016 National Land Cover Database: ISPRS Journal of Photogrammetry and Remote Sensing, v. 162, p. 184–199, at https://doi.org/10.1016/j.isprsjprs.2020.02.019

Jin, Suming, Homer, Collin, Yang, Limin, Danielson, Patrick, Dewitz, Jon, Li, Congcong, Zhu, Z., Xian, George, Howard, Danny, Overall methodology design for the United States National Land Cover Database 2016 products: Remote Sensing, v. 11, no. 24, at https://doi.org/10.3390/rs11242971

Yang, L., Jin, S., Danielson, P., Homer, C., Gass, L., Case, A., Costello, C., Dewitz, J., Fry, J., Funk, M., Grannemann, B., Rigge, M. and G. Xian. 2018. A New Generation of the United States National Land Cover Database: Requirements, Research Priorities, Design, and Implementation Strategies, ISPRS Journal of Photogrammetry and Remote Sensing, 146, pp.108-123.

#### **2020 Canada Land Cover**
Natural Resource of Canada. (2022). 2020 Land Cover of Canada [Data set]. https://open.canada.ca/data/en/dataset/ee1580ab-a23d-4f86-a09b-79763677eb47. Contains information licensed under the Open Government Licence – Canada.

#### **Corine Land Cover**

Access to data is based on a principle of full, open and free access as established by the Copernicus data and information policy Regulation (EU) No 1159/2013 of 12 July 2013. This regulation establishes registration and licensing conditions for GMES/Copernicus users and can be found [here](http://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX%3A32013R1159)

Free, full and open access to this data set is made on the conditions that:

1. When distributing or communicating Copernicus dedicated data and Copernicus service information to the public, users shall inform the public of the source of that data and information.
2.  Users shall make sure not to convey the impression to the public that the user's activities are officially endorsed by the Union.
3.  Where that data or information has been adapted or modified, the user shall clearly state this.
4.  The data remain the sole property of the European Union. Any information and data produced in the framework of the action shall be the sole property of the European Union. Any communication and publication by the beneficiary shall acknowledge that the data were produced “with funding by the European Union”.

Reference:

&copy;European Union, Copernicus Land Monitoring Service 2018, European Environment Agency (EEA)

#### **population**
Creative Commons Attribution 4.0 International (CC BY) License (https://creativecommons.org/licenses/by/4.0)

Center for International Earth Science Information Network - CIESIN - Columbia University. 2018. Gridded Population of the World, Version 4 (GPWv4): Population Density, Revision 11. Palisades, New York: NASA Socioeconomic Data and Applications Center (SEDAC). https://doi.org/10.7927/H49C6VHW 

#### **Canada country boundary**
Statistics Canada (2022). Boundary Files, Census Year 2021 [Data set]. https://www12.statcan.gc.ca/census-recensement/2021/geo/sip-pis/boundary-limites/index2021-eng.cfm?year=21. Reproduced and distributed on an "as is" basis with the permission of Statistics Canada.


#### **winnforum databases**
Available for public use under the copyright of The Software Defined Radio Forum, Inc. doing business as the Wireless Innovation Forum.

THIS DOCUMENT (OR WORK PRODUCT) IS BEING OFFERED WITHOUT ANY WARRANTY WHATSOEVER, AND IN PARTICULAR, ANY WARRANTY OF NON-INFRINGEMENT IS EXPRESSLY DISCLAIMED.  ANY USE OF THIS SPECIFICATION (OR WORK PRODUCT) SHALL BE MADE ENTIRELY AT THE IMPLEMENTER'S OWN RISK, AND NEITHER THE FORUM, NOR ANY OF ITS MEMBERS OR SUBMITTERS, SHALL HAVE ANY LIABILITY WHATSOEVER TO ANY IMPLEMENTER OR THIRD PARTY FOR ANY DAMAGES OF ANY NATURE WHATSOEVER, DIRECTLY OR INDIRECTLY, ARISING FROM THE USE OF THIS DOCUMENT (OR WORK PRODUCT).

## **Database Update**

### **Expected update frequency of each database file**
* **FS_Database:** daily (per FCC and ISED requirements). Note that the RAS database portion is expected to be updated rarely.
* **proc_lidar_2019:** every few years (whenever a new database is available)
* **Multiband-BDesign3D:** no change (unless a newer building database for Manhattan is needed)
* **globe:** every few years (whenever a new database is available)
* **srtm3arcsecondv003:** every few years (whenever a new database is available)
* **srtm1arcsecond:** every few years (whenever a new database is available)
* **3dep:** every few years (whenever a new database is available)
* **cdsm:** every few years (whenever a new database is available)
* **nlcd:** every few years (whenever a new database is available)
* **2020canadalandcover:** every few years (whenever a new database is available)
* **clc:** every few years (whenever a new database is available)
* **population:** every few years (whenever a new database is available)
* **US.kml:** every few years (whenever an updated country boundary is available)
* **CA.kml:** every few years (whenever an updated country boundary is available)
* **GB.kml:** every few years (whenever an updated country boundary is available)
* **BRA.kml:** every few years (whenever an updated country boundary is available)
* **itudata:** these haven't been updated for a long time but can be updated if new maps are generated.
* **winnforum databases:** might be as early as every 6 months (at the discretion of WinnForum)

### **Database update procedure**
* **FS_Database:** FS Script Parser automatically updates this daily (see next section)

* **proc_lidar_2019:** download lidar database and post-process

* **globe:** download database. Convert to WGS84 using open-afc/tools/geo_converters/to_wgs84.py script.
* **srtm3arcsecondv003:** download database. Convert to WGS84 using open-afc/tools/geo_converters/to_wgs84.py script.
* **srtm1arcsecond:** download database. Convert to WGS84 using open-afc/tools/geo_converters/to_wgs84.py script.
* **3dep:** download database. Convert to WGS84 using open-afc/tools/geo_converters/to_wgs84.py script.
* **cdsm:** download database. Follow the procedures on open-afc/tools/geo_converters/Canada CDSM surface model
* **nlcd:** download database, run proper gdal utilties to orient the tiles matching 3DEP 1-arcsec tiles and put in the proper directory
* **clc:** download database, run proper gdal scripts (open-afc/tools/geo_converters) to convert the data categorization mapping and coordinate system and put in the proper directory
* **2020canadalandocover:** download database, run proper gdal scripts (open-afc/tools/geo_converters) to convert the data categorization mapping and coordinate system and put in the proper directory
* **population:** download database and put in the proper directory


## **Database Processing/Format**
### **Processing done (in any) on the original database to convert it to a format usable by afc-engine**
* **FS_Database:** generated by the FS Script Parser.
* **LiDAR_Database:** generated from significant post-processing:
    For each city:
   * (1) Identify bare earth and building files available.
   * (2) Identify pairs of files where a pair consists of a bare earth and building polygon file that cover the same region.
   * (3) Convert bare earth into tif raster file, and convert building polygon file into tif raster file where both files are on same lon/lat grid.
   * (4) Combine bare earth and building tif files into a single tif file with bare earth on Band 1 and building height on band 2. Both Band 1 and Band 2 values are AMSL.
   * (5) Under target directory create directory for each city, under each city create dir structure containing combined tif files.
   * (6) For each city create info file listing tif files and min/max lon/lat for each file.
* **srtm3arcsecondv003:** the two SRTM tiles over Manhattan are removed since they erroneously contain building height
* **country.KML:** there is some post-processing done that is not documented here as we are moving to using a different processing.

* **Near Field Adjustment Factor File:** "nfa_table_data.csv" is created as follows.

1. Download "WINNF-TS-1014-V1.2.0-App01 6GHz Functional Requirements - Appendix A.xlsx" from [https://6ghz.wirelessinnovation.org/baseline-standards](https://6ghz.wirelessinnovation.org/baseline-standards)


2. Note that this .xlsx file has 18 tabs labeled "xdB = 0 dB", "xdB = -1 dB", "xdB = -2 dB", ..., "xdB = -17 dB".  Also note that in each of these 18 tabs, there is data for efficiency values ($\eta$) ranging from 0.4 to 0.7 in steps of 0.05.  Further note the following: 
    - For each xdB and efficiency, there is a two column dataset with columns labeled u and dB 
    - The dB value is the adjustment factor.
    - For each of these 2 column datasets, the last value of adjustment factor is 0
    - For each of these 2 column datasets, u begins at 0 and increases monotonically to a max value for which adjustment value is 0. 
    - For each xdB value, the max value of u for efficiency = 0.4 is >= the max value of u for any of the other efficiency values shown.

3. Interpolate the data.  For each 2 column dataset, use linear interpolation to compute the adjustment factor for 0.05 increments in u.

4. Pad the data.  For each 2 column dataset, append values of u in 0.05 increments up to the max u value for efficiency = 0.4.  For these appended values append 0 for the adjustment factor value.

5. Assemble all this data into a single 3-column CSV file named as nfa_table_data.csv.  The file header is "xdB,u,efficiency,NFA".  Subsequent data lines list values for xdB, u, efficiency, and NFA for each of the interpolated/padded 2-column datasets.

* **Near Field Adjustment Factor for Passive Repeaters File:** "WINNF-TS-1014-V1.2.0-App02.csv" is created as follows.

1. Download "WINNF-TS-1014-V1.2.0-App02 6GHz Functional Requirements - Appendix B.xlsx" from [https://6ghz.wirelessinnovation.org/baseline-standards](https://6ghz.wirelessinnovation.org/baseline-standards)

2. In the "Data" tab of this .xlsx file, note that this tab contains tabular gain data where each column corresponds to a different Q and each row corresponds to a different value of 1/KS.

3. The algorithm implemented in afc-engine only uses this table for values of KS > 0.4.  This means 1/KS <= 2.5.  The values of 1/KS in the .xlsx file go up to 7.5.  For the purpose of interpolation, keep the first row in the file with 1/KS > 2.5 (2.512241), and delete all rows after this row with larger values of 1/KS.

4. Count then number of Q values in the table (NQ).

5. Count the number of 1/KS values in the file (NK).

6. Replace the upper left cell in the table, that contains "1/KS" with "NQ:NK" where NQ and NK are the count values from steps 3 and 4 above.

7. Save the Data tab in .csv format. Save the file named as WINNF-TS-1014-V1.2.0-App02.csv.

### **Scripts to be used and procedure to invoke these scripts**
##### FS_Database:
FS Script Parser. The AFC Administrator can run the parser manually or set the time for the daily update. The parser fetches the raw daily and weekly ULS data from the FCC website.

##### NLCD creation:
###### Step 1.
Ensure that gdal utilities are installed on your machine(currently gdal ver 3.3.3 used):
```
dnf install gdal
```

###### Step 2.
Get extents of the original file by executing command below:
```
gdalinfo -norat -nomd -noct   nlcd_2019_land_cover_l48_20210604.img
```
###### Corner Coordinates:
```
Upper Left  (-2493045.000, 3310005.000) (130d13'58.18"W, 48d42'26.63"N)
Lower Left  (-2493045.000,  177285.000) (119d47' 9.98"W, 21d44'32.31"N)
Upper Right ( 2342655.000, 3310005.000) ( 63d40'19.89"W, 49d10'37.43"N)
Lower Right ( 2342655.000,  177285.000) ( 73d35'40.55"W, 22d 4'36.23"N)
Center      (  -75195.000, 1743645.000) ( 96d52'22.83"W, 38d43' 4.71"N)
```
###### Step 3.
Define the minimum/maximum Latitude and Longitude coordinates that contain the entire region covered by the file. In order to line up with 3DEP database, we want to make sure that each of these values are integer multiple of 1-arcsec. From the above extents, we see that the min longitude is 130d13'58.18"W. This can be rounded down to integer multiple of 1-arcsec as -(130 + 15/60). Similarly, maximum values are rounded up to integer multiple of 1-arcsec. Finally, the resolution is defined as 1-arcsec which equals 1/3600 degrees. Below commands can be typed directly into a bash shell.
```
minLon=`bc <<< 'scale = 6; -(130 + 15/60)'`
maxLon=`bc <<< 'scale = 6; -(63 + 37.5/60)'`
minLat=`bc <<< 'scale = 6; (21 + 41.25/60)'`
maxLat=`bc <<< 'scale = 6; (49 + 11.25/60)'`
lonlatRes=`bc <<< 'scale = 20; (1/3600)'`

echo minLon = $minLon
echo maxLon = $maxLon
echo minLat = $minLat
echo maxLat = $maxLat
```
###### Step 4.
Define the input and output files for the conversion using commands below:
fin=nlcd_2019_land_cover_l48_20210604.img
fout=nlcd_2019_land_cover_l48_20210604_resample.tif

###### Step 5.
Use gdal utility gdalwarp to convert the file to desired output
```
gdalwarp -t_srs '+proj=longlat +datum=WGS84' -tr $lonlatRes $lonlatRes -te $minLon $minLat $maxLon $maxLat $fin $fout
```

###### Step 6:
Combine 900+ federated .int files into a single gdal .vrt file.
```
gdalbuildvrt federated_nlcd.vrt output/*.int
```

###### Step 7:
Define the input and output files for the Federated file conversion
```
fin=federated_nlcd.vrt
fout=federated_nlcd.tif
```

###### Step 8:
Run gdal utility gdalwarp to convert the federated file using the exact same file extents as for the nlcd_2019_land_cover_l48_20210604_resample.tif file:
```
gdalwarp -te $minLon $minLat $maxLon $maxLat $fin $fout
```


## **Database Usage (TO BE UPDATED)**

### **Expected location of post-processed database on the AFC server**
There are three category of databases: Dynamic, Static and ULS.
1. **Dynamic:**
* These are assets that are subject to change more frequenty than Static, either by the user interacting with the GUI (AFC Config) uploading files (AntennaPatterns), or another asset that may change in the future
* Live under /var/lib/fbrat
* Examples are: afc_config, AntennaPatterns and analysis responses
* Note that the were moved to Object storage by default

2. **Static:**
* These are the assets that are not expected to change for at least a year (and some for many years)
* Live under /mnt/nfs/rat_transfer
* Examples are: Terrain (3DEP, SRTM, Globe), Building (LiDAR, Multiband-BDesign3D), NLCD, Population Density
* Below are the database directories under /mnt/nfs/rat_transfer
  * **ULS_Database:** Fallback (static) ULS_Database in case an active ULS_Database under fbrat is missing
  * **srtm3arcsecondv003**
  * **RAS_Database**
  * **proc_lidar_2019**
  * **population**
  * **Multiband-BDesign3D**
  * **globe**
  * **3dep**
  * **nlcd**
  * **itudata**

3. **ULS (note: WIP):**
* These are the supporting files for the ULS Script Parser that download, process, and create the new ULS files
* Live under /mnt/nfs/rat_transfer/daily_uls_parse/data_files
  * **WIP:** Functionality built into API
  * Data for yesterdaysDB (used to retain FSID from day to day) and highest known FSID (to avoid collision, FSIDs are not reused currently) are stored here.


### **Configuration options to change the location of the database**
TBD

