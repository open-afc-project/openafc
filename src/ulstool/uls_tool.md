# ***uls_tool.py***
# An instrument for working with AFC ULS data

## Cheat sheet
In examples below file/directory names are in CAMEL_CASE, mandatory syntax is
in lower case. Long commands are prolonged to next line using **\** sign.
Command lines prepended with **$** sign (to distinguish them from output).
*uls_tool.py* works on both Windows ands Linux.

* Help on subcommands, on a specific subcommand (help on *download_afc* in this
case):  
`$ uls_tool`  
`$ uls_tool download_afc --help`

* Date of latest ULS data on FCC server. ETA ~2 minutes  
`$ uls_tool.py latest`  
sample printout:  
`2022-01-25 08:11:03` # Time is FCC local (EST)

* Download latest raw ULS data files to given directory (e.g. for subsequent
reuse)  
`$ uls_tool.py latest --dir DIRECTORY`

* Download latest ULS data files and make AFC database from them. *fsid*s start
from 1. ETA ~10 minutes  
`$ uls_tool.py download_afc DATABASE.sqlite3`

* Ditto, but *fsid*s continue from PREV_AFC.sqlite3  
`$ uls_tool.py download_afc --prev_afc PREV_AFC.sqlite3 DATABASE.sqlite3`

* Make AFC database from previously downloaded raw ULS files  
`$ uls_tool.py download_afc --dir DIRECTORY --skip_download
DATABASE.sqlite3`

* Compare two AFC databases (FIRST.sqlite3 and SECOND.sqlite3)  
`$ uls_tool.py compare --removed IN_FIRST_ONLY.sqlite3 \ `  
`--added IN_SECOND_ONLY.sqlite3 FIRST.sqlite3 SECOND.sqlite3`  
Resulting statistics is printed, differences put to databases, specified with
*--added* and *--removed* switches

* Make KML file for displaying paths around San Jose on
https://mymaps.google.com  
'$ uls_tool.py gmap --lat 37 --lon -121 --distance 100 SAN_JOSE.kml `  
KML file has limit of 2000 features (points and lines), so center and radius
must be specified

