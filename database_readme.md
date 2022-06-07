# Database Readme

## **Database Description**

### **Details of Databases**
#### **ULS_Database:**
 contains parameters defining the FS links for interference analysis.
These are:
* FS Tx/Rx Lat/Long and Height above ground level,
* FS Rx Gain,
* FS Start/End Frequencies and Bandwidth,
* FS passive repeater Lat/Long and Height above ground level

#### **srtm3arcsecondv003:**
terrain height from SRTM database with 3-arcsec resolution (=90 meters at the equator)

#### **RAS_Database:**
contains parameters defining exclusion zone(s) around each RAS antenna that needs to be protected

#### **proc_lidar_2019:**
contains json files that allow showing boundaries of RAS exclusion zones and LiDAR in GUI.
* RAS_ExclusionZone.json
* LiDAR_Bounds.json.gz

This also contains all lidar tiles where each city has a subdirectory with tiles with a .csv that isn’t under the city subdirectory. The lidar zip file contains all of this.

* **population:** contains the Gridded Population of the World (GPW), v4.11, population density database along with conus/canada.kml that defines the analysis boundary.
**this is no longer used.** Previously this was used to determine RLAN morphology. This is being replaced by the NLCD database.
* **Multiband-BDesign3D:** contains building database over Manhattan.
* **globe:** contains NOAA GLOBE (1km resolution) terrain database.
* **3dep:** The 1_arcsec subdirectory (one currently used) contains 1arcsec (=30m) 3DEP terrain database files
* **nlcd:** contains nlcd_2019_land_cover_I48_20210604_resample.zip (referred to as "Production NLCD" in AFC Config UI) and federated_nlcd.zip (referred to as "WFA Test NLCD" in AFC Config UI) files. This is used to determine RLAN/FS morphology (i.e. Urban, Suburban or Rural) to pick the appropriate path/clutter loss model. In addition, it is used to determine the appropriate P.452 Rural clutter category.
* **itudata:** contains two ITU maps that are used by the ITM path loss model. 1) Radio Climate map (TropoClim.txt) and 2) Surface Refractivity map (N050.txt)

