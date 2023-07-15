Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Geodetic File Converter Scripts

## Table of Contents

- [General considerations](#general_considerations)
  - [Running scripts: standalone or docker (dependencies)?](#docker)
  - [GDAL version selection](#gdal)
  - [Performance (`--threads`, `--nice`, conversion order)](#performance)
  - [Ctrl-C](#ctrlc)
  - [Restarting (vs `--overwrite`)](#restarting)
  - [Pixel size (`--pixel_size`, `--pixels_per_degree`, `--round_pixels_to_degree`)](#pixel_size_alignment)
  - [Cropping/realigning (`--top`, `--bottom`, `--left`, `--right`, `--round_pixels_to_degree`)](#crop_realign)
  - [Geoids (`--src_geoid`, `--dst_geoid`, order of operations)](#geoids)
  - [Resampling (`--resampling`)](#resampling)
  - [GeoTiff format options (`--format_param`)](#geotiff_format)
  - [PNG data representation (`--scale`, `--no_data`, `--data_type`)](#png_representation)
- [Scripts](#scripts)
  - [*dir_md5.py* - computing MD5 hash over geodetic file directory](#dir_md5)
  - [*nlcd_wgs84.py* - converting land usage data to AFC-compatible format](#nlcd_wgs84)
  - [*lidar_merge.py* - flattens lidar files](#lidar_merge)
  - [*to_wgs84.py* - change coordinate system](#to_wgs84)
  - [*to_png.py* - converts files to PNG format](#to_png)
  - [*tiler.py* - cuts source files to 1x1 degree tiles](#tiler)
- [Conversion routines](#conversion_routines)
  - [Geoids](#geoid_rouitines)
    - [USA geoids](#usa_geoids_routines)
    - [Canada geoid](#canada_geoid_procedure)
    - [World geoids](#world_geoids_routines)
  - [USA/Canada/Mexico 3DEP terrain model](#3dep_routine)
  - [Coarse SRTM/GLOBE world terrain](#coarse_terrain_routine)
  - [High resolution terrain data (aka LiDAR)](#lidar_routine)
  - [Canada CDSM surface model](#cdsm_routine)
  - [Land usage files](#land_usage_routines)
    - [NLCD land usage files](#nlcd_routine)
    - [NOOA land usage files](#noaa_routine)
    - [Canada land usage files](#canada_land_usage_routine)
- [*proc_gdal* - creating LiDAR files](#proc_gdal)

## General considerations <a name="general_considerations"/>

This chapter describes peculiarities, common to all scripts.

### Running scripts: standalone vs Docker (dependencies) <a name="docker"/>

Geodetic File Converter scripts described here all may be run standalone - and examples for doing this will be provided.

However some of them require GDAL - sometimes GDAL utilities, sometimes Python GDAL libraries. GDAL utilities are  hard to install. GDAL Python bindings even harder to install. So, while running standalone is more convenient, Docker may need to be resorted to (Dockerfile is provided).

Here are scripts' dependencies that require separate installation:

|Script|GDAL utilities?|GDAL Python bindings?|Other dependencies|
|------|---------------|----------------------------------------|
|dir_md5.py|No|No|jsonschema(optional)|
|lidar_merge.py|Yes|No||
|nlcd_wgs84.py|Yes|For non-NLCD sources|pyyaml|
|tiler.py|Yes|No||
|to_png.py|Yes|No||
|to_wgs84.py|Yes|No||

Since files operated upon are located 'in the outer world' (outside the container's file system) proper use of mapping (`-v`, `--user` and `--group_add` in `docker run` command line), absolute paths etc. is necessary (refer Docker manuals/gurus for proper instruction).

`afc_tool.py` that is out of the scope of this document (and even may not will be a part of this distribution) may be used to simplify running scripts from docker. Compare (note the use of `^` before all 'out-of-container' file names):
```
afc_tool.py docker_run geo_converters to_wgs84.py --resampling cubic ^a.tif ^b.tif
```
and equivalent full form:  
```
docker run --rm -it --user 16256:16256 --group-add 16256 --group-add 20 --group-add 970 \
  --group-add 1426 --group-add 3167 --group-add 3179 --group-add 3186 --group-add 3199 \
  --group-add 55669 --group-add 61637 -v /home/fs936724:/files \
  geo_converters --resampling cubic to_wgs84.py /files/a.tif /files/b.tif
```

### GDAL version selection <a name="gdal"/>

GDAL (set of libraries and utilities that do the job in almost all scripts here) is constantly improving - both in capabilities and in reliability. And GDAL is an 'industry standard' without any viable alternatives.

To put it another way, GDAL is still painfully slow and has a lot of bugs. So it makes sense to use as late (stable) version as possible. As of time of this writing the latest version is 3.6.3 and it is much better than, say, 3.4.1.

GDAL version may be checked with `gdalinfo --version`

### Performance (`--threads`, `--nice`, conversion order) <a name="performance"/>

Geodetic data conversion (performed mainly by means of `gdal_transform` and `gdalwarp` utilities) is a painfully slow process, so all scripts presented here support parallelization. It is controlled by `--threads` parameter that has following forms:

|Form|Action|
|----|------|
|| Default (no `--threads` parameter) - use all CPUs|
|--threads **N**|Use N CPUs|
|--threads -**N**|Use all CPUs but N. E.g. if there are 8 CPUs `--threads -2` will take 6 (8-2) CPUs|
|--threads **N**%| Use N% of CPUs. E.g. if there are 8 CPUs `--threads 25%` will use 2 CPUs|

Note that `gdal_transform` and `gdalwarp` on large files use a lot of memory, so using too much CPUs will cause thrashing (excessive swapping). So it may make sense to limit the number of CPUs.

All scripts have `--nice` parameter to lower its priority (and thus do not impede user-interacting processes). On Windows it only works if 'psutil' Python module is installed. Not sure it will help against thrashing though.

`gdalwarp` (used by `to_wgs84.py` and `nlcd_wgs84.py`) works especially slow on large (e.g. 20GB) files. Hence LiDAR conversion process first slices source data to tiles then runs `to_wgs84.py` to apply geoids). This is not always possible though, so `nlcd_wgs84.py` works painfully slow (and using single CPU) on CONUS and Canada files and there is nothing that can be done about it (except for waiting for future GDAL releases).


### Ctrl-C <a name="ctrlc"/>

Python multithreading has an eternal bug of Ctrl-C mishandling. There is a hack that fixes it on *nix platforms (used in all scripts presented here), but unfortunately there is no remedy for Windows.

Hence in *nix (including WSL, PuTTY, etc.) one can terminate scripts presented here with Ctrl-C, whereas on Windows one can only stop script by, say, closing terminal window.

### Restarting (vs `--overwrite`) <a name="restarting"/>

`tiler.py` always produces and `lidar_merge.py`, `to_wgs84.py`, `to_png.py` may produce multiple output files. If some of these files already exist they just skip them. This makes the conversion process restartable - when started next time process will pick from where it left off (excluding half-done files - they'll be redone).

If this behavior is undesirable (e.g. if conversion parameter or source data has changed), there is an `--overwrite` switch that overwrite already existing files.

### Pixel size (`--pixel_size`, `--pixels_per_degree`, `--round_pixels_to_degree`) <a name="pixel_size_alignment"/>

Raster geospatial files (those that contain heights, land usage, population density, etc.) are, essentially image files (usually TIFF or PNG) with some metadata. Geospatial data is encoded as pixel values (data stored in pixel may be signed/unsigned int8/16/32/64, float 32/64, complex int/float of those sizes). Pixel size (distance between adjacent pixels) and alignment on degree mesh might be essential.

By default GDAL keeps pixel size and alignment during conversions - and it works well in many cases. In some cases however (e.f. when converting from MAP data - as in case of land usage source files or when pixels are slightly off their intended positions) pixel size and alignment should be specified/corrected explicitly (this is named resampling - more on it later). Here are command line parameters for setting/changing pixel size:

|Parameter|Meaning|
|---------|-------|
|--pixel_size **SIZE**|Pixel size in degrees. E.g. `--pixel_size 0.00001` means 100000 pixels per degree|
|--pixels_per_degree **N**|Number of pixels per degree. E.g. `--pixels_per_degree 3600` means 3600 pixels per degree (1 pixel per arcsecond)|
|--round_pixels_to_degree|Take existing pixel sizes and round it to nearest whole number of pixels per degree. E.g. if pixel size is 0.008333333333000, it will be rounded to 0.008333333333333|

### Cropping/realigning (`--top`, `--bottom`, `--left`, `--right`, `--round_pixels_to_degree`) <a name="crop_realign"/>

As of time of this writing some geospatial data (specifically - land usage) is represented by a large files that cover some region (e.g. CONUS, Canada, Alaska, Hawaii, etc.). Problem may occur if such files intersect (as CONUS and uncropped Alaska do, despite there is no CONUS data on Alaska file).

While large geospatial files are being gradually phased out (in favor of 1X1 degree tile files), as an intermediate solution it is possible to crop source file(s) to avoid intersection. This may be made with `--top MAXLAT`, `--bottom MINLAT`, `--left MINLON`, `--right MAXLON` parameters that set a crop values (MINLAT/MAXLAT are signed, north-positive degrees, MINLON/MAXLON are signed east-positive degrees).

It is not necessary to set all of them - some boundaries may be left as is.

Also pixels in file may be not aligned on degree mesh. While it can't be fixed directly, there is a `--round_pixels_to_degree` that extends boundary to next whole degree. Resulting pixels became degree-aligned on left/top boundaries, alignment on other boundaries may be achieved by setting pixel size (see chapter on pixel size parameters above).

### Geoids (`--src_geoid`, `--dst_geoid`, order of operations) <a name="geoids"/>

Geoid is a surface, perpendicular to vertical (lead line). Calm water surface is an example of geoid, albeit geoids are, usually, present more interests over the land.

Clearly there are infinitely many geoids, but there is just one geoid that goes through a particular point in space. For USA and Canada such point in space is Father Point, in Rimouski, Canada, so their geoids are compatible. Yet Earth shape changes, surveying techniques improve  and so each country has many releases of its geoid models (GEOID96, GEOID99, ... GEOID12B, GEOID18 for USA, HTv2 1997, 2002, 2010).

Geoids are GDAL files (usually with .gtx extension, albeit .bin and .byn extensions also happen). Contiguous countries (like Canada) usually have single-file geoid models covers it all, noncontiguous countries (like USA) have multifile geoid models that cover its various parts.

AFC Engine expects all heights (in particular - terrain and building heights) to be expressed relative to WGS84 ellipsoid. There is a perfect sense in it - it makes computation simple and closed (not depending on any external parameters).

All terrain data have contain geoidal heights (heights relative to geoid). There is a perfect sense in it - surface of constant geoidal height is horizontal (perpendicular to the lead). Unfortunately geoidal heights are very inconvenient for making computations.

Even though terrain heights are geoidal, AFC Engine treats them as ellipsoidal, that is delusional.

`to_wgs84.py` script allows to convert heights from geoidal to WGS84 by specifying geoid model for source data with `--src_geoid` parameter. Since geoid model may be multifile, its name may be filename of mask, e.g. `--src_geoid 'g2018?0.gtx'`. Note the quotes around name - they prevent shell to glob it.

If many files are converted, different files may need to have different geoid models, so `--src_geoid` switch may be used several times e.g.:

```
--src_geoid 'g2018?0.gtx' --src_geoid 'g2012b?0.gtx' --src_geoid HT2_2010v70.gtx
```
Here preferable in USA is GEOID18, wherever it does not cover (e.g. Hawaii) - GEOID12B, in Canada - HTv2 of 2010).

Also there is a `--dst_geoid` that makes *resulting* heights to be relative to given geoid. This parameter is for creation of delusional output data compatible with delusional dataset, containing heights relative to given geoid. This might be useful for compatibility cross-testing, but better not to be used for production purposes.

Important performance observation. If `to_wgs84.py` source files are not in WGS84 geodetic (latitude/longitude) coordinate system - it is better to **first convert them into WGS84 geodetic coordinate system (with `to_wgs84.py`) and then, with a separate run of `to_wgs84.py ... --src_geoid ...` convert heights to WGS84**. Doing both conversion in one swoop involves geoid backprojection which is either impossible (which manifests itself with funny error messages) or extremely (hundreds of times) slower.

### Resampling (`--resampling`) <a name="resampling"/>

Changing pixel sizes, cropping, applying geoids, changing of coordinate system changes pixel values in geospatial file so that resulting values are somehow computed from source values of 'surrounding pixels'.

This process is named **resampling** and it is may be performed per many various methods (`nearest`, `bilinear`, `cubic`, `cubicspline`, `lanczos`, `average`, `rms`, `mode`, `max`, `min`, `med`, `Q1`, `Q3`, `sum` - whatever it all means).

Resampling to use may be specified by `--resampling METHOD` parameter.

By default for byte source or destination data (e.g. land usage) `nearest` method is used, for all other cases `cubic` method is used.

### GeoTiff format options (`--format_param`) <a name="geotiff_format"/>

Scripts that generate geospatial files (i.e. all but `dir_md5.py`) have `--format_param` option to specify nonstandard file generation parameters. Several such parameters may be specified.

GeoTiff files (files with .tif, .hgt, .bil extensions) are big. Sometimes too big for default options. Here are some useful options for GeoTiff file generation:

|Option|Meaning|
|------|-------|
|--format_param COMPRESS=PACKBITS|Makes land usage files 10 time smaller without performance penalty. Also useful on terrain files with lots of NoData or sea|
|--format_param COMPRESS=LZW|Works well on terrain files (compress ratio 2-3 times), but slows things down. Might be a good tradeoff when working with behemoth LiDAR files on tight disk space|
|--format_param COMPRESS=ZSTD|Works better and faster than LZW. Reportedly not universally available|
|--format_param BIGTIFF=YES|Sometimes conversion fails if resulting file is too big (in this case error message appeals to use BIGTIFF=YES), this parameter enables BigTiff mode|
|--format_param BIGTIFF=IF_NEEDED|Enabled BigTiff mode if file expected to require it. Doesn't work well with compression|

### PNG data representation (`--scale`, `--no_data`, `--data_type`) <a name="png_representation"/>

PNG pixel may only contain 8 or 16 bit unsigned integers. Former used for e.g. land usage codes, latter - for terrain heights. Source terrain files however contain integer or floating point heights in meters. Latter need to be somehow mapped into former.

Also terrain files contain a special value of 'NoData` (as a rule - somewhere far from range of valid values), that also need to be properly mapped to PNG data value range.

PNG-generation scripts (`to_png.py`, `tiler.py`) provide the following options to control the process:

|Option|Meaning|
|------|-------|
|--data_type **DATA_TYPE**|Target data type. For PNG: `Byte` or `UInt16`|
|--scale **SRC_MIN SRC_MAX DST_MIN DST_MAX**|Maps source range of [SRC_MIN, SRC_MAX] to target (PNG) range [DST_MIN, DST_MAX]. By default for *integer source* and unsigned 16-bit target (PNG data) `--scale -1000 0 0 1000` is assumed (leaving 1 m source resolution, but shifting data range), whereas for *floating point source* and unsigned 16-bit target (PNG data) `--scale -1000 0 0 5000` is assumed (20cm resolution, shifting data range)|
|--no_data **NO_DATA**|NoData value to use. By default for unsigned 16-bit target (PNG data) 65535 is used|


## Scripts <a name="scripts"/>

## *dir_md5.py* - computing MD5 hash over geodetic file directory <a name="dir_md5"/>

Groups of geospatial file (same data, same purpose, same region) have (or at least - should have) `..._version_info.json` files containing identification and provenance of these file groups. These `..._version_info.json` files have `md5` field containing MD5 of files belonging to group.

`dir_md5.py` computes MD5 of given group of files. It may also update `..._version_info.json` file of this group or verify MD5 in file against actual MD5. Also it verifies the correctness of `..._version_info.json` against its schema (stored in `g8l_info_schema.json`).

MD5 of files in group computed in filenames' lexicographical order. Names themselves are not included into MD5.

Usually this utility works with ~0.5 GB/sec speed.

`$ dir_md5.py subcommand [options] [FILES_AND_DIRS]`

`FILES_AND_DIRS` explicitly specify files over which to compute MD5 (wildcards are allowed), may be used in `list` and `compute` subcommands. Yet it is recommended to use `..._version_info.json` files to specify file groups.

Subcommands:

|Subcommand|Function|
|----------|--------|
|list|Do not compute MD5, just list files that will be used in computation in order they'll be used|
|compute|Compute and print MD5|
|verify|Compute MD5 and compare it with one, stored in `..._version_info.json` file|
|update|Compute MD5 and write it to `..._version_info.json` file|
|help|Print help on a particular subcommand|


Options:

|Option|Function|
|------|--------|
|--json **JSON_FILE**|JSON file containing description of file group(s). Since it is located in the same directory as files, its name implicitly defines their location|
|--mask **WILDCARD**|Explicitly specified file group. This option may be defined several times. Note that order of files in MD5 defined by filenames, not by order of how they were listed|
|--recursive|Also include files in subdirectories (may be used for LiDAR files)|
|--progress|Print progress information|
|--stats|Print statistics|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|

### *nlcd_wgs84.py* - converting land usage data to AFC-compatible format <a name="nlcd_wgs84"/>

AFC Engine employs land usage data (essentially - urban/rural classification) in form of NLCD files. NLCD files used by AFC are in WGS84 geodetic (latitude/longitude) coordinate system.

AFC Engine expects land usage files to have WGS84 geodetic (latitude/longitude) coordinate system. It expects NLCD land usage codes to be used (see *nlcd_wgs84.yaml* for more information).

Source data for these files released in different forms:
* NLCD files as such - they have map projection (not geodetic) coordinate system.

* NOAA files - contain (as of time of this writing) more recent and detailed land usage data for Hawaii, Puerto Rico, Virgin Islands. These files also have map projection (not geodetic) coordinate system, also they have different land usage codes (that need to be translated to NLCD land usage codes).

* Canada land usage file - covers entire Canada, uses map projection coordinate system and yet anther land usage codes.

`nlcd_wgs84.py` script converts NLCD data files from the source format to one, used by AFC Engine.

There is a *nlcd_wgs84.yaml* file that accompanies this script. It defines NLCD encoding properties (code meaning and colors) and other encodings (code meaning and translation to NLCD). Presence of this file in same directory as `nlcd_wgsm4.py` is mandatory.

`nlcd_wgs84.py [options] SOURCE_FILE DEST_FILE`

Options:

|Option|Function|
|------|--------|
|--pixel_size **DEGREES**|Pixel size of resulting file in degrees|
|--pixels_per_degree **NUMBER**|Pixel size of resulting file in form of number of pixels per degree. As of time of this writing *3600* recommended for both NLCD and NOAA source data. If pixel size not specified in any way, `gdalwarp` utility will decide - this is not recommended|
|--top **MAX_LAT**|Optional upper crop boundary. **MAX_LAT** is north-positive latitude in degrees|
|--bottom **MIN_LAT**|Optional lower crop boundary. **MIN_LAT** is north-positive latitude in degrees|
|--left **MIN_LON**|Optional left crop boundary. **MIN_LON** is east-positive longitude in degrees|
|--right **MAX_LON**|Optional right crop boundary. **MAX_LON** is east-positive longitude in degrees|
|--encoding **ENCODING**|Translate land codes from given encoding (as of time of this writing - 'noaa' or 'canada'). All encodings defined in *nlcd_wgs84.yaml* file|
|--format **FORMAT**|Output file format (short) name. By default guessed from output file extension. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--format_param **NAME=VALUE**|Set output format option. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--resampling **METHOD**|Resampling method to use. Default for land usage is 'nearest'. See [gdalwarp -r](https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r) for more details|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|
|--overwrite|Overwrite target file if it exists|

### *lidar_merge.py* - flattens lidar files <a name="lidar_merge"/>

LiDAR files (misnomer, as most other terrain files are also obtained by LiDARs. Whatever) are high-resolution (one meter for USA) surface model (contain both terrain and rooftops) for urban agglomerations. Their original representation is complicated multilevel directory structure (which is a product of `proc_lidar` script - but that's a different story).

`lidar_merge.py` converts mentioned multilevel structure into flat geospatial files (one file per agglomeration).

`lidar_merge.py [options] SRC_DIR DST_DIR`

Here `SRC_DIR` is a root of multilevel structure (contains pert-agglomeration .csv files), `DST_DIR` is a directory for resulting flat files. Options are:

|Option|Function|
|------|--------|
|--overwrite|Overwrite already existing resulting files. By default already existing resulting files considered to be completed, thus facilitating process restartability|
|--out_ext **EXT**|Extension of output files. By default same as input terrain files' extension (i.e. *.tif*)|
|--locality **LOCALITY**|Do conversion only for given locality/localities (parameter may be specified several times). **LOCALITY** is base name of correspondent .csv file (e.g. *San_Francisco_CA*)|
|--verbose|Do conversion one locality at a time with immediate print of utilities' output. Slow. For debug purposes|
|--format **FORMAT**|Output file format (short) name. By default guessed from output file extension. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--format_param **NAME=VALUE**|Set output format option. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--resampling **METHOD**|Resampling method to use. See [gdalwarp -r](https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r) for more details. Default is 'cubic'|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|

### *to_wgs84.py* - change coordinate system <a name="to_wgs84"/>

All terrain source files have heights relative to geoids - whereas AFC Engine expects heights to be relative to WGS84 ellipsoid.

All land usage and some terrain source files are in map projection coordinate system whereas AFC Engine expects them to be in geodetic (latitude/longitude) coordinate system.

Some source files are in geodetic coordinate system that is not WGS84.

`to_wgs84.py` script converts source files to ones with WGS84 horizontal (latitude/longitude) and vertical (heights relative to WGSD84 - where applicable) coordinate systems. In certain cases it may convert both vertical and horizontal coordinate systems in one swoop, but this is **not recommended** as performance degrades dramatically (as it involves geoid backprojection - don't ask...). Instead it is recommended to fix horizontal coordinate system (if needed) first and then vertical (if needed).

`to_wgs84.py [options] FILES`

Here `FILES` may be:
- `SRC_FILE DST_FILE` pair - if `--out_dir` option not specified
- One or more filenames that may include wildcards - if `--out_dir` specified, but `--recursive` not specified
- One or more `BASE_DIR/*.EXT` specifiers - if both `--out_dir` and `--recursive` not specified. In this case files are being looked up in subdirectories and structure of these subdirectories is copied to output directory.

|Option|Function|
|------|--------|
|--pixel_size **DEGREES**|Pixel size of resulting file in degrees|
|--pixels_per_degree **NUMBER**|Pixel size of resulting file in form of number of pixels per degree|
|--round_pixels_to_degree|Round current pixel size to whole number of pixels per degree. Latitudinal and longitudinal sizes rounded independently|
|--top **MAX_LAT**|Optional upper crop boundary. **MAX_LAT** is north-positive latitude in degrees|
|--bottom **MIN_LAT**|Optional lower crop boundary. **MIN_LAT** is north-positive latitude in degrees|
|--left **MIN_LON**|Optional left crop boundary. **MIN_LON** is east-positive longitude in degrees|
|--right **MAX_LON**|Optional right crop boundary. **MAX_LON** is east-positive longitude in degrees|
|--round_boundaries_to_degree|Extend boundaries to next whole degree outward. This (along with `pixels_per_degree` or `--round_pixels_to_degree`) makes pixel mesh nicely aligned to degrees, that help create nicer tiles afterwards|
|--src_geoid **GEOID_PATTERN**|Assume source file has geoidal heights, relative to given geoid. **GEOID_PATTERN** is geoid file name(s) - if geoid consists of several files (e.g. US geoids) **GEOID_PATTERN** may contain wildcards (to handle geoids consisting of several files - like US geoids). This option may be specified several times (for source file set that covers large spaces)|
|--dst_geoid **GEOID_PATTERN**|Make resulting file contain geoidal heights, relative to given geoid (e.g. to test compatibility with other AFC implementations). This option may be specified several times, **GEOID_PATTERN** may contain wildcards - just like for `--src_geoid`|
|--extend_geoid_coverage **AMOUNT**|Artificially extend geoid file coverage by given **AMOUNT** (specified in degrees) when testing which geoid file covers file being converted. Useful when source files have margins|
|--format **FORMAT**|Output file format (short) name. By default guessed from output file extension. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--format_param **NAME=VALUE**|Set output format option. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--resampling **METHOD**|Resampling method to use. See [gdalwarp -r](https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r) for more details. Default is 'nearest' for byte data (e.g. land usage), 'cubic' otherwise|
|--out_dir **DIRECTORY**|Do the mass conversion to given directory. In this case `FILES` in command line is a list of files to convert|
|--out_ext **EXT**|Extension of resulting files. May be used in mass conversion. By default source files' extension is kept|
|--overwrite|Overwrite already existing resulting files. By default already existing resulting files considered to be completed, thus facilitating process restartability|
|--remove_src|Remove source files after successful conversion. May help save disk space (e.g. when dealing with huge LiDAR files)|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|

### *to_png.py* - converts files to PNG format <a name="to_png"/>

Convert source files to PNG. PNG is special as it may only contain 1 or 2 byte unsigned integer pixel data. To represent heights (that may be negative and sub-meter) it requires offsetting and scaling.

Also for some strange reason PNG files contain very limited metadata information (what coordinate system used, etc.), so they should only be used as terminal files in any sentence of conversions.

`to_png.py` script converts source data files to PNG format.

`to_png.py [options] FILES`

Here `FILES` may be pair `SRC_FILE DST_FILE` or, if `--out_dir` option specified, `FILES` are source files (in any number, may include wildcards).

|Option|Function|
|------|--------|
|--pixel_size **DEGREES**|Pixel size of resulting file in degrees|
|--pixels_per_degree **NUMBER**|Pixel size of resulting file in form of number of pixels per degree|
|--round_pixels_to_degree|Round current pixel size to whole number of pixels per degree. Latitudinal and longitudinal sizes rounded independently|
|--top **MAX_LAT**|Optional upper crop boundary. **MAX_LAT** is north-positive latitude in degrees|
|--bottom **MIN_LAT**|Optional lower crop boundary. **MIN_LAT** is north-positive latitude in degrees|
|--left **MIN_LON**|Optional left crop boundary. **MIN_LON** is east-positive longitude in degrees|
|--right **MAX_LON**|Optional right crop boundary. **MAX_LON** is east-positive longitude in degrees|
|--round_boundaries_to_degree|Extend boundaries to next whole degree outward. This (along with `pixels_per_degree` or `--round_pixels_to_degree`) makes pixel mesh nicely aligned to degrees, that help create nicer tiles afterwards|
|--no_data **VALUE**|Value to use as NoData. By default - same as in source for 1-byte pixels (e.g. land usage, 65535 for 2-byte pixels|
|--scale **SRC_MIN SRC MAX DST_MIN DST_MAX**|Maps source data to destination in a way tat [**SRC_MIN**, **SRC_MAX**] interval maps to [**DST_MIN**, **DST_MAX**]. Default is identical no mapping for 1-byte target data, -1000 0 0 1000 for 2-byte target and integer source data (keeping 1 m resolution), -1000 0 0 5000 for 2-byte target and floating point source data (20cm resolution)|
|--data_type **DATA_TYPE**|Pixel data type: 'Byte' or 'UInt16'|
|--format **FORMAT**|Output file format (short) name. By default guessed from output file extension. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--format_param **NAME=VALUE**|Set output format option. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--resampling **METHOD**|Resampling method to use. See [gdalwarp -r](https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r) for more details. Default is 'nearest' for byte data (e.g. land usage), 'cubic' otherwise|
|--out_dir **DIRECTORY**|Do the mass conversion to given directory. In this case `FILES` in command line is a list of files to convert|
|--recursive|Look in subdirectories of given source file parameters and copy subdirectory structure to output directory (that must be specified)|
|--out_ext **EXT**|Extension of resulting files. May be used in mass conversion. By default source files' extension is kept|
|--overwrite|Overwrite already existing resulting files. By default already existing resulting files considered to be completed, thus facilitating process restartability|
|--remove_src|Remove source files after successful conversion. May help save disk space (e.g. when dealing with huge LiDAR files)|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|

### *tiler.py* - cuts source files to 1x1 degree tiles <a name="tiler"/>

Ultimately all geospatial files should be tiled to 1x1 degree files (actually, slightly wider, as tiles may have outside 'margins' several pixel wide). Source files for tiling may be many and they may overlap, in overlap zones some source files are preferable with respect to other.

Some resulting tiles, filled only with NoData or some other ignored values should be dropped.

Tile files should be named according to latitude and longitude of their location.

`tiler.py` script cuts source files to tiles.

It names tile files according to given pattern that contains *{VALUE[:FORMAT]}* inserts. Here *FORMAT* is integer format specifier (e.g. 03 - 3 character wide with zero padding). Following values for *VALUE* are accepted:

|Value|Meaning|
|-----|-------|
|lat_hem|Lowercase tile latitude hemisphere (**n** or **s**)|
|LAT_HEM|Uppercase tile latitude hemisphere (**N** or **S**)|
|lat_u|Latitude of tile top|
|lat_l|Latitude of tile bottom|
|lon_hem|Lowercase tile longitude hemisphere (**e** or **w**)|
|LON_HEM|Uppercase tile longitude hemisphere (**E** or **W**)|
|lon_u|Longitude of tile left|
|lon_l|Longitude of tile right|

E.g. standard 3DEP file names have the following pattern: *USGS_1_{lat_hem}{lat_u}{lon_hem}{lon_l}.tif*

`tiler.py [options] FILES`

Here `FILES` specify names of source files that need to be tiled (filenames may contain wildcards). Files specified in order of preference - preferable come first.

|Option|Function|
|------|--------|
|--tile_pattern **PATTERN**|Pattern for tile file name. May include directory. See above in this chapter|
|--remove_value **VALUE**|Drop tiles that contain only this value and NoData value. This option may be specified several times, however only 'monochrome' tiles are dropped|
|--pixel_size **DEGREES**|Pixel size of resulting file in degrees|
|--pixels_per_degree **NUMBER**|Pixel size of resulting file in form of number of pixels per degree|
|--round_pixels_to_degree|Round current pixel size to whole number of pixels per degree. Latitudinal and longitudinal sizes rounded independently|
|--top **MAX_LAT**|Optional upper crop boundary. **MAX_LAT** is north-positive latitude in degrees|
|--bottom **MIN_LAT**|Optional lower crop boundary. **MIN_LAT** is north-positive latitude in degrees|
|--left **MIN_LON**|Optional left crop boundary. **MIN_LON** is east-positive longitude in degrees|
|--right **MAX_LON**|Optional right crop boundary. **MAX_LON** is east-positive longitude in degrees|
|--margin **NUM_PIXELS**|Makes outer margin **NUM_PIXELS** wide around created tiles|
|--no_data **VALUE**|Value to use as NoData. By default - same as in source for 1-byte pixels (e.g. land usage, 65535 for 2-byte pixels|
|--scale **SRC_MIN SRC MAX DST_MIN DST_MAX**|Maps source data to destination in a way tat [**SRC_MIN**, **SRC_MAX**] interval maps to [**DST_MIN**, **DST_MAX**]. Default is identical no mapping for 1-byte target data, -1000 0 0 1000 for 2-byte target and integer source data (keeping 1 m resolution), -1000 0 0 5000 for 2-byte target and floating point source data (20cm resolution)|
|--data_type **DATA_TYPE**|Pixel data type. See [gdal_translate -ot](https://gdal.org/programs/gdal_translate.html#cmdoption-gdal_translate-ot) fro more details||
|--format **FORMAT**|Output file format (short) name. By default guessed from output file extension. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--format_param **NAME=VALUE**|Set output format option. See [GDAL Raster Drivers](https://gdal.org/drivers/raster/index.html) for more information|
|--resampling **METHOD**|Resampling method to use. See [gdalwarp -r](https://gdal.org/programs/gdalwarp.html#cmdoption-gdalwarp-r) for more details. Default is 'nearest' for byte data (e.g. land usage), 'cubic' otherwise|
|--out_dir **DIRECTORY**|Do the mass conversion to given directory. In this case `FILES` in command line is a list of files to convert|
|--overwrite|Overwrite already existing resulting files. By default already existing resulting files considered to be completed, thus facilitating process restartability|
|--verbose|Create one tile at a time, printing all output in real time. Slow. For debug purposes|
|--threads [-]**N**[%]|How many CPUs to use (if positive), leave unused (if negative) or percent of CPUs (if followed by %)|
|--nice|Lower priority (on Windows required `psutil` Python module)|


## Conversion routines <a name="conversion_routines"/>

For sake of simplicity, all examples will be in non-Docker form. Also for sake of simplicity `--threads` and `--nice` will be omitted.

All arguments containing wildcard, etc. are in single quotes. This is not necessary for positional parameters in non-Docker operation, yet for Docker operation it is always necessary, so it is done to simplify switching to Docker. Feel free to omit them for positional parameters.

As of time of thsi writing by-region storage of geodetic files was not implemented yet. Hence proper naming (prefixing) of final files is not covered here. Maybe some day...

### Geoids <a name="geoid_rouitines/">

#### USA geoids <a name="usa_geoids_routines"/>

Most recent USA geoid is GEOID18, it only covers CONUS and Puerto Rico. Previous geoid is GEOID12B, it covers CONUS, Alaska, Guam, Hawaii, Guam, Hawaii, Puerto Rico.

1. *GEOID18*
   - Download from [Geoid 18 Data](https://vdatum.noaa.gov/download.php) as [NGS data (.bin) zip](https://www.myfloridagps.com/Geoid/NGS.zip)
   - CONUS: `gdal_translate -of GTX g2018u0.bin usa_ge_prd_g2018u0.gtx`
   - Puerto Rico: `gdal_translate -of GTX g2018p0.bin usa_ge_prd_g2018p0.gtx`

2. *GEOID12B*
   - Download from [NOAA/NOS's VDatum: Download VDatum](https://vdatum.noaa.gov/download.php) as [GEOID12B](https://vdatum.noaa.gov/download/data/vdatum_GEOID12B.zip).
   - vdatum/core/geoid12b/g2012ba0.gtx for Alaska -> usa_ge_prd_g2012ba0.gtx
   - vdatum/core/geoid12b/g2012bg0.gtx for Guam -> usa_ge_prd_g2012bg0.gtx
   - vdatum/core/geoid12b/g2012bh0.gtx for Hawaii -> usa_ge_prd_g2012bh0.gtx
   - vdatum/core/geoid12b/g2012bp0.gtx for Puerto Rico -> usa_ge_prd_g2012bp0.gtx
   - vdatum/core/geoid12b/g2012bs0.gtx for Samoa -> usa_ge_prd_g2012bs0.gtx
   - vdatum/core/geoid12b/g2012bu0.gtx for CONUS. -> usa_ge_prd_g2012bu0.gtx

#### Canada geoid <a name="canada_geoid_procedure"/>

Most recent Canada geoid is HTv2, Epoch 2010.

- Download from [Geoid Models](https://webapp.csrs-scrs.nrcan-rncan.gc.ca/geod/data-donnees/geoid.php?locale=en) (registration required) as [HTv2.0, GeoTIFF, 2010](https://webapp.csrs-scrs.nrcan-rncan.gc.ca/geod/process/download-helper.php?file_id=HT2_2010_tif)
     
- Convert to .gtx: `gdal_translate -of GTX HT2_2010v70.tif can_ge_prd_HT2_2010v70.gtx`

#### World geoids <a name="world_geoids_routines"/>

EGM1996 (aka EGM96) is rather small (low resolution), EGM2008 (aka EGM08) is rather large (~1GB). Use whatever you prefer.

1. *EGM1996*
   - Download from [NOAA/NOS's VDatum: Download VDatum](https://vdatum.noaa.gov/download.php) as [EGM1996](https://vdatum.noaa.gov/download/data/vdatum_EGM1996.zip).
   - vdatum/core/egm1996/egm1996.gtx -> wrd_ge_prd_egm1996.gtx

2. *EGM2008*
   - Download from [NOAA/NOS's VDatum: Download VDatum](https://vdatum.noaa.gov/download.php) as [EGM1996](https://vdatum.noaa.gov/download/data/vdatum_EGM2008.zip).
   - vdatum/core/egm1996/egm2008.gtx -> wrd_ge_prd_egm2008.gtx

### USA/Canada/Mexico 3DEP terrain model <a name="3dep_routine"/>

This terrain model is a set of 1x1 degree tile files, horizontal coordinate system is WGS84, heights are geoidal (since USA and Canada geoids tied to same point, any of them might be used)

1. Download from [Rockyweb FTP server](https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/1/TIFF/current/) From all downloaded stuff only USGS_1_*.tif files are needed. They should be put to single directory (let it be *DOWNLOAD_DIR*)

2. Convert 3DEP files in *DOWNLOAD_DIR* to WGS84 horizontal coordinates (leaving heights geoidal). Result is in *3dep_wgs84_geoidal_tif*:  
`to_wgs84.py --out_dir 3dep_wgs84_geoidal_tif 'DOWNLOAD_DIR/*.tif`  
Takes ~40 minutes on 8 CPUs. YMMV

3. Convert heights of files in *3dep_wgs84_geoidal_tif* to ellipsoidal. Result in *3dep_wgs84_tif*  
```
to_wgs84.py --src_geoid 'GEOID_DIR/usa_ge/usa_ge_prd_g2018?0.gtx' \
    --src_geoid 'GEOID_DIR/usa_ge/usa_ge_prd_g2012b?0.gtx' \
    --src_geoid GEOID_DIR/can_ge/can_ge_prd_HT2_2010v70.gtx \
    --format_param COMPRESS=PACKBITS \
    --out_dir 3dep_wgs84_tif '3dep_wgs84_geoidal_tif/*.tif`
```  
Some Mexican tiles will be dropped, as they are not covered by USA or Canada geoids
Takes ~1.5 hours on 8 CPUs. YMMV

4. Convert TIFF tiles in *3dep_wgs84_tif* to PNG tiles in *3dep_wgs84_png*  
`to_png.py --out_dir 3dep_wgs84_png '3dep_wgs84_tif/*.tif'`
Takes 13 minutes on 8 CPUa. YMMV


### Coarse SRTM/GLOBE world terrain <a name="coarse_terrain_routine">

SRTM is terrain model of 3 arcsecond resolution that covers Earth between 60N and 56S latitudes. It consists of 1x1 degree tile files that have WGS84 horizontal coordinates and Mean Sea Level (global geoidal) heights.

GLOBE is global terrain model of 30 arcsecond resolution that covers entire Earth. It consists of large tiles that have WGS84 horizontal coordinates and Mean Sea Level (global geoidal) heights.

Let's create combined tiled global terrain model.

1. Download Globe files from [Get Data Tiles-Global Land One-km Base Elevation Project | NCEI](https://www.ngdc.noaa.gov/mgg/topo/gltiles.html) as [All Tiles in One .zip file](https://www.ngdc.noaa.gov/mgg/topo/DATATILES/elev/all10g.zip)   
  Unpack files to *all10* directory

2. Download header files for Globe files from [Topography and Digital Terrain Data | NCEI](https://www.ngdc.noaa.gov/mgg/topo/elev/esri/hdr/) - to the same *all10* directory

3. SRTM data - finding source data to download is not that easy (albeit probably possible). Assume we have them in *srtm_geoidal_hgt* directory.

4. Ascribe coordinate system to GLOBE files in *all10* and convert them to .tif. Result in *globe_geoidal_tif*:  
```
mkdir globe_geoidal_tif
for fn in all10/???? ; do gdal_translate -a_srs '+proj=longlat +datum=WGS84' $fn globe_geoidal_tif/${fn##*/}.tif ; done
```  
Takes ~1 minute. YMMV

5. Convert heights of files in *globe_geoidal_tif* directory to ellipsoidal. Result in *globe_wgs84_tif*:  
   `to_wgs84.py --src_geoid GEOID_DIR/wrd_ge/wrd_ge_prd_egm2008.gtx --out_dir globe_wgs84_tif 'globe_geoidal_tif/*.tif'`  
Takes ~1 minute on 8 CPUs. YMMV

6. Convert heights of SRTM in *srtm_geoidal_hgt* directory to WGS84. Result in *wgs84_tif* directory:  
   `to_wgs84.py --src_geoid GEOID_DIR/wrd_ge/wrd_ge_prd_egm2008.gtx --out_ext .tif --out_dir wgs84_tif 'srtm_geoidal_hgt/*.hgt'`  
Takes ~40 minutes on 8 CPUs. YMMV

7. Add GLOBE tiles to the north of 60N to *wgs84_tif* directory:  
```
tiler.py --bottom 60 --margin 1 \
    --tile_pattern 'wgs84_tif/{LAT_HEM}{lat_d:02}{LON_HEM}{lon_l:03}_globe.tif' \
    'globe_wgs84_tif/*.tif'
```  
Takes ~8 minutes on 8 CPUs. YMMV

8. Add GLOBE tiles to the south of 567S to *wgs84_tif* directory:  
```
tiler.py --top=-56 --margin 1 \
    --tile_pattern 'wgs84_tif/{LAT_HEM}{lat_d:02}{LON_HEM}{lon_l:03}_globe.tif' \
    'globe_wgs84_tif/*.tif'
```
Takes ~9 minutes on 8 CPUs. YMMV

9. Convert TIFF files in *wgs84_tif* directory to PNG in *wgs84_png*:  
   `to_png.py --data_type UInt16 --out_dir wgs84_png 'wgs84_tif/*.tif'`  
Takes ~45 minutes on 8 CPUs. YMMV


### High resolution terrain data (aka LiDAR)  <a name="lidar_routine">

Assume LiDAR files (multilevel set of 2-band TIFF files, indexed by .csv files) is in *proc_lidar* directory. Let's convert it to tiles.

1. Convert LiDARS to set of per-agglomeration TIFF files with geoidal heights. Result in *lidar_geoidal_tif* directory:  
  `lidar_merge.py --format_param BIGTIFF=YES --format_param COMPRESS=ZSTD proc_lidar lidar_geoidal_tif`  
  Here ZSTD compression was used to save disk space, albeit it makes conversion process slower.  
  Takes 2.5 hours on 8 CPUs. YMMV

2. Tile lidars in *lidar_geoidal_tif*. Result is in *tiled_lidar_geoidal_tif* directory:  
```
tiler.py --format_param BIGTIFF=YES --format_param COMPRESS=ZSTD --margin 2 \
    --tile_pattern 'tiled_lidar_geoidal_tif/{lat_hem}{lat_u:02}{lon_hem}{lon_l:03}.tif' \
    'lidar_geoidal_tif/*.tif'
```   
  Takes 5 hours on 8 CPUs. YMMV

3. Convert geoidal heights of tiles in *tiled_lidar_geoidal_tif* to ellipsoidal. Result in *tiled_lidar_wgs84_tif* directory:  
```
to_wgs84.py --format_param BIGTIFF=YES --format_param COMPRESS=ZSTD --remove_src \
    --src_geoid 'GEOID_DIR/usa_ge/usa_ge_prd_g2018?0.gtx' \
    --src_geoid 'GEOID_DIR/usa_ge/usa_ge_prd_g2012b?0.gtx' \
    --src_geoid GEOID_DIR/can_ge/can_ge_prd_HT2_2010v70.gtx \
    --out_dir tiled_lidar_wgs84_tif \
    'tiled_lidar_geoidal_tif/*.tif'
```  
Takes ***22 hours on 16 CPUs (probably ~45 hours on 8CPUs)***. YMMV

4. Convert TIF files in *tiled_lidar_wgs84_tif* directory to PNG files in *tiled_lidar_wgs84_png* directory:  
`to_png.py --out_dir tiled_lidar_wgs84_png 'tiled_lidar_wgs84_tif/*'`  
Takes ~8 hours on 8 CPUs. YMMV


### Canada CDSM surface model <a name="cdsm_routine">

CDSM is a surface (rooftop/treetop) model that covers canada up to 61N latitude.It can be downloaded from [Canadian Digital Surface Model 2000](https://open.canada.ca/data/en/dataset/768570f8-5761-498a-bd6a-315eb6cc023d) page as [Cloud Optimized GeoTIFF of the CDSM](https://datacube-prod-data-public.s3.ca-central-1.amazonaws.com/store/elevation/cdem-cdsm/cdsm/cdsm-canada-dem.tif)

1. Converting *cdsm-canada-dem.tif* to WGS84 horizontal coordinate system (leaving heights geoidal) to *dsm-canada-dem_wgs84_geoidal.tif*  
```
to_wgs84.py --pixels_per_degree 3600 --round_boundaries_to_degree \
    --format_param BIGTIFF=YES --format_param COMPRESS=PACKBITS \
    cdsm-canada-dem.tif cdsm-canada-dem_wgs84_geoidal.tif
```  
Takes ~3 hours. YMMV

2. Tiling *dsm-canada-dem_wgs84_geoidal.tif* to *tiled_wgs84_geoidal_tif/can_rt_n<lat>w<lon>.tif*  
```
tiler.py --margin 1 --format_param COMPRESS=PACKBITS \
    --tile_pattern 'tiled_wgs84_geoidal_tif/{lat_hem}{lat_u:02}{lon_hem}{lon_l:03}.tif' \
    dsm-canada-dem_wgs84_geoidal.tif
```  
Lot of tiles contain nothing but NoData - they are dropped.
Takes ~1 hour on 8 CPUs. YMMV

3. Converting heights of files in *tiled_wgs84_geoidal_tif* to ellipsoidal, result in *tiled_wgs84_tif*. Geoids are under GEOID_DIR, using Canada HTv2 geoid
```
to_wgs84.py --src_geoid GEOID_DIR/can_ge/can_ge_prd_HT2_2010v70.gtx \
    --extend_geoid_coverage 0.001 \
    --out_dir tiled_wgs84_tif 'tiled_wgs84_geoidal_tif/*.tif'
```  
Takes ~10 minutes on 8 CPU. YMMV

4. Converting tiff tiles in *tiled_wgs84_tif* to png, result in *tiled_wgs84_png*  
`to_png.py --out_dir tiled_wgs84_png 'tiled_wgs84_tif/*.tif'`  
Takes ~3 minutes on 8 CPU. YMMV

### Land usage files <a name="land_usage_routines">

The biggest issue with land usage files is to catch 'em all.

#### NLCD land usage files <a name="nlcd_routine"/>

1. Downloading sources.
   - CONUS 2019. From [Data | Multi-Resolution Land Characteristics (MLRC) Consortium. Pick 'Land Cover' and 'CONUS'](https://www.mrlc.gov/data?f%5B0%5D=category%3ALand%20Cover&f%5B1%5D=region%3Aconus) download [NLCD 2019 Land Cover (CONUS)](https://s3-us-west-2.amazonaws.com/mrlc/nlcd_2019_land_cover_l48_20210604.zip)  
     Unpack to *nlcd_sources* folder
   - Alaska 2016. From [Data | Multi-Resolution Land Characteristics (MLRC) Consortium. Pick 'Land Cover' and 'Alaska'](https://www.mrlc.gov/data?f%5B0%5D=category%3ALand%20Cover&f%5B1%5D=region%3Aalaska) download [NLCD 2016 Land Cover (ALASKA)](https://s3-us-west-2.amazonaws.com/mrlc/NLCD_2016_Land_Cover_AK_20200724.zip)  
     Unpack to *nlcd_sources* folder

2. Converting NLCD sources (that use map projection coordinate system) in *nlcd_sources* to TIFF files with WGS84 coordinate system. Result in *nlcd_wgs84_tif*
  - CONUS 2019  
```
nlcd_wgs84.py --pixels_per_degree 3600 \
    --format_param BIGTIFF=YES --format_param COMPRESS=PACKBITS \
    nlcd_sources/nlcd_2019_land_cover_l48_20210604.img \
    nlcd_wgs84_tif/nlcd_2019_land_cover_l48_20210604.tif
```  
Takes ~50 minutes. YMMV
  - Alaska 2016. Crop at 53N to avoid intersection with CONUS land coverage (which might be an issue when used without tiling):   
```
nlcd_wgs84.py --pixels_per_degree 3600 --bottom 53 \
    --format_param BIGTIFF=YES --format_param COMPRESS=PACKBITS \
    nlcd_sources/NLCD_2016_Land_Cover_AK_20200724.img \
    nlcd_wgs84_tif/NLCD_2016_Land_Cover_AK_20200724.tif
```
Takes ~9 minutes. YMMV

3. Tiling files in *nlcd_wgs84_tif*, result in *tiled_nlcd_wgs84_tif*:  
```
tiler.py --format_param COMPRESS=PACKBITS --remove_value 0 \
    --tile_pattern 'tiled_nlcd_wgs84_tif/{lat_hem}{lat_u:02}{lon_hem}{lon_l:03}.tif' \
    'nlcd_wgs84_tif/*.tif'
```  
Takes ~13 minutes on 8 CPUs. YMMV

4. Converting tiles in *tiled_nlcd_wgs84_tif* directory to PNG. Result in *tiled_nlcd_wgs84_png* directory:  
`to_png.py --out_dir tiled_nlcd_wgs84_png 'tiled_nlcd_wgs84_tif/*.tif'`  
Takes ~3 minutes on 8 CPUs. YMMV

#### NOOA land usage files <a name="noaa_routine"/>

1. Downloading sources:
   - Guam 2016. From [C-CAP Land Cover files for Guam](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/guam/) download [guam_2016_ccap_hr_land_cover20180328.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/guam/guam_2016_ccap_hr_land_cover20180328.img) to *noaa_sources* directory
   - Hawaii 2005-2011. From [C-CAP Land Cover files for Hawaii](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/) download to *noaa_sources* directory:
     - [hi_hawaii_2005_ccap_hr_land_cover.ige](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_hawaii_2005_ccap_hr_land_cover.ige)
     - [hi_hawaii_2010_ccap_hr_land_cover20150120.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_hawaii_2010_ccap_hr_land_cover20150120.img)
     - [hi_kahoolawe_2005_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_kahoolawe_2005_ccap_hr_land_cover.img)
     - [hi_kauai_2010_ccap_hr_land_cover20140929.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_kauai_2010_ccap_hr_land_cover20140929.img)
     - [hi_lanai_2011_ccap_hr_land_cover_20141204.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_lanai_2011_ccap_hr_land_cover_20141204.img)
     - [hi_maui_2010_ccap_hr_land_cover_20150213.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_maui_2010_ccap_hr_land_cover_20150213.img)
     - [hi_molokai_2010_ccap_hr_land_cover20150102.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_molokai_2010_ccap_hr_land_cover20150102.img)
     - [hi_niihau_2010_ccap_hr_land_cover20140930.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_niihau_2010_ccap_hr_land_cover20140930.img)
     - [hi_oahu_2011_ccap_hr_land_cover20140619.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/hi/hi_oahu_2011_ccap_hr_land_cover20140619.img)
   - Puerto Rico 2010. From [C-CAP Land Cover files for Puerto Rico](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/pr/) download to *noaa_sources* directory
     - [pr_2010_ccap_hr_land_cover20170214.ige](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/pr/pr_2010_ccap_hr_land_cover20170214.ige)
     - [pr_2010_ccap_hr_land_cover20170214.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/pr/pr_2010_ccap_hr_land_cover20170214.img) 
   - US Virgin Islands 2012. From [C-CAP Land Cover files for US Virgin Islands](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/usvi/) download to *noaa_sources* directory:
     - [usvi_stcroix_2012_ccap_hr_land_cover20150910.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/usvi/usvi_stcroix_2012_ccap_hr_land_cover20150910.img)
     - [usvi_stjohn_2012_ccap_hr_land_cover20140915.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/usvi/usvi_stjohn_2012_ccap_hr_land_cover20140915.img)
     - [usvi_stthomas_2012_ccap_hr_land_cover20150914.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/usvi/usvi_stthomas_2012_ccap_hr_land_cover20150914.img)
   - American Samoa 2009-2010. From [C-CAP Land Cover files for American Samoa](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/) download to *noaa_sources* directory:
     - [as_east_manua_2010_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_east_manua_2010_ccap_hr_land_cover.img)
     - [as_rose_atoll_2009_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_rose_atoll_2009_ccap_hr_land_cover.img)
     - [as_swains_island_2010_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_swains_island_2010_ccap_hr_land_cover.img)
     - [as_tutuila_2010_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_tutuila_2010_ccap_hr_land_cover.img)
     - [https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_west_manua_2010_ccap_hr_land_cover.img](https://chs.coast.noaa.gov/htdata/raster1/landcover/bulkdownload/hires/as/as_west_manua_2010_ccap_hr_land_cover.img)

2. Converting NOAA sources (that use map projection coordinate system) in *noaa_sources* to TIFF files with WGS84 coordinate system. Result in *noaa_wgs84_tif*:  
```
for path_fn_ext in noaa_sources/*.img ; do fn_ext=${path_fn_ext##*/} ; fn=${fn_ext%.*} ; \
    nlcd_wgs84.py --encoding noaa --pixels_per_degree 3600 \
        --format_param BIGTIFF=YES --format_param COMPRESS=PACKBITS \
        ${path_fn_ext} noaa_wgs84_tif/${fn}.tif ; done
```
Takes ~12 minutes on 8 CPUs. YMMV

3. Tiling files in *noaa_wgs84_tif*, result in *tiled_noaa_wgs84_tif*:  
```
tiler.py --format_param COMPRESS=PACKBITS --remove_value 0 \
    --tile_pattern 'tiled_noaa_wgs84_tif/{lat_hem}{lat_u:02}{lon_hem}{lon_l:03}.tif' \
    'noaa_wgs84_tif/*.tif'
```
Takes ~10 seconds on 8 CPUs. YMMV

4. Converting tiles in *tiled_noaa_wgs84_tif* directory to PNG. Result in *tiled_noaa_wgs84_png* directory:  
`to_png.py --out_dir tiled_noaa_wgs84_png 'tiled_noaa_wgs84_tif/*.tif'`  
Takes ~2 seconds on 8 CPUs. YMMV

### Canada land usage files <a name="canada_land_usage_routine"/>
1. Downloading source. From [2015 Land Cover of Canada - Open Government Portal](https://www.mrlc.gov/data?f%5B0%5D=category%3ALand%20Cover&f%5B1%5D=region%3Aconus) download [https://ftp.maps.canada.ca/pub/nrcan_rncan/Land-cover_Couverture-du-sol/canada-landcover_canada-couverture-du-sol/CanadaLandcover2015.zip)  
  Unpack to *canada_lc_sources* folder

2. Converting Canada land cover sources (that use map projection coordinate system) in *canada_lc_sources* to TIFF files with WGS84 coordinate system. Result in *canada_lc_wgs84_tif*:  
```
nlcd_wgs84.py --encoding noaa --pixels_per_degree 3600 \
    --format_param BIGTIFF=YES --format_param COMPRESS=PACKBITS \
    canada_lc_sources/CAN_LC_2015_CAL.tif \
    canada_lc_wgs84_tif/CAN_LC_2015_CAL.tif
```  
Takes ***12.5 hours***. Of them 20 minutes was spent on 8 CPUs and the rest - on single CPU. YMMV

3. Tiling Canada land cover file in *canada_lc_wgs84_tif*, result in *tiled_canada_lc_wgs84_tif*:  
```
tiler.py --format_param COMPRESS=PACKBITS --remove_value 0 \
    --tile_pattern 'tiled_canada_lc_wgs84_tif/{lat_hem}{lat_u:02}{lon_hem}{lon_l:03}.tif' \
    canada_lc_wgs84_tif/CAN_LC_2015_CAL.tif
```  
Takes ~50 minutes on 8 CPUs. YMMV

4. Converting tiles in *tiled_canada_lc_wgs84_tif* directory to PNG. Result in *tiled_canada_lc_wgs84_png* directory:  
`to_png.py --out_dir tiled_canada_lc_wgs84_png 'tiled_canada_lc_wgs84_tif/*.tif'`
Takes ~4 minutes om 8 CPU. YMMV

## *proc_gdal* - creating LiDAR files <a name="proc_gdal"/>
This directory contains undigested, but very important files that convert LiDAR files from source format (as downloaded from https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities/ ) to form, compatible with AFC Engine (two-band geodetic raster files and their index .csv files)