* Get data, related to callsign ZUZZEL25 from EM table of raw ULS files to
.csv file  
`$ uls_tool.py csv --table EM --callsign ZUZZEL25 --dir_directory \`  
`--skip_download ZUZZEL.csv`

## Overview

### ULS overview

https://www.fcc.gov/wireless/universal-licensing-system is an FCC
infrastructure that maintains information about various RF spectrum users - in
particular, about 6GHz spectrum users. This information used by AFC server
engine to determine what channels may be used by 6GHz SP (Standard Power) WiFi
Access Points.

The unit of ULS registration (and update) is a **callsign** - caps alphanumeric
sequence. Every piece of ULS data somehow related to some callsign.

6GHz spectrum users registered in ULS, use transmitter at some fixed location
and receivers that are either at some fixed location (have registered permanent
RX antenna location) or not (mobile or whatever). AFC server ignores nonfixed
receivers and only concerns about fixed ones.

RF signal from transmitter to fixed receiver goes along a **path**, that
consists of one or more straight segments. Multisegment paths have repeaters at
segment joints.

Paths are identified by callsign and path number within callsign. Path is
unidirectional, oncoming paths are have different path numbers or even different
callsigns.

Every transmitter/receiver/repeater antenna situated at some **location**.
Location is identified by callsign and location number (location number is not
the same thing as path number). Location may have several antennas, hence
antenna is identified by callsign, location number and antenna number (latter
is within the location).

Transmitter uses one or more **frequency ranges** for transmission. These
frequency ranges may intersect or be contained in one another - AFC only
concerned about resulting (distinct, i.e. non-intersecting) frequency ranges.

Very good overview of 6GHz ULS data may be found in
https://www.wirelessinnovation.org/assets/work_products/Reports/winnf-tr-1008-v1.0.0%206%20ghz%20incumbent%20data%20technical%20report.pdf

ULS information is updated daily and may be downloaded from FCC server in form
of ZIP-files, containing CSV tables (with **|** as field separator). These
ZIP-files referred to as **raw ULS data** in this document. These tables look
like they were conceived as a relational database, but now they contain so much
obvious duplications, omissions and inconsistencies that quite possible that
FCC internally handles them as text files.

AFC server engine uses SQLite file (referred to as **AFC database** in this
document), created from raw ULS data by **AFC ULS download script**. This AFC
database consists of a single table, each row defining a frequency range within
single path (i.e. if path has several frequency ranges, it occupies several
rows).

As of time of this writing AFC database does not support paths that have more
than one repeater or repeaters with distinct RX and TX antennas (albeit such
paths do exist in raw ULS data).

### *uls_tool.py* overview

*uls_tool.py* was originally created as ULS data analysis tool. Eventually it
got an AFC ULS download capabilities too. Still most of its features are not
related to ULS download.

*uls_tool.py* works with the following data formats:
* **Raw ULS data**. Zip archives downloaded from FCC ULS server. Contain
set of CSV files (**|** as field separator) with .dat extension (AN.dat,
EM.dat, etc.)  
    * General information on these files may be found in
https://www.fcc.gov/sites/default/files/pubacc_intro_11122014.pdf
    * Format of these tables documented in
https://www.fcc.gov/sites/default/files/public_access_database_definitions_v6.pdf
    * Values of some enumerated fields, used in these tables may be found in
https://www.fcc.gov/sites/default/files/uls_code_definitions_20201222.txt
    * Database field names and characteristics may be found in
https://www.fcc.gov/sites/default/files/public_access_database_definitions_sql_v3_4.txt
* **AFC database**. SQLite database, used by AFC server. Built from raw ULS data
by ULS downloader. Consists of a single table, each row contains information on
a single frequency range of single path. See *AFC database structure* chapter
for details on this table's structure.
* **ULS database**. uls_tool.py may also works with its own SQLite database. This
multitable database closely reflects the structure of ULS raw data: it consists
of same tables (AN, EM, ...) that have same fields. Viewing this database in
SQLite viewer allows to resolve questions/problems around ULS data (viewing raw
ULS files in, say, Excel is not possible, as these files too large for Excel).  
ULS databases may be used in same contexts as AFC databases: they may be
compared and used for KML (map data) generation. AFC database may be generated
from ULS database.  
ULS database may contain the whole content of raw ULS data tables or just a
6GHz-related subsets. Former takes hours to create (SQLite is rather slow for
writing), 6GHz usually suffice.
* **KML files** (output only). Files with geodetic features that can be loaded
into https://mymaps.google.com to display them against GoogleMaps backdrop
(*gmap* subcommand)
* **CSV files** (output only). Comma separated files that can be viewed e.g. in
Excel. uls_tool.py may put there an excerpt from some raw ULS table (*csv*
subcommand)

## *uls_tool.py* operation
### Prerequisites
*uls_tool.py* is a Python script, it requires Python 3.7 or more recent. Its
shebang line requests *python3*

Also it requires SqlAlchemy installed (`pip install SQLAlchemy`). Tested on
SqlAlchemy 1.4 and works fine, tested on SqlAlchemy and works extremely
slow (as it does not support bulk write), not tested on SqlAlchemy 1.3.  
Here is how to check SqlAlchemy version:  
`$ python3`  
`>>> import sqlalchemy`   
`>>> sqlalchemy.__version__`  
`'1.4.28'`

*uls_tool.py* was tested on Linux and Windows - on both platforms it works
fine. Probably it will work equally well on MacOS.

### Overview of subcommands
|Subcommand|What it does|
|----------|------------|
|help|Print list of subcommands or help on a particular subcommand|
|latest|Print date of latest ULS data on FCC server|
|download_afc|Download AFC database or create it from pre-downloaded raw ULS files|
|download_uls|Download ULS database or create it from pre-downloaded raw ULS files|
|uls_to_afc|Create AFC database from ULS database|
|gmap|Create KML file with paths for displaying it on Google Maps|
|compare|Compare paths in two databases (ULS or AFC)|
|csv|Extract data on given callsign from given table of raw ULS data in form of Excel-compatible .csv file|

### Help
General help (list of subcommands) might be printed in one of following ways:
```
$ uls_tool.py
$ uls_tool.py --help
$ uls_tool.py -h
$ uls_tool.py help
```
Following text is printed:
```
usage: uls_tool.py [-h] SUBCOMMAND ...

