open-afc/src contains several modules/packages.

afc-engine: Performs the core computational analysis to determine channel availability.  The afc-engine reads a json configuration file and a json request file, and generates a json response file.

coalition_ulsprocessor: Takes raw FS data in the format provide by the FCC, or corresponding  government agency for other countries, and pieces together FS links.  For the US, Radio Astronomy Site (RAS) data is in the file RASdatabase.dat.  The format of the file RASdatabase.dat is similar to a CSV format, except the field delimiter is the pipe symbol "|" and not a comma ",".  The file has no header line.  Each line describes a single RAS region.  The fields on each line are defined as follows.  Note that field 6 in the table below specifies an Exclusion Zone String for each record.  The allowable types of exclusion zone strings are:

* **One Rectangle** : RAS zone is a single rectangle
* **Two Rectangles** : RAS zone consists of two rectangles
* **Circle** : RAS zone is a circle
* **Horizon Distance** : RAS zone is defined by a distance to horizon calculation

Position | Data Element | Type | Description | Example
--- |--- |--- |--- |---
1 | Record Type | String | Fixed string "RA" to indicate Radio Astronomy | RA
2 | Name | String | Name of RAS | Allen Telescope Array
3 | Location | String | City, state of RAS | Hat Creek, CA
4 | Start Frequency | Double | Start frequency of RAS band in MHz | 6650
5 | Stop Frequency | Double | Stop frequency of RAS band in MHz | 6675.2
6 | Exclusion Zone | String | Exclusion Zone String | Horizon Distance
7 | Rect1 Lat1 | String | Min Latitude for first Rectangle in DD-MM-SS format, blank if unused |
8 | Rect1 Lat2 | String | Max Latitude for first Rectangle in DD-MM-SS format, blank if unused |
9 | Rect1 Lon1 | String | Min Longitude for first Rectangle in DD-MM-SS format, blank if unused |
10 | Rect1 Lon2 | String | Max Longitude for first Rectangle in DD-MM-SS format, blank if unused |
11 | Rect2 Lat1 | String | Min Latitude for second Rectangle in DD-MM-SS format, blank if unused |
12 | Rect2 Lat2 | String | Max Latitude for second Rectangle in DD-MM-SS format, blank if unused |
13 | Rect2 Lon1 | String | Min Longitude for second Rectangle in DD-MM-SS format, blank if unused |
14 | Rect2 Lon2 | String | Max Longitude for second Rectangle in DD-MM-SS format, blank if unused |
15 | Radius     | Double | Radius of Circle RAS zone in Km, blank if unused |
16 | Center Lat | String | Latitude of Center of Circle or Horizon Distance RAS zone in DD-MM-SS format, blank if unused | 40-49-03 N
17 | Center Lon | String | Longitude of Center of Circle or Horizon Distance RAS zone in DD-MM-SS format, blank if unused | 121-28-24 W
18 | Height AGL | Double | Height of RAS antenna to use for Horizon Distance RAS zone in meters, blank if unused | 6.1

afclogging: Library used by afc-engine for creating run logs.

afcsql: Library used by the afc-engine for reading sqlite database files.

ratapi: Python layer that provides the REST API for administration of the system and responding to requests

web: see src/web/README.md
