Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Geodetic File Converter Scripts

## Table of Contents

- [Docker environment - why?](#docker)
- [*dir_md5.py* - computing MD5 hash over geodetic file directory](#dir_md5)
  - [Usage](#dir_md5_usage)
  - [Recommended command lines for various geodetic data sources](#dir_md5_command_lines)
- [*nlcd_to_wgs84.py* - converting land usage data to AFC-compatible format](#nlcd_to_wgs84)
  - [Usage](#nlcd_to_wgs84_usage)
  - [Recommended command lines for various land cover data sources](#dir_md5_command_lines)
- [*proc_gdal* - creating LiDAR files](#proc_gdal)

## Docker environment - why and how <a name="docker">

Geodetic File Converter scripts described here all may be run standalone - and examples for doing this will be provided.

However they require GDAL - both as set of libraries and set of utilities. GDAL is notoriously hard to install correctly (both on Windows and *nix), hence Dockerfile with GDAL installed is provided to run them in.

Since files operated upon are located 'in the outer world' (outside the container file system) proper use of mapping (-v in `docker run` command line) is necessary (refer Docker manuals/gurus for proper instruction). It is important to remember that both source and destination directories of mapping must be absolute paths.

## *dir_md5.py* - computing MD5 hash over geodetic file directory <a name="dir_md5"/>

Orders files by name lexicographically (in recursive case - first by subdirectory name, than by file name) and computes MD5 of their binary concatenation.

Note that file names are not part of computed MD5, they are only used for files' ordering.

Thus computed MD5 may be used as an identifier of set of geodetic files. This utility may insert computed MD5 into given JSON file.

Usually this utility works with ~0.5 GB/sec speed

`$ dir_md5.py [options] DIRS_AND_FILES`

Here `DIRS_AND_FILES` is space-separated list of filenames and directories, wildcards are handled by shell (not by script - use `--mask` option if script support is required)

Options:

|Option|Function|
|------|--------|
|--list|Print list of matching files in order MD5 could have been computed, but does not compute MD5|
|--mask **WILDCARD**|Only allow files that match given wildcard. This option may be specified several times. If none specified all files are used|
|--recursive|Recurse into subdirectories|
|--json **FILE:KEY_PATH[:[PREFIX][:SUFFIX]]**|Put computed MD5 into given JSOM file (create it is not existing) at given location, appending/prepending given prefix/suffix|
|--stats|Print size/time/rate statistics|
|--progress|Print progress information|

### Usage <a name="dir_md5_usage"/>

Some examples:
* Compute MD5 of all files in the directory `foo`:  
  `$ dir_md5.py foo`
* Same, but using script in container `geo_converters`:  
  ``$ docker run -v `pwd`/foo:/foo geo_converters dir_md5.py /foo``  
  Here outside-of-container relative directory `foo` is converted to absolute (with `` `pwd`/``) and mapped to container's `/foo` directory by means of `-v` switch.
* Compute MD5 of all `.csv` and `.tif` files in `foo` directory using standalone script:  
  `$ dir_md5.py --mask '*.tif' --mask '*.csv' foo`
* Same, but using script in container `geo_converters`:  
  ``$ docker run -v `pwd`/foo:/foo geo_converters dir_md5.py --mask '*.tif' --mask '*.csv' /foo``
* Compute MD5 of all `.csv` and `.tif` files in `foo` directory and its subdirectories, printing progress information and statistics - using standalone script:  
  `$ dir_md5.py --mask '*.tif' --mask '*.csv' --recursive --stats --progress foo`
* Same, but using script in container `geo_converters`:  
  ``$ docker run -v `pwd`/foo:/foo geo_converters dir_md5.py --mask '*.tif' --mask '*.csv' --recursive --stats --progress /foo``
* Compute MD5 of all files in directory `foo` and place result into *version* subkey of *bar* key of `../version_info.json` file, prefxed with *1.2_*:  
  `$ dir_md5.py --json ../version_info.json:bar/version:1.2_ foo`
* Same, but using script in container `geo_converters`:  
  ``$ docker run -v `pwd`/foo:/foo -v `pwd`/..:/dest geo_converters dir_md5.py --json ../dest/version_info.json:bar/version:1.2_ /foo``  
  Note the mappings - first for files' directory (to `foo`), second for directory of target file (to `/dest`)
* List files in `foo` directory and its subdirectories whose MD5 will be computed - in order it will be computed and with total size of these files:  
  `$ dir_md5.py --list --stats foo`
* Same, but using script in container `geo_converters`:  
  ``$ docker run -v `pwd`/foo:/foo geo_converters dir_md5.py --list --stats /foo``  


### Recommended command lines for various geodetic data sources <a name="dir_md5_command_lines"/>

All examples will be given in standalone and container form. Variable parts (directories, container names) shown in <CAPS_IN_BRACKETS>: 

* NLCD
  * Standalone
    * List NLCD files  
      `$ dir_md5.py --list --stats --mask '*.tif' <NLCD_DIR>`
    * Compute MD5  
      `$ dir_md5.py --mask '*.tif' <NLCD_DIR>`
  * Containerized
    * List NLCD files  
      ``$ docker run -v `realpath <NLCD_DIR>`:/nlcd <CONTAINER> \ ``  
      `dir_md5.py --list --stats --mask '*.tif' /nlcd`
    * Compute MD5  
      ``$ docker run -v `realpath <NLCD_DIR>`:/nlcd <CONTAINER> \ ``  
      `dir_md5.py --mask '*.tif' /nlcd`
* 3DEP
  * Standalone
    * List 3DEP files  
      `$ dir_md5.py --list --stats --mask '*.tif' <3DEP_DIR>`
    * Compute MD5  
      `$ dir_md5.py --mask '*.tif' <3DEP_DIR>`
  * Containerized
    * List 3DEP files  
      ``$ docker run -v `realpath <3DEP_DIR>`:/3dep <CONTAINER> \ ``  
      `dir_md5.py --list --stats --mask '*.tif' /3dep`
    * Compute MD5  
      ``$ docker run -v `realpath <3DEP_DIR>`:/3dep <CONTAINER> \ ``  
      `dir_md5.py --mask '*.tif' /3dep`
* LiDAR
  * Standalone
    * List LiDAR-related files  
      `$ dir_md5.py --list --stats --recursive --mask '*.csv' --mask '*.tif' <LIDAR_DIR>`
    * Compute MD5  
      `$ dir_md5.py --recursive --mask '*.csv' --mask '*.tif' <LIDAR_DIR>`
  * Containerized
    * List LiDAR-related files  
      ``$ docker run -v `realpath <LIDAR_DIR>`:/lidar <CONTAINER> \ ``  
      `dir_md5.py --list --stats --recursive --mask '*.csv' --mask '*.tif' /lidar`
    * Compute MD5  
      ``$ docker run -v `realpath <LIDAR_DIR>`:/lidar <CONTAINER> \ ``  
      `dir_md5.py --recursive --mask '*.csv' --mask '*.tif' /lidar`
* SRTM
  * Standalone
    * List SRTM files  
      `$ dir_md5.py --list --stats --mask '*.hgt' <SRTM_DIR>`
    * Compute MD5  
      `$ dir_md5.py --mask '*.hgt' <SRTM_DIR>`
  * Containerized
    * List SRTM files  
      ``$ docker run -v `realpath <SRTM_DIR>`:/srtm <CONTAINER> \ ``  
      `dir_md5.py --list --stats --mask '*.hgt' /srtm`
    * Compute MD5  
      ``$ docker run -v `realpath <SRTM_DIR>`:/srtm <CONTAINER> \ ``  
      `dir_md5.py --mask '*.hgt' /srtm`
* Globe
  * Standalone
    * List Globe files  
      `$ dir_md5.py --list --stats --mask '*.bil' <GLOBE_DIR>`
    * Compute MD5  
      `$ dir_md5.py --mask '*.bil' <GLOBE_DIR>`
  * Containerized
    * List Globe files  
      ``$ docker run -v `realpath <GLOBE_DIR>`:/globe <CONTAINER> \ ``  
      `dir_md5.py --list --stats --mask '*.bil' /globe`
    * Compute MD5  
      ``$ docker run -v `realpath <GLOBE_DIR>`:/globe <CONTAINER> \ ``  
      `dir_md5.py --mask '*.bil' /globe`

## *nlcd_to_wgs84.py* - converting land usage data to AFC-compatible format <a name="nlcd_to_wgs84"/>

AFC Engine employs land usage data (essentially - urban/rural classification) in form of NLCD files. NLCD files used by AFC are in WGS84 geodetic (latitude/longitude) coordinate system.

Source data for these files released in different forms:
* NLCD files as such - they have not geodetic, but map projection coordinate system.

* NOAA files - contain (as of time of this writing) more recent and detailed land usage data for Hawaii, Puerto Rico, Virgin Islands. These files also have not geodetic, but map projection coordinate system, also they have different land usage codes.

`nlcd_to_wgs84.py` script converts NLCD data files from the source format to one, used  by AFC Engine.

`nlcd_to_wgs84.py [options] SOURCE_FILE DEST_FILE`

Options:

|Option|Function|
|------|--------|
|--pixel_size **DEGREES**|Pixel size of resulting file in degrees|
|--pixels_per_degree **NUMBER**|Pixel size of resulting file in form of number of pixels per degree. As of time of this writing *3600* recommended for both NLCD and NOAA source data. If pixel size not specified in any way, `gdalwarp` utility will decide - this is not recommended|
|--translate **ENCODING**|Translate land usage codes from one encoding to other. So far the only supported encoding is *noaa*. Translation process is admittedly slow|
|--format **GDAL_FORMAT**|Format of output file (if can't be derived from file's extension) - see https://gdal.org/drivers/raster/index.html Usually this switch is unnecessary, as output file extension is *.tif*|
|--format_param **NAME=VALUE**|Output format option. *COMPRESS=PACKBITS* is recommended for TIFF output, as it shrinks size by a factor of 9|
|--temp_dir **TEMPDIR**|This script requires a lot of space for intermediate files, that may be unavailable when it runs inside the container. This switch allows to use specified (presumably - outside of container) directory for temporary files|
|--overwrite|Overwrite output files if already exists|

### Usage <a name="dnlcd_to_wgs84_usage"/>

Some examples:

* Convert NLCD file `foo/nlcd_2019_land_cover_l48_20210604.img` (CONUS land usage) to AFC Engine compatible `bar/nlcd_2019_land_cover_l48_20210604_resampled.tif`:  
  `$ nlcd_to_wgs84.py --pixels_per_degree 3600 --format_param COMPRESS=PACKBITS \ `  
  `foo/nlcd_2019_land_cover_l48_20210604.img \ `  
  `bar/nlcd_2019_land_cover_l48_20210604_resampled.tif`
* Same, but running script from container named `geo_converters`:
  ``docker run -v `realpath foo`:/foo -v `realpath bar`:/bar nlcd_to_wgs84.py \ ``  
  `--pixels_per_degree 3600 --format_param COMPRESS=PACKBITS --temp_dir /bar \ `  
  `/foo/nlcd_2019_land_cover_l48_20210604.img \ `  
  `/bar/nlcd_2019_land_cover_l48_20210604_resampled.tif`  
  Note the use of target directory (`/bar` in container file system) as temporary files' directory. Also note that files in command line use mapped directory names.
* Convert NOAA file `foo/hi_maui_2005_ccap_hr_land_cover.img` (Hawaii Maui island land usage) to AFC Engine compatible `/bar/hi_maui_2005_ccap_hr_land_cover_resampled.tif`  
  `$ nlcd_to_wgs84.py --pixels_per_degree 3600 --format_param COMPRESS=PACKBITS \ `
  `--translate noaa \ `  
  `foo/hi_maui_2005_ccap_hr_land_cover.img \ `  
  `bar/hi_maui_2005_ccap_hr_land_cover_resampled.tif`  
  Note the use of `--translate noaa` for NOAA source file
* Same, but running script from container named `geo_converters`:  
  ``docker run -v `realpath foo`:/foo -v `realpath bar`:/bar nlcd_to_wgs84.py \ ``  
  `--pixels_per_degree 3600 --format_param COMPRESS=PACKBITS --temp_dir /bar \ `  
  `--translate noaa \ `  
  `/foo/hi_maui_2005_ccap_hr_land_cover.img \ `  
  `/bar/hi_maui_2005_ccap_hr_land_cover_resampled.tif`  

## *proc_gdal* - creating LiDAR files <a name="proc_gdal"/>
This directory contains undigested, but very important files that convert LiDAR files from source format (as downloaded from https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/ ) to form, compatible with AFC Engine (two-band geodetic raster files and their index .csv files)