ULS download and manipulation tool. V1.0

positional arguments:
  SUBCOMMAND
    latest      Prints date of the latest ULS database on FCC server
    download_uls
                Download ULS data and make ULS database from them
    uls_to_afc  Make AFC database from ULS database
    download_afc
                Download ULS data and make AFC database from them
    gmap        Export KML to import to Google maps
    compare     Compare paths in two ULS or AFC databases
    csv         Download and convert single or consolidated (with daily
                updates) .dat file to Excel-style .csv (fully or for single
                callsign)
    help        Prints help on given subcommand

optional arguments:
  -h, --help    show this help message and exit
```
Help on individual command may be obtained using *help* subcommand (below is
example with *download_afc* subcommand):
```
$ uls_tool.py help download_afc
usage: uls_tool.py download_afc [-h] [--extra] [--mobile] [--progress]
                                [--quiet] [--dir DOWNLOAD_DIRECTORY]
                                [--skip_download] [--retry]
                                [--zip ZIP_FILE_NAME] [--report FILE_NAME]
                                DST_DB

positional arguments:
  DST_DB                Name of file for AFC SQLite database. Overrides if
                        file already exists

optional arguments:
  -h, --help            show this help message and exit
  --extra               Generate extra fields (not in original AFC database)
  --mobile              Allow mobile paths
  --progress            Print download progress information
  --quiet               Do not print messages on nonfatal data
                        inconsistencies
  --dir DOWNLOAD_DIRECTORY
                        Download FCC ULS files archives to given directory
                        and retain them or (--skip_doownload specified) use
                        FCC ULS files from given directory instead of
                        downloading them. Default is to download to temporary
                        directory and remove it afterwards. For development
                        purposes
  --skip_download       Don't download FCC ULS files, use already downloaded,
                        located in directory, specified with --dir switch
                        (that must present). For development purposes
  --retry               Retry download several times if it aborts. For use
                        e.g. on terminally overheated machines
  --zip ZIP_FILE_NAME   Take ULS tables from given single (presumably - full)
                        .zip archive instead of downloading from FCC ULS
                        site. For development purposes
  --report FILE_NAME    Write ULS data inconsistency report to given file
