Copyright (C) 2022 Broadcom. All rights reserved.  
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# FS Database downloader service

## Table of contents
- [FS Downloading overview](#overview)
- [On importance of FS Database continuity](#continuity)
- [FS Downloader service script](#service)
- [Service healthcheck](#healthcheck)
- [Service state](#service_state)
  - [State database](#state_database)
  - [Prometheus metrics](#metrics)
- [Troubleshooting](#troubleshooting)
  - [Results of last download](#results_location)
  - [Redownload](#redownload)
  - [`fs_db_diff.py` FS Database comparison tool](#fs_db_diff)
  - [`fs_afc.py` FS Database test tool](#fs_afc)
  - [`fsid_tool.py` FSID extraction/embedding tool](#fsid_tool)


## FS Downloading overview <a name="overview"/>

The purpose of AFC service is to avoid interference of SP APs with existing **Fixed Service** (**FS**) microwave RF transmission networks.

To avoid this interference AFC system needs information about these FS networks (location of FS receivers and transmitters, their antenna parameters, etc.). This information is (supposed to be) maintained by regulatory authorities - such as FCC in US, etc. - in some online databases.

FS Downloader service downloads information from these disparate sources and puts it to **FS Database** - *SQLITE3* file that is subsequently used by AFC Engine to make AFC computations.

For historical reasons (first supported authority was FCC and there this database named **ULS**), FS database may be (and usually is) interchangeably named **ULS Database**, FS Downloader - **ULS Downloader**, etc.

FS Downloader itself (as of time of this writing - `src/ratapi/ratapi/daily_uls_parse.py`) is being developed in its own opaque way, of which author of this text knows nothing (even after the fact - not a lot), hence it will not be documented here.

This text documents a bunch of scripts that wrap FS Downloader to make a service inside AFC.


## On importance of FS Database continuity <a name="continuity">

While performing FS Downloader maintenance and troubleshooting, it is very important to keep in mind a subtle but very important aspect of **FS Database continuity**.

**FS Paths** (FS receivers, transmitters and receivers that define path of RF signal) have objective unique names (callsign/id/frequency). However these names are **not** used by AFC Engine. Instead, AFC Engine refers to FS Paths via **FSID**s, sequentially assigned by FS downloader script. Don't ask why.

To maintain FSID uniqueness, FS downloader script maintains a table of already used FSIDs. Between FS download script invocation this table is stored inside FS Database.

Hence, essentially, FSID table for new FS Database is based on one, stored in previous database. If there is no previous database - FSIDs are restarted from 0. This is not a catastrophe, but may become a major nuisance in subsequent troubleshooting.

Hence one should always strive to maintain FS Database lineage, deriving new one from previous (usually pointed to by a symlink, passed to `uls_service.py` via `--ext_db_dir` and `--ext_db_symlink` parameters.


## FS Downloader service script <a name="service"/>

Service implemented in `uls/uls_service.py` script that has the following functionality:
- Downloads FS Database by means of *daily_uls_parse.py*
- Checks database integrity. Currently the following integrity checks implemented:
  - How much database changed since previous time
  - If it can be used for AFC Computations without errors
- If integrity check passed and database differs from previous - copies it to destination directory and retargets the symlink that points to current FS Database. This symlink is used by rest of AFC to work with FS data
- Notifies Rcache service on where there were changes - to invalidate affected cached responses and initiate precomputation

`uls/uls_service.py` is controlled by command line parameters, most of which can be passed via environment variables. Most of default values defined in *uls_service.py*, but some defined in *Dockerfile-uls_service*). **It is recommended to run this script without parameters** (some exception can be found in [Troubleshooting](#troubleshooting) section.


soTable below summarizes these parameters and environment variables:

|Parameter|Environment variable|Default|Comment|
|---------|-----------------------|-------|-------|
|--download_script **SCRIPT**|ULS_DOWNLOAD_SCRIPT|/mnt/nfs/rat_transfer/<br>daily_uls_parse/<br>daily_uls_parse.py|Download script|
|--download_script_args **ARGS**|ULS_DOWNLOAD_SCRIPT_ARGS||Additional (besides *--region*) arguments for *daily_uls_parse.py*|
|--region **REG1:REG2...**|ULS_DOWNLOAD_REGION||Colon-separated list of country codes (such as US, CA, BR) to download. Default to download all supported countries|
|--result_dir **DIR**|ULS_RESULT_DIR|/mnt/nfs/rat_transfer/<br>ULS_Database/|Directory where to *daily_uls_parse.py* puts downloaded database|
|--temp_dir **DIR**|ULS_TEMP_DIR|/mnt/nfs/rat_transfer/<br>daily_uls_parse/temp/|Directory where to *daily_uls_parse.py* puts temporary files|
|--ext_db_dir **DIR**|ULS_EXT_DB_DIR|/*Defined in dockerfile*|Directory where to resulting database should be placed (copied from script's result directory). May be only initial part of path, rest being in `--ext_db_symlink`|
|--ext_db_symlink **FILENAME**|ULS_CURRENT_DB_SYMLINK|rat_transfer/ULS_Database/<br>FS_LATEST.sqlite3<br>*Defined in dockerfile*|Name of symlink in resulting directory that should be retargeted at newly downloaded database. May contain path - same that should be used in AFC Config|
|--fsid_file **FILEPATH**|ULS_FSID_FILE|/mnt/nfs/rat_transfer/<br>daily_uls_parse/<br>data_files/fsid_table.csv|Full name of file with existing FSIDs, used by *daily_uls_parse.py*. Between downloads this data is stored in FS Database|
|--ext_ras_database **FILENAME**|ULS_EXT_RAS_DATABASE|rat_transfer/RAS_Database/<br>RASdatabase.dat<br>*Defined in dockerfile*|Name of externally maintained 'RAS database' (.csv file with restricted areas)|
|--ras_database **FILENAME**|ULS_RAS_DATABASE|/mnt/nfs/rat_transfer<br>/daily_uls_parse/data_files<br>/RASdatabase.dat|Where from *daily_uls_parse.py* reads RAS database|
|--service_state_db_dsn<br>**CONNECTION_STRING**|ULS_SERVICE_STATE_DB_DSN||Connection string to state database. It contains FS service state that is used by healthcheck script. This parameter is mandatory|
|--service_state_db_create_if_absent|ULS_SERVICE_STATE_DB_CREATE_IF_ABSENT|True|Create state database if absent|
|--service_state_db_recreate|ULS_SERVICE_STATE_DB_RECREATE|False|Recreate state database if it exists|
|--prometheus_port **PORT**|ULS_PROMETHEUS_PORT||Port to serve Prometheus metrics on (default is to not serve)|
|--statsd_server **HOST[:PORT]**|ULS_STATSD_SERVER||StatsD server to send metrics to. Default is to not send|
|--check_ext_files **BASE_URL:SUBDIR:FILENAME[,FILENAME...][;...**|ULS_CHECK_EXT_FILES|"https://raw.githubusercontent.com<br>/Wireless-Innovation-Forum/<br>6-GHz-AFC/main/data/common_data:<br>raw_wireless_innovation_forum_files:<br>antenna_model_diameter_gain.csv,<br>billboard_reflector.csv,<br>category_b1_antennas.csv,<br>high_performance_antennas.csv,<br>fcc_fixed_service_channelization.csv,<br>transmit_radio_unit_architecture.csv|Certain files used by *daily_uls_parse.py* should be identical to certain files on the Internet. Comparison performed by *uls_service.py*, this parameter specifies what to compare. Several such group may be specified semicolon-separated. This parameter may be specified several times and currently hardcoded in *uls/Dockerfile-uls_service*|
|--max_change_percent **PERCENT**|ULS_MAX_CHANGE_PERCENT|10|Downloaded FS Database fails integrity check if it differs from previous by more than this percent of number of paths. If absent/empty - this check is not performed|
|--afc_url **URL**|ULS_AFC_URL||REST API URL (in *rat_server*/*msghnd*) to use for AFC computation with custom FS database as part of AFS Database integrity check. If absent/empty - this check is not performed| 
|--afc_parallel **N**|ULS_AFC_PARALLEL|1|Number of parallel AFC Requests to schedule while testing FS Database on *rat_server*/*msghnd*|
|--rcache_url **URL**|RCACHE_SERVICE_URL||Rcache REST API top level URL - for invalidation of areas with changed FSs. If empty/unspecified (or if Rcache is not enabled) - see below) - no invalidation is performed|
|--rcache_enabled [**TRUE/FALSE**]|RCACHE_ENABLED|TRUE|TRUE if Rcache enabled, FALSE if not|
|--delay_hr **HOURS**|ULS_DELAY_HR|0|Delay (in hours) before first download attempt. Makes sense to be nonzero in regression testing, to avoid overloading system with unrelated stuff. Ignored if --run_once specified|
|--interval_hr **HOURS**|ULS_INTERVAL_HR|4|Interval in hours between download attempts|
|--timeout_hr **HOURS**|ULS_TIMEOUT_HR|1|Timeout in hours for *daily_uls_parse.py*|
|--nice|ULS_NICE|*Defined in Dockerfile*|Execute download on lowered priority. Values for environment variable: TRUE/FALSE|
|--run_once|ULS_RUN_ONCE||Run once (default is to run periodically). Values for environment variable: TRUE/FALSE|
|--verbose|||Print more detailed (for debug purposes)|
|--force|||Force FS database update even if it is not changed and bypassing database validity checks (e.g. to overrule them)|


## Service healthcheck <a name="healthcheck"/>

Being bona fide docker service, ULS downloader has a healthcheck procedure. It is implemented by `uls/service_healthcheck.py` script that has the following functionality:

- Pronounces container healthy or not
- May send **alarm** emails on what's wrong
- May send periodical **beacon** emails on overall status (even when no problems detected)

Healthcheck script makes its conclusion based on when there was last successful something (such as download attempt, download success, database update, data change, etc.) This information is left by service script in status (aka state) directory.

Healthcheck script invocation periodicity, timeout etc. is controlled by parameters in `HEALTHCHECK` clause of *uls/Dockerfile-uls_service*. Looks like they can't be parameterized for setting from outside, so they are hardcoded.

*service_healthcheck.py* is controlled by command line parameters, that can be set via environment variables.  **It is recommended to run this script without parameters**

Environment variables starting with *ULS_HEALTH_* control pronouncing container health, starting with *ULS_ALARM_* control sending emails (both alarm and beacon).

Parameters are:

|Parameter|Environment variable|Default|Comment|
|---------|-----------------------|-------|-------|
|--service_state_db_dsn<br>**CONNECTION_STRING**|ULS_SERVICE_STATE_DB_DSN||Connection string to state database. This parameter is mandatory|
|--download_attempt_max_age_health_hr **HR**|ULS_HEALTH_ATTEMPT_MAX_AGE_HR|6|Age of last download attempt in hours, enough to pronounce container as unhealthy|
|--download_success_max_age_health_hr **HR**|ULS_HEALTH_SUCCESS_MAX_AGE_HR|8|Age of last successful download in hours, enough to pronounce container as unhealthy|
|--update_max_age_health_hr **HR**|ULS_HEALTH_UPDATE_MAX_AGE_HR|40|Age of last FS data change in hours enough to pronounce container as unhealthy|
|--smtp_info **FILEPATH**|ULS_ALARM_SMTP_INFO||Name of secret file with SMTP credentials for sending emails. If empty/unspecified no emails being sent|
|--email_to **EMAIL**|ULS_ALARM_EMAIL_TO||Whom to send alarm email. If empty/unspecified no emails being sent|
|--info_email_to **EMAIL**|ULS_BEACON_EMAIL_TO||Whom to send beacon information email. If empty/unspecified uses alarm email|
|--email_sender_location **COMMENT**|ULS_ALARM_SENDER_LOCATION||Note on sending service location to use in email. Optional|
|--alarm_email_interval_hr **HR**|ULS_ALARM_ALARM_INTERVAL_HR||Minimum interval in hours between alarm emails (emails on something that went wrong). If empty/unspecified no alarm emails being sent|
|--download_attempt_max_age_alarm_hr **HR**|ULS_ALARM_ATTEMPT_MAX_AGE_HR||Minimum age in hours of last download attempt to send alarm email. If empty/unspecified - not checked|
|--download_success_max_age_alarm_hr **HR**|ULS_ALARM_SUCCESS_MAX_AGE_HR||Minimum age in hours of last successful download attempt to send alarm email. If empty/unspecified - not checked|
|--region_update_max_age_alarm **REG1:HR1,REG2:HR2...**|ULS_ALARM_REG_UPD_MAX_AGE_HR||Per-region (`US`, `CA`, `BR`...) minimum age of last data change to send alarm email. Not checked for unspecified countries|
|--beacon_email_interval_hr **HR**|ULS_ALARM_BEACON_INTERVAL_HR||Interval in hours between beacon emails (emails that contain status information even if everything goes well). If empty/unspecified - no beacon emails is being sent|
|--verbose||Print mode detailed information (for debug purposes)|
|--force_success||Return success exit code even if errors were found (used when healthcheck script called from `--run_once` service script - only to send alarm/beacon emails)|
|--print_email||Print email instead of sending it (for debug purposes)|

## Service state <a name="service_state">

`uls_service.py` provides its state (what and when was accomplished, what problems were encountered) to two places: state database (for use by healthcheck script that pronounces container health status and may send alarm/beacon emails) and, optionally, to Prometheus (that may provide this state to Grafana for nice display and/or use its alarm facility to notify about problems).

So there are two facilities that may send alarms (healthcheck script and Prometheus). As of time of this writing Prometheus alarms not yet ready, so it is healthcheck script that is currently responsible for alarms - but ultimately it will change.

State database contains more information than needed for pronouncement service as health or sending alarm. It is intended to be a source of information for investigation of problem reasons.

### State database <a name="state_database">

State database stored PostgreSQL server. Since it is disposable, it is assumed to be *bulk_postgres* server. As of time of this writing, database name is *fs_state*. This database has the following tables:

#### `milestone` table

Contains dates when milestones were last passed. There are following milestones:

|Milestone|Meaning|
|---------|-------|
|ServiceBirth|`uls_service.py` started for the first time (since state database creation)|
|ServiceStart|`uls_service.py` started|
|DownloadStart|FS download script started|
|DownloadSuccess|FS download script completed successfully|
|RegionChanged|Region (country) FS data changed. This milestone is per-region|
|DbUpdated|FS database file successfully updated (after being downloaded, found to be different from previous, successfully passed checks)|
|ExtParamsChecked|External parameter files successfully checked|
|Healthcheck|Healthcheck script ran|
|BeaconSent|Beacon email sent|
|AlarmSent|Alarm email sent|

Table structure:

|Column|Type|Content|
|------|----|-------|
|milestone|enum|Milestone name (see table above)|
|region|string or null|Region name for region-specific milestones, null for region-inspecific milestones|
|timestamp|datetime|When milestone was last reached|

#### `checks` table

Contains information about passed checks. As of time of this writing there are following types of check:

|Check Type|Meaning|Check Item|
|----------|-------|----------|
|ExtParams|Files that should be in sync with their external prototypes|File name|
|FsDatabase|FS Database validity tests|Individual tests|

Table structure:

|Column|Type|Content|
|------|----|-------|
|check_type|enum|Type of check (see table above)|
|check_item|string|Individual check item (see table above)|
|errmsg|string or null|Null if check passed, error message if not|
|timestamp|datetime|When check was performed|

#### `logs` table

Contains one last log of FS script and subsequent actions for each log type. There are following log types

|Log Type|Meaning|
|--------|-------|
|Last|Log of last FS download run|
|LastFailed|Log of last failed FS download|
|LastCompleted|Log of last successful FS download|

Table structure:

|Column|Type|Content|
|------|----|-------|
|log_type|enum|Log type (see table above)|
|text|string|Log text|
|timestamp|datetime|When log was collected|

#### `alarms` table

Contains set of alarms sent in most recent alarm message. There are following types of alarms:

|Alarm type|Meaning|Reason column|
|----------|-------|-------------|
|MissingMilestone|Milestone was not reached for too long|Milestone name (see chapter on milestones above)|
|FailedCheck|Failed check|Type of failed check (see chapter on checks above)|

Table structure:

|Column|Type|Content|
|------|----|-------|
|alarm_type|enum|Alarm type (see table above)|
|alarm_reason|string|Alarm reason (see table above)|
|timestamp|datetime|When alarm was sent

### Prometheus metrics <a name="metrics">

When FS downloader service runs continuously, it may serve Prometheus metrics in a normal way (at `/metrics` over HTTP through specified port). However service is executed in run-once mode - this is not possible, in this case it may send metrics to some push gateway, of which StatsD was arbitrarily chosen (as it also might be used by Gunicorn). Which one (if any) is chosen is configured through `--prometheus_port` and `--statsd_server`. Here are metrics being served:

All metrics are gauges. Time metrics contain seconds since the epoch (January 1, 1970, UTC)

|Metric|Label|Meaning|
|------|-----|-------|
|fs_download_started||Seconds since the epoch of last time FS downloader script started|
|fs_download_succeeded||Seconds since the epoch of last time FS downloader script succeeded|
|fs_download_database_updated||Seconds since the epoch of last time FS database file was updated|
|fs_download_region_changed|region|Seconds since the epoch of last time region data was updated|
|fs_download_check_passed|check_type|1 if last time check of given type was succeeded, 0 otherwise|

Since StatsD does not support labels, hey are appended to metric names (e.g. `fs_download_region_changed_US` instead of `fs_download_region_changed{region="US")`).


## Troubleshooting <a name="troubleshooting"/>

Whenever something goes wrong with ULS download (e.g. log shows some errors, alarm email receiving, general desire to tease author of this text), one can take a look into container log to see where things went wrong and then 'exec -it sh' into FS downloader container to make further investigations.

All scripts mentioned below (and above) are located in */wd* directory in container (*uls* directory in sources).

**And don't forget to read a chapter on [FS Database continuity](#continuity).**

### Results of last download <a name="results_location">

- Underlying FS Downloader script: **src/ratapi/ratapi/daily_uls_parse.py**
- *daily_uls_parse.py*'s temporary files from last download: **/mnt/nfs/rat_transfer/daily_uls_parse/temp/**  
   Cleaned immediately before next download attempt
- *daily_uls_parse.py*'s results directory: **/mnt/nfs/rat_transfer/ULS_Database/**  
  In particular, this directory contains recently downloaded FS database, even if it had not pass integrity check.  
  Cleaned immediately before next download attempt

### Redownload <a name="redownload">

In FS downloader container issue command:  
`/wd/uls_service.py --force --run_once`

### `fs_db_diff.py` FS Database comparison tool <a name="fs_db_diff">

`/wd/fs_db_diff.py` (`uls/fs_db_diff.py` in sources) compares two FS databases.

This script is used by *uls_service.py* for integrity checking (to make sure that amount of change not exceed certain threshold), it also can be used standalone for troubleshooting purposes (e.g. to see what exactly changed if amount of change is too big).

FS Database is a set of RF transmission paths over fixed networks, so comparison is possible by:

- What paths are different
- In what locations paths are different

General format of this script invocation is:

`$ ./fs_db_diff.py [--report_tiles] [--report_paths] FS_DATABASE1 FS_DATABASE2`

This script prints how many paths each database has and how many of them are different. Besides:
- If *--report_tiles* parameter specified, list of 1x1 degree tiles containing difference is printed
- If *--report_tiles* parameter specified, for each path with difference a detailed report (in what field difference was found) is printed.


### `fs_afc.py` FS Database test tool <a name="fs_afc">

`/wd/fs_afc.py` (`uls/fs_afc.py` in sources) tests FS Database by making AFC computations on some points in some countries. Result is unimportant, but it should be computed without errors.

This script is used by *uls_service.py* for integrity checking (to make sure that all points successfully computed), it also can be used standalone for troubleshooting purposes (e.g. to see what what's wrong with points, for which result was not computed).

Points, countries and AFC Request pattern are specified in `/wd/fs_afc.yaml` (`uls/fs_afc.yaml` in sources) config file. It's structure will not be reviewed here - hope it is self evident enough.

General format of this script invocation is:

`$ ./fs_afc.py --server_url $ULS_AFC_URL [PARAMETERS] FS_DATABASE`  
Here `FS_DATABASE` may have path - same as should be used in AFC Config.

Parameters are:

|Parameter|Comment|
|---------|-------|
|--config **FILENAME**|Config file name. Default is file with same name and in same directory as this script, but with *.yaml* extension|
|--server_url **URL**|REST API URL for submitting AFC requests with custom FS Database. Same as specified by *--afc_url* to *uls_service.py*, hence may be taken from *ULS_AFC_URL* environment variable. This parameter is mandatory|
|--region **COUNTRY_CODE**|Country code (such as US, CA, BR...) for of points to run computation for. This parameter may be specified several times. Default is to run for all points in all countries, defined in the config file|
|--parallel **NUMBER**|Number of parallel AFC requests to issue. Default is defined in config file (3 as of time of this writing)|
|--timeout_min **MINUTES**|AFC request timeout in minutes. Default is defined in config file (5 minutes as of time of this writing)|
|--point **COORD_OR_NAME**|Point to make AFC Computation for. Specified either as *LAT,LON* (latitude in north-positive degrees, longitude in east-positive degrees), or as point name (in config file each point has name). Default is to run for all points defined in all (or selected by *--region* parameter) countries|
|--failed_json **FILENAME**|File name for JSON of failed AFC Request. May be subsequently used with curl|

### `fsid_tool.py` FSID extraction/embedding tool <a name="fsid_tool">

As it was explained in chapter on [FS Database continuity](#continuity), FSID table is stored in FS database. `wd/fsid_tool.py` (`uls/fsid_tool.py` in sources) script allows to extract it from FS Database to CSV and embed it from CSV to FS Database.

General format of this script invocation:

`./fsid_tool.py extract|embed [--partial] FS_DATABASE CSV_FILE`

Here *extract* subcommand extracts FSID table from FS Database to CSV file, whereas *embed* subcommand embeds FSID table from CSV file to FS Database.

*--partial* allows for unexpected column names in FS Database or CSV file (which is normally an error).
