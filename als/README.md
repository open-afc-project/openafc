Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Tools For Working With Log Databases

## Table of Contents
- [Databases ](#databases)
  - [`ALS` Database ](#als_database)
    - [Partitioning](#als_partitioning)
    - [Updating database schema](#als_schema_update)
  - [`AFC_LOGS` database ](#afc_logs_database)
- [`als_siphon.py` - moving logs from Kafka to Postgres](#als_siphon_py)
- [`als_query.py` - querying logs from Postgres databases](#als_query_py)
  - [Installation/invocation](#als_query_install)
  - [Connection strings](#als_query_dsn)
  - [Time and time zones](#als_query_time)
  - [`log` subcommand ](#als_query_log)
    - [`log` subcommand examples ](#als_query_log_examples)
  - [`als` subcommand ](#als_query_als)
    - [`als` subcommand examples ](#als_query_als_examples)
  - [`timezones` subcommand ](#als_query_timezones)
  - [`help` subcommand](#als_query_help)
- [`als_db_tool.py` script](#als_db_tool)
  - [`prepare_sql` subcommand](#als_db_tool_prepare_sql)
  - [`add_partitions` subcommand](#als_db_tool_add_partitions)
  - [`partition_info` subcommand](#als_db_tool_partition_info)
  - [`remove_partitions` subcommand](#als_db_tool_remove_partitions)

## Databases <a name="databases">

ALS (AFC Log Storage) functionality revolves around two PostgreSQL databases, used for log storage: `ALS` and `AFC_LOGS`.

### `ALS` database <a name="als_database">

Stores log of AFC Request/Response/Config data. Has rather convoluted multitable structure (see `als/als_db_schema/ALS.PNG`)

#### Partitioning <a name="als_partitioning">

Since ALS database stores every ALS request and response, over time it may become rather large. To avoid its unlimited grouth and simplifying deletion of older data, ALS database is partitioned - each table is stored in per-month partitions.

Partitions for future months should be preacreated ahead of time. `als_siphon` container (aka `als-siphon` pod) on each (re)start ensures that there partitions for **`AFC_ALS_MONTH_PARTITIONS_AHEAD`** (environment variable) future months are created.

Removal of old partitions is a manual operation - see `als_db_tool.py remove_partitions ...` described later.

#### Updating database schema <a name="als_schema_update">

ALS database schema described in `als/als_db_schema/ALS.dbml` - this is a primary source data. Its updating involves following steps:

1. On [https://dbdiagram.io](https://dbdiagram.io) press ***Go To App*** button and copypaste contents of `ALS.dbml` into left pane.
2. Make changes, arrange right-side schema picture nicely.
3. Copyaste changed schema back to `als/als_db_schema/ALS.dbml`
4. ***Export / To PNG*** updated schema picture to `als/als_db_schema/ALS.png`
5. ***Export / To PostgreSQL*** updated schema to `als/als_db_schema/ALS.sql`.  
Please note that resulting `.sql` file is not partitioned - partitions are added on the fly on database creation on `als_siphon` container start
6. Add Alembic migration script to `als/als_migrations/versions`

### `AFC_LOGS` database <a name="afc_logs_database">

For each JSON log type (*topic* on Kafka parlance) this database has separate table with following columns:

- `time` Log record timetag in UTC timezone

- `source` String that uniquely identifies entity that created log record

- `log` JSON log record.

## `als_siphon.py` - moving logs from Kafka to Postgres<a name="als_siphon_py">

Various AFC Server components write ALS and JSON logs to Kafka queues using `als.py`. `als_siphon.py`, running on `als_siphon` container/pod fetches log records from Kafka and transfers them to previously described PostgreSQL databases. It also can create these databases on startup if they are not already exist.

`$ als_siphon.py SUBCOMMAND PARAMETERS`

Subcommands are:

- `init` Creates `ALS` and/or `AFC_LOGS` database. If already exists, databases may be recreated or left intact.

- `siphon` Transferring log records from Kafka to PostgreSQL.

- `init_siphon` First creates databases then do the siphoning - it is this subcommand that runs in `als_siphon` container/pod.

Parameters are many - see help messages. They are set in `Dockerfile.siphon` from environment variables (see comments in this file) that, in turn may be set/changed in `docker-compose.yaml` (Compose configuration) or in `helm/afc-int/values.yaml` or its overrides (see comments in `helm/afc-int/values.yaml`).

Basically, once everything set up, this script is intended to work without intervention.


## `als_query.py` - querying logs from Postgres databases <a name="als_query_py">

This script queries logs, stored in `ALS` and `AFC_LOGS` databases.


### Installation/invocation <a name="als_query_install">

`als_query.py` requires Python 3.7+ with following modules installed: `sqlalchemy 1.4` (incompatible with `sqlalchemy 2.*` as it has some breaking changes),  `psycopg2`, `geoalchemy2`, `pytimeparse`, `pytz`, `tabulate`.

`als_query.py` already present in `als_siphon` container/pod, so, instead of laboriously installing all prerequisites (and passing database connection string parameters) it is recommended to run `als_query.py` right from container by means of `docker-compose ... exec als_query.py ...` or `kubectl exec als_siphon... als_query.py ...`
 

### Connection strings <a name="als_query_dsn">

`als_query.py` subcommands that access database (`log` and `als`) require full or partial connection strings. They can be passed in the following ways:

- Via command line parameters (`--dsn` and, optionally, `--password_file`)
- Via environment variables (`POSTGRES_LOG_CONN_STR` and, optionally, `POSTGRES_LOG_PASSWORD_FILE` for `log` subcommand, `POSTGRES_ALS_CONN_STR` and, optionally, `POSTGRES_ALS_PASSWORD_FILE` for `als` subcommand)
- Not at all. Just run `als_query.py` from inside `als_siphon` container or pod that already has these environment variables set

`--dsn` parameter as well as `POSTGRES_LOG_CONN_STR`, `POSTGRES_ALS_CONN_STR` environment variable have form `[<SCHEME>//:][<USERNAME>][:<PASSWORD>]<HOST>[:<PORT>][/<DATABASE>][?<OPTIONS>]` Of them only `<HOST>` is required, other may use defaults. `<PASSWORD>` may be specified explicitly or be taken from password file.

`--password_file` parameter as well as `POSTGRES_LOG_PASSWORD_FILE` and `POSTGRES_ALS_PASSWORD_FILE` environment variables may specify the name of file, that contains password. It might be used to map Docker Compose or Kubernetes secrets.

### Time and time zones <a name="als_query_time">

Records in ALS and Log databases are time tagged. Database stores timetags using UTC time zone (that is used by default), which might be inconvenient to use by mere mortals.

Both `als` and `log` subcommands have `--timezone` parameter that specifies what timezone to use for printing timetag and for interpreting time-related parameters (if timezone there not explicitly specified). Timezone may be specified in following forms:

- **UTC offset**. Using `UTC` or `GMT` prefix. E.g. `--timezone UTC-7` or `--timezone GMT+5:30`

- **Timezone name**. E.g `--timezone US/Pacific`. List of known timezone names may be printed with `timezones` subcommand (`...als_query.py timezones`)

- **Abbreviated name**. E.g. `--timezone PST`. This method is ***not recommended*** as e.g. in the summer Western time is `PDT`, whereas `PST` is Philippine time. In the winter `PDT` become unavailable (because it is winter), whereas `PST` also become unavailable (because it is ambiguous).

Time format both on print and in command line parameters is ISO 8601. On print it is *YYYY-MM-DDTHH:MM:SS.ssssss+/-HH:MM*. In command line parameters year/month/day may be omitted (current ones assumed), hour/minute/second may also be omitted (zeros assumed), timezone may be omitted (current timezone - UTC or specified by `--timezone` is assumed).


### `log` subcommand <a name="als_query_log">

`log` subcommand retrieves JSON logs from `AFC_LOGS` database. Each JSON log is belongs to certain *topic* (handy term, originated from Kafka). Topic is just any string, passed to `als.als_json_log()` - *lowercase is recommended* to avoid wrestling with SQL. It is recommended to keep format of JSON, used for every topic consistent, but this is not mandated.

Since content of JSON may be any and PostgreSQL already provides the special 'SELECT' syntax for accessing JSON data (see e.g. [here](https://www.postgresqltutorial.com/postgresql-tutorial/postgresql-json/) and [here](https://www.javatpoint.com/postgresql-json), google for further assistance), one queries JSON  logs with `log` subcommand by directly specifying a `SELECT` statement.

Each table in *AFC_LOGS* has the following columns (this is important when composing `SELECT` statements):

|Column|Content|
|------|-------|
|time|Time tag of when log record was made in|
|source|Name of entity that made record|
|log|JSON log data|

Command format:  
`als_query.py log OPTIONS [SELECT_BODY]`

|Option|Meaning|
|------|-------|
|--dsn **CONNECTION_STRING**|Full or partial connection string to JSON Logs database. May also be specified via `POSTGRES_LOG_CONN_STR` environment variable. See [Connection strings](#als_query_dsn) chapter|
|--password_file **PASSWORD_FILE**|Name of file containing password for connection string. May also be specified via `POSTGRES_LOG_PASSWORD_FILE` environment variable. See [Connection strings](#als_query_dsn) chapter|
|--verbose|Print SQL statements being emitted|
|--max_age **DURATION**|Only return latest records since given duration ago (e.g. `--max_age 1d3h`). May not be specified together with **SELECT_BODY**|
|--timezone **TIMEZONE**|Timezone to use for time output and as default for `--time_from` and `--time_to`. See [Time and time zones](#als_query_time) chapter|
|--time_from **[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]**|Return records since given time. May not be specified together with **SELECT_BODY**. See [Time and time zones](#als_query_time) chapter|
|--time_to **[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]**|Return records before given time. May not be specified together with **SELECT_BODY**. See [Time and time zones](#als_query_time) chapter|
|--max_count **MAX_COUNT**|Maximum number of results to print (all by default). May not be specified together with **SELECT_BODY**|
|--oldest_first|Sort in reverse time order. May not be specified together with **SELECT_BODY**|
|--format **FORMAT**|Output format: `table`, `csv`, `json` (unindented JSON, one line per record), `indent` (indented JSON)|
|--topics|Print current list of topics|
|--topic TOPIC|Topic to print. May not be specified together with **SELECT_BODY**|
|--sources|Print list of log sources - for all topics or for topic, specified with `--topic`|
|**SELECT_BODY**|Body of select statement to execute - without leading '`SELECT`' and trailing '`;`'. Topic should be specified in the '`FROM`' clause. Default is to print entire table for topic, specified by `--topic` (within limits, imposed by `--max_age`, `--time_from`, `--time_to` and `--max_count`. May need to be quoted|

#### `log` subcommand examples <a name="als_query_log_examples">

Examples below assume that DSN is specified via environment variables and proper command prefix is used (`docker-compose exec ...` or `kubectl exec ...` or none if execution performed from within the container).

- List all topics:  
  `als_query.py log --topics`  

- Print content of `user_access` topic (table) in its entirety, in table format:  
  `als_query.py log --topic user_access`  

- Print key names of JSONs of topic `rcache_invalidation`:  
  `als_query.py log "DISTINCT json_object_keys(log) FROM rcache_invalidation"`  
  This statement will invoke `SELECT DISTINCT json_object_keys(log) FROM rcache_invalidation;` SQL query.  
  Here 'log' is a name of column, containing JSON log records, `DISTINCT` and `FROM` are uppercase to demonstrate they are part of `SELECT` statement - they can be lowercase as well. Quotes used because otherwise `bash` tries to treat parentheses in a peculiar way.   

- Print all WebUI logins of user `foo` for last 3 days:  
  ```
  als_query.py log \
    "log FROM (user_access where log->'user'='foo') AND ((now()-time)<=make_interval(0,0,0,3))"
  ```
  Note the quotes around field names  
  Simpler way to achieve similar effect:  
  `als_query.py log --topic user_access --max_age 3d | grep foo`

- Print 10 last records from `rcache_invalidation` topic in indented JSON format, timed per US/Pacific time zone:  
  ```
  als_query.py log --topic rcache_invalidation --max_count 10 \
      --latest_first --timezone US/Pacific
  ```

- Print WebUI logins since October 12 of this(!) year in Australia/Queensland timezone, observing generated SQL queries:  
  ```
  als_query.py log --topic user_access --time_from 10-12 \
      --timezone Australia/Queensland --verbose
  ```


### `als` subcommand <a name="als_query_als">

This subcommand prints information about AFC request-responses, one request/response per line (unless `--distinct` specified). Some filtering and sorting options are provided.

One can chose what pieces of information about request/response to print:

|Out item<br>name|What is it?|Can be <br>sorted by?|
|----|-----------|------------------|
|time|Request RX time|Yes|
|server|AFC Server identity|Yes|
|request|AFC Request (e.g. for WebUI)|No|
|response|AFC Response|No|
|req_msg|AFC Request message|No|
|resp_msg|AFC Response message|No|
|duration|Message processing duration|Yes|
|serial|AP serial number|Yes|
|certificates|AP Certifications|Yes|
|location|AP Location|No|
|distance|Distance from from point, specified by `--pos` to AP|Yes|
|azimuth|Azimuth from point, specified by `--pos` to AP|Yes|
|psd|Max PSD results|Yes|
|eirp|Max EIRP results|Yes|
|config|AFC Config|Yes|
|region|Region aka customer (AFC Config ID)|Yes|
|error|Response code and supplemental info|Yes|
|dn|mTLS distinguished name|Yes|
|ip|IP address or request sender (AP or AFC Proxy)|Yes|
|runtime_opt|Flags from AFC Request URL (gui, nocache, debug, edebug)|Yes| 
|uls|ULS data version (not quite implemented as of time of this writing)|Yes|
|geo|Geodetic data version (not quite implemented as of time of this writing)|Yes|

This table may be printed with `--outs` command line parameter.

Command format:  
`als_query.py als OPTIONS [OUTS]`

|Option|Meaning|
|------|-------|
|--dsn **CONNECTION_STRING**|Full or partial connection string to ALS database. May also be specified via `POSTGRES_ALS_CONN_STR` environment variable. See [Connection strings](#als_query_dsn) chapter|
|--password_file **PASSWORD_FILE**|Name of file containing password for connection string. May also be specified via `POSTGRES_ALS_PASSWORD_FILE` environment variable. See [Connection strings](#als_query_dsn) chapter|
|--verbose|Print SQL statements being emitted|
|--keyhole_template **TEMPLATE_FILE**|Keyhole shape tempolate file name. File is generated by `keyhole_gen.py --type sql ...` from results of AFC Engine `KeyHoleShape` analysis. Keyhole shape covers area containing APs that may interfere eith FS RX looking in gioven direction (`--pos` and `--azimuth` parameters respectively). This file may also be specified with `KEYHOLE_TEMPLATE_FILE` environment variable|
|--max_age **DURATION**|Only return latest records since given duration ago (e.g. `--max_age 1d3h`)|
|--timezone **TIMEZONE**|Timezone to use for time output and as default for `--time_from` and `--time_to`. See [Time and time zones](#als_query_time) chapter|
|--time_from **[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]**|Return records since given time. See [Time and time zones](#als_query_time) chapter|
|--time_to **[[[yyyy-]mm-]dd][T[hh[:mm[:ss]]]][tz]**|Return records before given time. See [Time and time zones](#als_query_time) chapter|
|--max_count **MAX_COUNT**|Maximum number of results to print (all by default)|
|--format **FORMAT**|Output format: `table`, `csv`, `json` (unindented JSON, one line per record), `indent` (indented JSON)|
|--decode_errors|Print table with `als_siphon.py` decoding and other errors. Only time/count filtering parameters may be used|
|--outs|Print table of possible **OUTS** and `--order_by` values (see beginning of this chapter)|
|--miles|Use miles instead of kilometers in `dist` column and `--dist` parameter|
|--pos **LAT**,**LON**|Position from which distance to AP is measured (`dist` column, `--dist` parameter). Latitude and longitude (separated by comma and/or spaces) specified in degrees, hemisphere may be specified by sign (north/east is positive) or by letter: `--pos 37.5,-121.2` is the same as `--dist "37.5n 121W"`|
|--dist **DISTANCE**|Only print records for APs within given distance (specified in kilometers or miles - see `--miles` parameter) from position specified by `--pos`|
|--azimuth **AZIMUTH**|Only print records for APs at given direction (within keyhole shape) from posiotion, specified by `--pos`|
|--region[=-]**REGION**|Only print records served with AFC Config of/except given region|
|--serial[=-]**SERIAL**|Only print records for AP of/except given serial number|
|--certificate[=-]**CERTIFICATE**|Only print records of APs of/except given certification (i.e. of given manufacturer)|
|--ruleset[=-]**RULESET_ID**|Only print records of APs certified of/except given ruleset ID (i.e. certified by given certification authority)|
|--cn[=-]**MTLS_COMMON_NAME**|Only print records of/except given CN of mTLS certificate|
|-request_opt[=-]**OPT**|Only print records of messages with/without given request option flags (gui, nocache, debug, edebug)|
|--resp_code[=-]**CODE**|Only print records of/except given AFC Response codes (except `GeneralFailure` is `--resp_code=--1`)|
|--psd **[FROM_MHZ][-TO_MHZ]**|Only print records responded with PSD in given frequency range. Range boundaries specified in MHz|
|--eirp **CHANNELS_OR_FREQUENCIES**|Only print records responded with EIRP for channels specified by numbers, number ranges and frequency ranges. E.g. `--eirp 1-33,13,1-50:40,6800-7000` - here we have a range of all channel numbers, individual channel number, range for 40MHz only and frequency range, YMMV|
|--order_by **ITEM[,desc]**|Order by given output item (not all may be used - see table in the beginning of chapter). `desc` means order in descending order. This parameter may be specified several times|
|--distinct|Only print distinct results (i.e. drop repeated ones)|
|**OUTS**|Output items to print - see table of the beginning of this chapter (also printed by `--outs` parameter)|

#### `als` subcommand examples <a name="als_query_als_examples">

Examples below assume that DSN is specified via environment variables and proper command prefix is used (`docker-compose exec ...` or `kubectl exec ...` or none if execution performed from within the container).

- Print `als_siphon.py` decode errors for the last month:  
  `als_query.py als --decode_errors --max_age 30d`

- Print table of output item names:  
  `als_query.py als --outs`

- Print dates, serials and positions of APs, within 100 miles from given point, allowed to transmit in 6800-7000MHz frequency range, for last year, sorted by distance (farthest first) except requests made by ULS Downloader and from WebUI. Output is in CSV format:  
  ```
  als_query.py als --pos 37N,121E --dist 100 --miles --max_age 365d \
      --psd 6800-7000 --eirp 6800-7000 --serial=-FS_ACCEPTANCE_TEST \
      --runtime_opt=-gui --order_by distance,desc --format csv \
      time serial location
  ```


### `timezones` subcommand <a name="als_query_timezones">

Print list of known timezones.

`als_query.py timezones`


### `help` subcommand <a name="als_query_help">

Print help on given subcommand:

- Print list of subcommands:  
  `als_query.py help`

- Print help on `als` subcommand:  
  `als_query.py help als`

- Print help midway of entering command - just insert 'help' before subcommand name, every thing past it will be ignored:  
  `als_query.py help als --decode_errors --max_age `


## `als_db_tool.py` script <a name="als_db_tool">

This script (and its accompanying `als_db_tool.yaml` parameter file) provide functionality, related to ALS database partitioning.

It is contained in `als_siphon` container (aka `als-siphon` pod) thus taking ALS database DSN and password file parameters from environment variables.

### `prepare_sql` subcommand <a name="als_db_tool_prepare_sql">

This subcommand used internally on container startup and migration script that added partitioning. See help message for more information.

### `add_partitions` subcommand <a name="als_db_tool_add_partitions">

Adds partitions for future months. Since this operation automagically performed on container (re)start and may be controlled by **`AFC_ALS_MONTH_PARTITIONS_AHEAD`** environment variable there is no point in going to deatils either. See help message for more information.

### `partition_info` subcommand <a name="als_db_tool_partition_info">

Prints information about partition tables and their parents

```als_db_tool.py partition_info [OPTIONS]```

Options are:

|Option|Meaning|
|------|-------|
|--dsn **DSN**|DSN of ALS database (possibly without password or with default passwrd). By default taken from  `POSTGRES_ALS_CONN_STR` environment variable|
|--password_file **FILENAME**|File with ALS database password. By default taken from `POSTGRES_ALS_PASSWORD_FILE` environment variable|
|--by_parent|Print per-parent-table information of oldest and farthest in the future partition. By default print list of partition tables|

### `remove_partitions` subcommand <a name="als_db_tool_remove_partitions">

Removes all partition tables

```als_db_tool.py remove_partitions [OPTIONS]```

Options are:

|Option|Meaning|
|------|-------|
|--dsn **DSN**|DSN of ALS database (possibly without password or with default passwrd). By default taken from  `POSTGRES_ALS_CONN_STR` environment variable|
|--password_file **FILENAME**|File with ALS database password. By default taken from `POSTGRES_ALS_PASSWORD_FILE` environment variable|
|--keep_months **NUMBER_OF_MONTHS**|Delete all partitions older than given number of months|
|--keep_from **YEAR-MONTH**|Delete all partitions older than given month of given year|