```
Also presence of *--help* in command line prints help on subcommand
independently of other switches. So this command:  
`$ uls_tool.py download_afc some_switches_here --help`  
will cause same printout as above, which might be convenient when name of some
switch is forgotten during typing the command.

### *latest* subcommand
Print FCC local date/time (EST) of latest or pre-downloaded raw ULS data. Takes
around two minutes to complete.

Command line format:  
`$ uls_tool.py latest some_switches_here`  
Command line switches:

|Switch|Function|
|------|--------|
|--inc|Report date of latest daily incremental data (ignoring date of full data)|
|--dir **raw_file_directory**|If *--skip_download* also specified - use raw ULS data from given directory. Otherwise - put downloaded raw ULS data to given directory|
|--skip_download|Use raw ULS data from directory, specified with *--dir* (where to they were previously downloaded)|
|--zip **zip_file**|Use raw ULS data from only given Zip archive (full raw ULS data consists of several such archives - full weekly archive and incremental daily archives)
|--retry|Retry several times on unsuccessful download attempts|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Print EST date/time of latest ULS data on FCC server:  
`$ uls_tool.py latest`  
Sample printout:   
`2022-01-25 08:11:03`

* Ditto, but with progress tracking:  
`$ uls_tool.py latest --progress`  

* Ditto, but with retry on network failure:  
`$ uls_tool.py latest --retry`  

* Download raw ULS data to RAW_ULS_DIR directory (for subsequent reuse):  
`$ uls_tool.py latest --dir RAW_ULS_DIR`

### *download_afc* subcommand
Download raw ULS data and make AFC database from it. Or make AFC database from
pre-downloaded raw ULS data.

Command line format:  
`$ uls_tool.py download_afc some_switches_here AFC_DATABASE.sqlite3`  
Command line switches:

|Switch|Function|
|------|--------|
|--prev_afc **previous_afc**.sqlite3|Continue path *fsid* assignment from given AFC database. Default is to assign *fsid* sequentially from 1|
|--extra|Put some additional columns to generated database. These columns not used by AFC engine, but help in database troubleshooting (see *AFC database structure* chapter)|
|--mobile|Add information on mobile (nonfixed) paths that somehow have fixed receiver antenna coordinates (these paths should not be in AFC database. This switch is for compatibility with existing ULS downloader script that puts mobile paths to AFC database)|
|--report **report_file**|Write to given file a report on encountered raw ULS data inconsistencies|
||Other switches same as in the *latest* subcommand:|
|--dir **raw_file_directory**|If *--skip_download* also specified - use raw ULS data from given directory. Otherwise - put downloaded raw ULS data to given directory|
|--skip_download|Use raw ULS data from directory, specified with *--dir* (where to they were previously downloaded)|
|--zip **zip_file**|Use raw ULS data from only given Zip archive (full raw ULS data consists of several such archives - full weekly archive and incremental daily archives)|
|--retry|Retry several times on unsuccessful download attempts|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Download AFC database, assigning *fsid* sequentially, starting from 1:  
`$ uls_tool.py download_afc AFC_DATABASE.sqlite3`

* Ditto, but continue *fsid*s, starting from previous database:  
`$ uls_tool.py download_afc --prev_afc PREV_DB.sqlite3 AFC_DATABASE.sqlite3`

* Ditto, but also add some extra columns for better data analysis:  
`$ uls_tool.py download_afc --extra AFC_DATABASE.sqlite3`

* Ditto, but without wall of error messages, with retry on network errors and
with fancy progress tracking:  
`$uls_tool.py download_afc --quiet --retry --progress AFC_DATABASE.sqlite3`

* Ditto, also generate report on inconsistencies, found in raw ULS data:  
`$ uls_tool.py download_afc --report REPORT_FILE.txt --quiet \`  
`AFC_DATABASE.sqlite3`

* Create AFC database from raw ULS data, previously downloaded to RAW_ULS_DIR:  
`$ uls_tool.py download_afc --dir RAW_ULS_DIR --skip_download \`  
`AFC_DATABASE.sqlite3`

### *download_uls* subcommand
Download raw ULS data and make ULS database from it. Or make ULS database from
pre-downloaded raw ULS data. Takes from 30 minutes to 3 hours, depending on
*--data* switch.

Command line format:  
`$ uls_tool.py download_uls some_switches_here ULS_DATABASE.sqlite3`  
Command line switches:

|Switch|Function|
|------|--------|
|--data {*all*\|*used*\|*6g*}|Data to include: *all* for all data (takes a couple of hours, includes duplicated lines from original raw ULS data), *used* - data used in some paths, *6g* - data related to existing 6GHz incumbents (this is the default)|
|--strict|Maintain unique primary keys in all tables. This drops duplicates in them, which might be undesirable. Yet, unless *--data all* is specified, unique primary keys are maintained in most tables|
|--overwrite|Overwrite target database if it exists. Default is to stop abnormally|
|--skip_write|Skip database writing operations (to find out how slow SQLite is - quite a lot!)|
||Other switches same as in the *latest* subcommand|
|--dir **raw_file_directory**|If *--skip_download* also specified - use raw ULS data from given directory. Otherwise - put downloaded raw ULS data to given directory|
|--skip_download|Use raw ULS data from directory, specified with *--dir* (where to they were previously downloaded)|
|--zip **zip_file**|Use raw ULS data from only given Zip archive (full raw ULS data consists of several such archives - full weekly archive and incremental daily archives)|
|--retry|Retry several times on unsuccessful download attempts|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Make ULS database with 6GHz data only, watching for progress and not
watching for errors:  
`$ uls_tool.py download_uls --progress --quiet ULS_DATABASE.sqlite3`

* Make database with all data, watching for progress and not watching for
errors:  
`$ uls_tool.py download_uls --progress --quiet --data all ULS_DATABASE.sqlite3`

### *uls_to_afc* subcommand
Creates AFC database from ULS database.

Command line format:  
`$ uls_tool.py uls_to_afc some_switches_here ULS_DATABASE.sqlite3 AFC_DATABASE.sqlite3`  
Command line switches:

|Switch|Function|
|------|--------|
|--prev_afc **previous_afc**.sqlite3|Continue path *fsid* assignment from given database. Default is to assign *fsid* sequentially from 1|
|--extra|Put some additional columns to generated database. These columns not used by AFC engine, but help in database troubleshooting (see *AFC database structure* chapter)|
|--mobile|Add information on mobile (nonfixed) paths that somehow have fixed receiver antenna coordinates (these paths should not be in AFC database. This switch is for compatibility with existing ULS downloader script that puts mobile paths to AFC database)|
|--report **report_file**|Write to given file a report on encountered raw ULS data inconsistencies|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Create AFC database from ULS database, watching for progress and not watching
for errors, *fsid*s are sequential (starting from 1):  
`$ uls_tool.py uls_to_afc --progress --quiet ULS_DATABASE.sqlite3 AFC_DATABASE.sqlite3`

* Ditto, but include mobile paths (to be more compatible with current ULS
downloader implementation):  
`$ uls_tool.py uls_to_afc --progress --quiet --mobile \ `  
`ULS_DATABASE.sqlite3 AFC_DATABASE.sqlite3`


### *gmap* subcommand
Create KML file to display paths on Google Maps.

How to display KML file on Google Maps in browser described here:  
https://support.google.com/mymaps/answer/3024836

KML file have a limit of 2000 features per file, hence the entire US-full of
paths can't be displayed. So *gmap* subcommand allows to put to KML file paths
in the circle vicinity of some point. Circular boundary of the vicinity is also
drawn.

Command line format:  
`$ uls_tool.py gmap some_switches_here AFC_OR_ULS_DATABASE.sqlite3 KML_FILE.kml`  
Command line switches:

|Switch|Function|
|------|--------|
|--lat **latitude**|Latitude in degrees (north positive) of center point of the vicinity|
|--lon **longitude**|Longitude in degrees (east positive) of center point of the vicinity|
|--distance **miles**|Radius of vicinity|
|--direction|Display direction arrows on map (every arrowhead counted as feature, thus decreasing the number of displayable paths)|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Create KML file with paths with direction arrows in vicinity of San Jose, CA:  
`$ uls_tool.py gmap --lat 37.3 --lon -121.8 --distance 85 --direction \`  
`AFC_OR_ULS_DATABASE.sqlite3 PATHS.kml`

* Ditto, but with too big radius:  
`$ uls_tool.py gmap --lat 37.3 --lon -121.8 --distance 100 --direction \`  
`AFC_OR_ULS_DATABASE.sqlite3 PATHS.kml`  
The following error message will be printed:  
`uls_tool.py. WARNING: Number of KML features exceeds 2000. 1435 of 2143 paths written`

### *compare* subcommand
Compare paths from two databases (each may be AFC or ULS). Comparison is
performed only by coordinates and frequency ranges.

Command line format:
`$ uls_tool.py compare some_switches_here AFC_OR_ULS_DATABASE1.sqlite3 \`  
`AFC_OR_ULS_DATABASE2.sqlite3`  
Command line switches:

|Switch|Function|
|------|--------|
|--added **db_for_added**|Database to put added paths to (paths in second but not in first source database)|
|--removed **db_for_removed**|Database to put removed paths to (paths in first but not in second source database)|
|--common **db_for_common**|Database to put common paths to|
|--unique|Print lists of unique path identifiers in each database|
|--spatial **resolution**[;**top_n**]|Print *top_n* locations where the most changes encountered (if omitted - all places are printed). <br/>*resolution* specifies spatial resolution (size of places being compared) in degrees of latitude/longitude
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Compare two databases and print some simple statistics:  
`$ uls_tool.py compare DB1.sqlite3 DB2.sqlite3`  
Sample printout:
```
Paths added: 1094
Paths removed: 1386
Paths common: 105433
```

* Ditto, but with lists of unique path IDs:
`$ uls_tool.py compare --unique DB1.sqlite3 DB2.sqlite3`  
Sample printout:
```
Unique paths in first database: KA88834/1, KA88882/1, KA88915/1, ...<very long list>
Unique paths in second database: KBV58/18, KBV58/19, KCG74/28, ...<also very long list>
Paths added: 1094
Paths removed: 1386
Paths common: 105433
```

* Ditto, but with spatial distribution of differences (top ten places, two
degrees resolution):  
`$ uls_tool.py compare --spatial 2;10 DB1.sqlite3 DB2.sqlite3`  
Sample printout:
```
Paths added: 1258
Paths removed: 453
Paths common: 105612
Changes around 30.0N, 96.0W: 63
Changes around 32.0N, 96.0W: 63
Changes around 38.0N, 122.0W: 55
Changes around 40.0N, 122.0W: 47
Changes around 34.0N, 118.0W: 42
Changes around 34.0N, 112.0W: 36
Changes around 42.0N, 122.0W: 34
Changes around 40.0N, 76.0W: 33
Changes around 40.0N, 74.0W: 33
Changes around 32.0N, 92.0W: 33
```

* Ditto, but retrieve differences into AFC-style databases:  
`$ uls_tool.py compare --removed SPECIFIC_FOR_DB1.sqlite3 \`  
`--added SPECIFIC_FOR_DB2.sqlite DB1.sqlite3 DB2.sqlite3`  

### *csv* subcommand
From given raw ULS data table (on FCC server or pre-downloaded) retrieve rows,
pertinent to given callsign(s) and put them to Excel-compatible .csv file.

Command line format:
`$ uls_tool.py csv some_switches_here FILE.csv `  
Command line switches:

|Switch|Function|
|------|--------|
|--callsign **callsign**|Callsign to retrieve information about. Several may be specified. If omitted - entire table is retrieved|
|--table **table**|Two-letter table name (AN, EM...)|
|--day {*full*\|*mon*\|*tue*\|...}|Specific dataset to retrieve from - full weekly or some daily update|
||Other switches same as in the *latest* subcommand|
|--dir **raw_file_directory**|If *--skip_download* also specified - use raw ULS data from given directory. Otherwise - put downloaded raw ULS data to given directory|
|--skip_download|Use raw ULS data from directory, specified with *--dir* (where to they were previously downloaded)|
|--zip **zip_file**|Use raw ULS data from only given Zip archive (full raw ULS data consists of several such archives - full weekly archive and incremental daily archives)|
|--retry|Retry several times on unsuccessful download attempts|
|--progress|Print fancy countups and time measurements|
|--quiet|Do not print nonfatal warnings|

Some examples:

* Latest data on callsign ZUZZEL57 from EM table:  
`$ uls_tool.py csv --table EM --callsign ZUZZEL57 FILE.csv`

* Ditto, but from pre-downloaded raw ULS files:  
`$ uls_tool.py csv --table EM --dir RAW_ULS_DIR --skip_download --callsign ZUZZEL57 FILE.csv`

* Ditto, but only Tuesday updates:  
`$ uls_tool.py csv --table EM --day tue --callsign ZUZZEL57 FILE.csv`

## Database structures

Both AFC and ULS databases are SQLite 3 databases. Such database is just a
file, containing a bunch of tables. Database may be viewed with SQlite viewer -
there are many of them.

*DB Browser for SQLite* ( https://sqlitebrowser.org/ ) is very good free SQLite
browser for Windows. Besides standalone use it can be set up as Explorer
extension and default opener (associated with .sqlite3 files) - site contains
an instruction on how to do it. It is not as comfortable as Excel, but quite
sufficient for data analysis.

### AFC database structure

Here is the structure of AFC database, generated by Open AFC ULS downloader.
Columns used by AFC computation engine (as of time of this writing) shown in
**bold**.

|Field|Description|
|-----|-----------|
|id|Line number in the table|
|**fsid**|Path identifier. Assigned sequentially, continuing numbering from previous AFC database|
|**callsign**|Path callsign|
|status|License status (*License Status* field from *HD* table of raw ULS data). Only paths with **A** and **L** (Active and Pending respectively) are admitted to AFC database| 
|**radio_service**|*Radio Service Code* from *HD* table of raw ULS data - whatever is it|
|**name**|Licensee entity name (*Entity Name* from *EN* table of raw ULS data)|
|common_carrier|*Common Carrier* field from *HD* table of raw ULS data. As of time of this writing NULL in all paths of AFC database|
|mobile|*Mobile* field from *HD* table converted from string to boolean. As of time of this writing NULL in all paths. Real mobile/fixed distinguishers are *Path Type Code* from *PA* table and *Location Class Code* from *LO* table (as of time of this writing, these fields are in agreement, but latter is preferable, as it a documented enumeration, whereas former is a free-form text - albeit more conveniently located). As for time of this writing AFC database does contain a bunch of mobile paths, albeit it should not|
|**rx_callsign**|Receiver callsign. May be different from path callsign|
|**rx_antenna_num**|*Antenna number* from *AN* table. Utility is unknown, as it is only meaningful in pair with location number|
|**freq_assigned_start_mhz**|Start of frequency range|
|**freq_assigned_end_mhz**|End of frequency range|
|**tx_eirp**|Transmitter EIRP power in dBm|
|**emission_des**|Emission designator. See https://fccid.io/Emissions-Designator/ for structure|
|**tx_lat_deg**|Transmitter WGS84 latitude in signed degrees (North is positive)|
|**tx_long_deg**|Transmitter WGS84 longitude in signed degrees (East is positive)|
|**tx_ground_elev_m**|Terrain elevation above WGS84 at transmitter location|
|**tx_polarization**|Transmitter antenna polarization (*H*, *V* or number)|
|**tx_height_to_center_raat_m**|Height of transmitter antenna center above ground in meters|
|**tx_gain**|Transmitter antenna gain|
|**rx_lat_deg**|Receiver WGS84 latitude in signed degrees (North is positive)|
|**rx_long_deg**|Receiver WGS84 longitude in signed degrees (East is positive)|
|**rx_ground_elev_m**|Terrain elevation above WGS84 at receiver location|
|**rx_ant_model**|Receiver antenna model|
|**rx_height_to_center_raat_m**|Height of receiver antenna center above ground in meters|
|**rx_gain**|Receiver antenna gain|
|**p_rx_indicator**|"Y" if path has repeaters ("N" or NULL otherwise)|
|**p_rp_lat_degs**|Repeater WGS84 latitude in signed degrees (North is positive)|
|**p_rp_long_degs**|Repeater WGS84 longitude in signed degrees (East is positive)|
|**p_rp_height_to_center_raat_m**|Height of repeater antenna center (receiving? transmitting?) above ground in meters|
|path_number|Path number. It's strange that AFC server does not use it (thus relying on ambiguous *fsid* in its diagnostics messages)|

Additional fields, added when *--extra* command line switch specified:  

|Field|Description|
|-----|-----------|
|seg_count|Number of segments (number of repeaters plus 1)|
|grant_date|License grant date. *Grant Date* from *HD* table|
|expired_date|License expiration date. Often not agree with *License Status* - i.e. path may be marked as "A" (having active license), but its license is many years since expired|
|cancellation_date|License cancellation date (NULL for all 6 GHz paths so far)|
|path_type_desc|*Path Type Code* from *PA* table. Distinguishes mobile and fixed paths|

### ULS database structure

ULS database consists of ULS raw data tables used by AFC (AN, CP, EM, EN,
FR, HD, LO, PA, SG - see
https://www.fcc.gov/sites/default/files/public_access_database_definitions_v6.pdf)
put to database (for easy access), wrapped into SQLite. It is a  multitable
database (each table corresponds to one of raw ULS tables).

Fields are named according to
https://www.fcc.gov/sites/default/files/public_access_database_definitions_v6.pdf
except that *callsign* in all ULS database tables named *callsign* (whereas
raw ULS data named it differently in different tables)

When *--data all* specified and *--strict* not specified ULS data loaded as is,
including duplicates.

## uls_tool.py structure

This is brief overview of top-level objects in uls_tools.py:

|Object|Responsibility|
|------|--------------|
|VERSION|Minor number changed on bugfixes and enhancements, major number changed on new features and externally observable behavior/interface changes|
|dp()|Debug print routine|
|ob(), of(), oi(),<br/>ost(), od()|MyPy appeasement. Ensure that argument is of *Optional[\<SomeType\>]* and return this argument. *\<SomeType\>* is *bool*, *float*, *int*, *str*, *datetime.datetime* respectively|
|mb(), mf(), mi(),<br/>mst(), md()|MyPy appeasement. Ensure that argument is of *\<SomeType\>* and return this argument. *\<SomeType\>* is *bool*, *float*, *int*, *str*, *datetime.datetime* respectively|
|error(), error_if()|Print message and exit script - unconditionally or conditionally|
|class Progressor|Print progress value on the same line (i.e. with \\r as line terminator)|
|class Timing|Measures and prints timing of long operations|
|class FreqRange|Transmit frequency range|
|class ArgContainer|Container of function arguments (keyword and positional). Allows to apply them to function and query selectively. Used for instantiation of SqlAlchemy objects|
|class UlsTable|Structure definitions of ULS raw data/ULS database tables plus some simple functionality around|
|class Complainer|Manages complaints on ULS raw data imperfections: prints complaints as they come and creates sorted report on complaints afterwards (if *--report* switch is specified|
|class Complaint\*|Individual complaint classes (in report Complainer orders complaints first by class then by sort keys, defined in classes)|
|class GeoCalc|Geodetic calculations (distance, intersection, etc.), used by KmlWriter|
|class PathInfo|Data on a single path - the unit of information exchange between various classes|
|class PathFreqBucket|Collection of paths with same callsign and path number but different frequency ranges (merges intersecting ranges))|
|class PathValidator|Checks if given path deserves going to AFC database or should be reported and dropped|
|class DbPathIterator|Iterates over paths stored in ULS or AFC database|
|class FsidGenerator|Generates *fsid* field values|
|class AfcWriter|Writes paths to AFC database|
|class Downloader|Downloads from FCC server and unpacks raw ULS data|
|class UlsFileReader|Reader of single raw ULS table file|
|class UlsTableReader|Reader of raw ULS table (weekly full plus patches)|
|class UlsDatabaseFiller|Collection of utilities to fill ULS database. Orchestration is performed from do_download() - not the best idea, but let it be|
|class DownloadedPathIterator|Iterate over paths, stored in raw ULS data tables|
|class KmlWriter|Writes paths to KML file (for displaying in GoogleMaps)|
|do_\*()|Executor of subcommands (e.g. *do_download()* executes *download* subcommand)|
|main()|Parses command line parameters, sets up logging|