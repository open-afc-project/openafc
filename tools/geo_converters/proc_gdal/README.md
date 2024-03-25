#   Lidar processing.

## Download Data

Lidar data can be downloaded from:
https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/

## Processing

The attached script, `create_lidar_database_threaded.pl`  was used to process the downloaded lidar data to the format used by the afc-engine.  This script is written in perl and makes use of the following standard gdal programs:

- `gdalbuildvrt`
- `gdalinfo`
- `gdal_rasterize`
- `gdal_translate`
- `gdalwarp`
- `ogr2ogr`
- `ogrinfo`
- `ogrmerge.py`

The script also makes use of a custom C++ program:

- `proc_gdal`

On line 68 of `create_lidar_database_threaded.pl`, the following variables need to be set:
    `$name`       = name of dataset, used for some intermediate file names, not really important, I used "NGA_US_Cities".
    `$sourceDir`  = path to directory that contains downloaded lidar data.
    `$destDir`    = path to directory where processed lidar files will be placed.
    `$tmpDir`     = temporary directory where numerous intermediate files are generated.
    `$numThread`  = this perl script is threaded, number of threads to run.

The script has several command line options.  Running `create_lidar_database_threaded.pl -h` will list the command line options and gives a short description of each option.

The downloaded lidar data is organized into a separate directory for each city.  The script takes a list of cities and processes the lidar data for those cities.  Line 114 defines the perl list `@cityList`, where each item in the list is the full path to the directory of a single city.  Optionally, each city can have several parameters that control the processing for that city.  These parameters are appended to the full path directory separated by colons ':'.

Each city has an independent dataset.  The function `processCity()` processes the lidar data for a single city.  To parallelize this script, each city is processed in its own thread.  The core processing of this script is in the function `processCity()`.  Note that for each city, the file format, tile sizes, gdal coordinate system used, and several other parameters are not the same for each city.  Also, the representation of "invalid values" in each dataset is not the same for all cities.  These are challenges in developing this script, and make the implementation somewhat less than elegant.

The function `processCity()` begins by hierarchically traversing the directory for the city looking for subdirectories that contain the following directories:

- BE : subdirectory containing bare-earth raster data
- VEC/BLD3 : subdirectory containing 3D building data
- VEC/BLD2 : subdirectory containing 32 building data

The list `@subdirlist` is created and contains a list of subdirectories that contain both bare-earth and building data.  These subdirectories are then processed one at a time.  

For each subdirectory, the list `@srcBElist` is created and is a list of the bare earth raster files under the subdirectory.  These files have a file extension of `.img` or `.tif`.  Each of these files is "fixed" by removing invalid values and properly setting nodata values.  Also, an image file showing coverage is created.  This is done using the function fixRaster which executes the program `proc_gdal`.  Next, the extents of each file (minLon, maxLon, minLat, maxLat) are determined using `gdalinfo`.  

The list `@srcBldglist` is created and is a list of 3D vector building data files under the subdirectory.  These files have a file extension of `.shp`.  The extents of each file are determined using `ogrinfo`.  Each file is also checked to contain a field recognized as building height.  Recognized field names are: `topelev_m`, `TOPELEV_M`, `TopElev3d`, `TopElev3D`, `TOPELEV3D`.  Each of these files is "fixed" by changing the name of the building height field to `topelev_m` and by replacing MultiPolygon with polygons.  Also, an image file showing coverage is created.   This is done using the function `fixVector()` which executes the program `proc_gdal`.

The list `@srcBldg2list` is created and is a list of 2D vector building data files under the subdirectory.  These files have a file extension of `.shp`.  The extents of each file are determined using `ogrinfo`.  Each file is also checked to contain a field recognized as building height.  Recognized field names are: `maxht_m`, `MAXHT_M`, `HGT2d`.  Each of these files is "fixed" by changing the name of the building height field to `topelev_m` and by replacing MultiPolygon with polygons.  Also, an image file showing coverage is created.   This is done using the function fixVector which executes the program `proc_gdal`.

The extents of all the bare earth files in the subdirectory are combined to find the total extents of all bare earth data in the subdirectory.  This is also done for the 3D and 2D building data.  KML is generated that shows where there is source lidar data.  Bare earth, 3D building, and 2D building data are all shown in the KML.

The script then iterates through bare-earth files and building files looking for a single bare earth file that covers the same region as a single building file.  This is done by comparing the extents of the file.  The function `isMatch()` compares extents and returns 1 if the overlap area is $\geq$ 80% of the total combined area.  Next the scripts looks for matches by grouping multiple bare-earth files and multiple building files. 

For each match, the gdal program `gdalbuildvrt` is used to combine the bare-earth files into a single bare-earth file.  Next, `gdalwarp` is used to set the gdal coordinate system used to be a WGS84, and a sampling resolution of $10^{-5}$ degrees which corresponds to 1.11 meters.  Next the gdal program `ogr2ogr` is run on each building file in the match to remove all fields except the building height field.  Then `ogrmerge.py` is run to combine these building files into a single building file.  Then `ogr2ogr` is run to convert the building file into an sqlite format using the WGS84 coordinate system.  Next, `gdal_rasterize` is run to convert the building polygon file into raster data having the same extents and resolution as the bare-earth data.  The next step is to then use `gdalbuildvrt` to combine the overlapping bare-earth and building raster files into a single file with 2 layers.  This 2 layer file has bare earth data on layer 1 and building data on layer 2.  Finally, this this multilayer raster file is split into rectangular pieces where each dimension is $\leq$ `$gpMaxPixel`.  The files are split using `gdal_translate` and the resulting files are saved in a multilayer raster `.tif` format.  These are the final processed files that are used by the `afc-engine`.