### **Location or procedure to download/acquire these databases**
* **ULS_Database:** Created using ULS Script Parser from ULS raw data on FCC website (see details in the ULS Script documentation)
* **srtm3arcsecondv003:** https://www2.jpl.nasa.gov/srtm/
* **RAS_Database:** File created based on the Exclusion zones listed in US385
* **proc_lidar_2019:** raw data obtained from https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/
* **population:** https://sedac.ciesin.columbia.edu/data/set/gpw-v4-population-density-rev11
* **Multiband-BDesign3D:** this was purchased https://www.b-design3d.com/
* **globe:** https://ngdc.noaa.gov/mgg/topo/globe.html
* **3dep:** https://data.globalchange.gov/dataset/usgs-national-elevation-dataset-ned-1-arc-second
* **nlcd:** original file nlcd_2019_land_cover_I48_20210604 was downloaded from [link](https://www.mrlc.gov/data?f%5B0%5D=category%3Aland%20cover) (download NLCD 2019 Land Cover (CONUS)). Usig gdal utilties this file was translated to nlcd_2019_land_cover_I48_20210604_resample.zip so that the 1-arcsec tiles matchup with 1-arcsec 3DEP tiles. The federated_nlcd.zip file was obtained by using other gdal utilities to convert federated's many files to one file covering CONUS.
* **itudata:** Radio Climate map from ITU-R Rec, P.617-3 (https://www.itu.int/rec/R-REC-P.617-3-201309-S/en) and Surface Refractivity map from ITU-R Rec, P.452-17 (https://www.itu.int/rec/R-REC-P.452-17-202109-I/en)

### **Licences and Source Citations**
#### **NLCD**
Public domain

References:

Dewitz, J., and U.S. Geological Survey, 2021, National Land Cover Database (NLCD) 2019 Products (ver. 2.0, June 2021): U.S. Geological Survey data release, https://doi.org/10.5066/P9KZCM54

Wickham, J., Stehman, S.V., Sorenson, D.G., Gass, L., and Dewitz, J.A., 2021, Thematic accuracy assessment of the NLCD 2016 land cover for the conterminous United States: Remote Sensing of Environment, v. 257, art. no. 112357, at https://doi.org/10.1016/j.rse.2021.112357 

Homer, Collin G., Dewitz, Jon A., Jin, Suming, Xian, George, Costello, C., Danielson, Patrick, Gass, L., Funk, M., Wickham, J., Stehman, S., Auch, Roger F., Riitters, K. H., Conterminous United States land cover change patterns 2001–2016 from the 2016 National Land Cover Database: ISPRS Journal of Photogrammetry and Remote Sensing, v. 162, p. 184–199, at https://doi.org/10.1016/j.isprsjprs.2020.02.019

Jin, Suming, Homer, Collin, Yang, Limin, Danielson, Patrick, Dewitz, Jon, Li, Congcong, Zhu, Z., Xian, George, Howard, Danny, Overall methodology design for the United States National Land Cover Database 2016 products: Remote Sensing, v. 11, no. 24, at https://doi.org/10.3390/rs11242971

Yang, L., Jin, S., Danielson, P., Homer, C., Gass, L., Case, A., Costello, C., Dewitz, J., Fry, J., Funk, M., Grannemann, B., Rigge, M. and G. Xian. 2018. A New Generation of the United States National Land Cover Database: Requirements, Research Priorities, Design, and Implementation Strategies, ISPRS Journal of Photogrammetry and Remote Sensing, 146, pp.108-123.

#### **proc_lidar_2019**
Available for public use with no restrictions

Disclaimer and quality information is at https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/00_NGA%20133%20US%20Cities%20Data%20Disclaimer%20and%20Explanation%20Readme.pdf 

#### **3DEP**
Public domain

Data available from U.S. Geological Survey, National Geospatial Program.

#### **Globe**
Public domain

NOAA National Geophysical Data Center. 1999: Global Land One-kilometer Base Elevation (GLOBE) v.1. NOAA National Centers for Environmental Information. https://doi.org/10.7289/V52R3PMS. Accessed TBD

#### **population**
Creative Commons Attribution 4.0 International (CC BY) License (https://creativecommons.org/licenses/by/4.0)

Center for International Earth Science Information Network - CIESIN - Columbia University. 2018. Gridded Population of the World, Version 4 (GPWv4): Population Density, Revision 11. Palisades, New York: NASA Socioeconomic Data and Applications Center (SEDAC). https://doi.org/10.7927/H49C6VHW 

## **Database Update**

### **Expected update frequency of each database file**
* **ULS_Database:** daily (per FCC requirement)
* **srtm3arcsecondv003:** every few years (whenever a new database is available)
* **RAS_Database:** whenever there is a change to RAS exclusion zones (expect rarely)
* **proc_lidar_2019:** every few years (whenever a new database is available)
* **population:** every few years (whenever a new database is available)
* **Multiband-BDesign3D:** no change (unless a newer building database for Manhattan is needed)
* **globe:** every few years (whenever a new database is available)
* **3dep:** every few years (whenever a new database is available)
* **nlcd:** every few years (whenever a new database is available)
* **itudata:** these haven't been updated for a long time but can be updated if new maps are generated.

### **Database update procedure**
* **ULS_Database:** ULS Script Parser automatically updates this daily (see next section)
* **srtm3arcsecondv003:** download database and put in the proper directory
* **RAS_Database:** update the .csv file when there is an update
* **proc_lidar_2019:** download lidar database and post-process
* **population:** download database and put in the proper directory
* **globe:** download database and put in the proper directory
* **3dep:** download database and put in the proper directory
* **nlcd:** download database, run proper gdal utilties to orient the tiles matching 3DEP 1-arcsec tiles and put in the proper directory

## **Database Processing/Format**
### **Processing done (in any) on the original database to convert it to a format usable by afc-engine**
* **ULS_Database:** generated by the ULS Script Parser.
* **RAS_Database:** generated from the parameters defining RAS exclusion zones.
* **LiDAR_Database:** generated from significant post-processing:
    For each city:
   * (1) Identify bare earth and building files available.
   * (2) Identify pairs of files where a pair consists of a bare earth and building polygon file that cover the same region.
   * (3) Convert bare earth into tif raster file, and convert building polygon file into tif raster file where both files are on same lon/lat grid.
   * (4) Combine bare earth and building tif files into a single tif file with bare earth on Band 1 and building height on band 2. Both Band 1 and Band 2 values are AMSL.
   * (5) Under target directory create directory for each city, under each city create dir structure containing combined tif files.
   * (6) For each city create info file listing tif files and min/max lon/lat for each file.
* **srtm3arcsecondv003:** the two SRTM tiles over Manhattan are removed since they erroneously contain building height

### **Scripts to be used and procedure to invoke these scripts**
##### ULS_Database:
ULS Script Parser. The AFC Administrator can run the parser manually or set the time for the daily update. The parser fetches the raw daily and weekly ULS data from the FCC website.

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


## **Database Usage**

### **Expected location of post-processed database on the AFC server**
There are three category of databases: Dynamic, Static and ULS.
1. **Dynamic:**
* These are assets that are subject to change more frequenty than Static, either by the user interacting with the GUI (AFC Config) uploading files (AntennaPatterns, ULS_Database), or another asset that may change in the future
* Live under /var/lib/fbrat
* Examples are: ULS_Database, afc_config, AntennaPatterns, RAS_Database, and analysis responses
* Note that the RAS_Database technically lives as a static asset but the code uses a symlink at /var/lib/fbrat/RAS_Database

2. **Static:**
* These are the assets that are not expected to change for at least a year (and some for many years)
* Live under /usr/share/fbrat/rat_transfer
* Examples are: Terrain (3DEP, SRTM, Globe), Building (LiDAR, Multiband-BDesign3D), NLCD, Population Density
* Below are the database directories under /usr/share/fbrat/rat_transfer
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
* Live under /var/lib/fbrat/daily_uls_parse/data_files
  * **WIP:** Functionality built into API
  * Data for yesterdaysDB (used to retain FSID from day to day) and highest known FSID (to avoid collision, FSIDs are not reused currently) are stored here.


### **Configuration options to change the location of the database**
TBD

