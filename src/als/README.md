Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Tools For Working With Log Databases

## Table of Contents
- [Databases ](#databases)
  - [*ALS* Database ](#als_database)
  - [*AFC\_LOGS* Database ](#afc_logs_database)
  - [Initial Database ](#initial_database)
  - [Template Databases ](#template_databases)
- [`als_siphon.py` - Moving Logs From Kafka To Postgres ](#als_siphon_py)
- [`als_query.py` - Querying Logs From Postgres Database ](#als_query_py)
  - [Installation](#als_query_install)
  - [Addressing PostgreSQL](#als_query_server)
  - [`log` Command ](#als_query_log)
    - [`log` Command Examples ](#als_query_log_examples)


## Databases <a name="databases">

ALS (AFC Log Storage) functionality revolves around two PostgreSQL databases, used for log storage: **ALS** and **AFC_LOGS**.

### *ALS* Database <a name="als_database">

Stores log of AFC Request/Response/Config data. Has rather convoluted multitable structure.

SQL code for creation of this database contained in *ALS.sql* file. This file should be considered generated and not be manually edited.

*als_db_schema* folder contains source material for *ALS.sql* generation:

- *ALS.dbml*. Source file for [dbdiagram.io](https://dbdiagram.io) DB diagramming site. Copy/paste content of this file there, make modifications, then copy/paste back to this file.  
  Also upon completion *ALS_raw.sql* and *ALS.png* should be exported (as *Export to PostgreSQL* and *Export to PNG* respectively) - see below.

- *ALS_raw.sql*. Database creation SQL script that should exported from [dbdiagram.io](https://dbdiagram.io) after changes made to *ALS.dbml*.  
  This file is almost like final *ALS.sql*, but requires certain tweaks:
    * Declaring used PostgreSQL extensions (PostGIS in this case)
    * Removal of many-to-many artifacts. For many-to-many relationships [dbdiagram.io](https://dbdiagram.io) creates artificial tables that are, adding insult to injury, violate PostgreSQL syntax. They are not used and should be removed.
    * Segmentation. Database is planned with segmentation in mind (by *month_idx* field). But segmentation itself not performed. This will need to be done eventually.

- *als_rectifier.awk* AWK script for converting *ALS_raw.sql* to *ALS.sql*.

- *ALS.png* Picturesque database schema. Should be exported as PNG after changes, made to *ALS.dbml*.

### *AFC_LOGS* Database <a name="als_logs_database">

For each JSON log type (*topic* on Kafka parlance) this database has separate table with following columns:

- *time* Log record timetag with timezone

- *source* String that uniquely identifies entity that created log record

- *log* JSON log record.

### Initial Database <a name="initial_database">

To create database `als_siphon.py` script should connect to already database. This already existing database named *initial database*, by default it is built-in database named *postgres*.

### Template Databases <a name="template_database">

Template databases, used for creation of *ALS* and *AFC_LOGS* databases. Something other than default might be used (but not yet, as of time of this writing).


## `als_siphon.py` - Moving Logs From Kafka To Postgres <a name="als_siphon_py">

The main purpose of `als_siphon.py` is to fetch log records from Kafka and move them to previously described PostgreSQL databases. Also it can initialize those databases.

`$ als_siphon.py COMMAND PARAMETERS`

Commands are:

- `init` Create *ALS* and/or *AFC_LOGS* database. If already exists, databases may be recreated or left intact.

- `siphon` Do the moving from Kafka to PostgreSQL.

- `init_siphon` First create databases then do the siphoning. Used for Docker operation.

Parameters are many - see help messages.


## `als_query.py` - Querying Logs From Postgres Database <a name="als_query_py">

This script queries logs, stored in *ALS* and *AFC_LOGS* databases.

As of time of this writing this script only supports `log` command that reads JSON logs from *AFC_LOGS*


### Installation <a name="als_query_install">

`als_query.py` requires Python 3 with reasonably recent *sqlalchemy*, *psycopg2*, *geoalchemy2* modules installed (latter is optional - not required for e.g. `log` command). 

Proper installation of these modules requires too much luck to be described here (as even `venv/virtualenv` does not help always - only sometimes). If you'll succeed - fine, otherwise there is one more method: running from the container where `als_siphon.py` installed. In latter case invocation looks like this:

`$ docker exec SIPHON_CONTAINER als_query.py CMD ...`

Here `SIPHON_CONTAINER` is either value from first column of `docker ps` or from last column of `docker-compose ps`.

### Addressing PostgreSQL Server <a name="als_query_server">

Another important aspect is how to access PostgreSQL database server where logs were placed.

#### Explicit specification

Using `--server` (aka `-s`) and `--password` parameters of `als_query.py` command line). Here are most probable cases:

1. `als_query.py` runs inside `als_siphon.py` container, PostgreSQL runs inside the container, named `als_postgres` in *docker-compose.yaml* (that's how it is named as of time of this writing):  
  `$ docker exec SIPHON_CONTAINER als_query.py CMD \ `  
  `--server [USER@]als_postrgres[:PORT][?OPTIONS] [--password PASSWORD] ...`  
  Here `USER` or `PORT` might be omitted if they are `postgres` and `5432` respectively. `--password PASSWORD` and `OPTIONS` are optional.   
  Actually, in this case `--server` and `--password` may be omitted - see below on the use of environment variables.

2. User/host/port of PostgreSQL server is known:  
  `$ [docker exec SIPHON_CONTAINER] als_query CMD \ `  
  `--server [USER@]HOST[:PORT][?OPTIONS] [--password PASSWORD] ...`  

3. `als_query.py` runs outside container, PostgreSQL runs inside container:  
  `$ als_query.py CMD \ `  
  `--server [USER@]^POSTGRES_CONTAINER[:PORT][?OPTIONS] \ `  
  `[--password PASSWORD] ...`  
  Note the `***^***` before `POSTGRES_CONTAINER`. Here, again `POSTGRES_CONTAINER` is either value from first column of `docker ps` or from last column of `docker-compose ps` for container running PostgreSQL

I expect #1 to be the common case for development environment, #2 - for deployment environment, #3 - for illustrations (for sake of brevity) or for some lucky conditions.

#### Environment variables

If `--server` parameter not specified `als_query.py` attempts to use environment variables:

- `POSTGRES_LOG_USER`, `POSTGRES_HOST`, `POSTGRES_PORT`, `POSTGRES_LOG_PASSWORD` for accessing *AFC_LOGS* database
- `POSTGRES_ALS_USER`, `POSTGRES_HOST`, `POSTGRES_PORT`, `POSTGRES_ALS_PASSWORD` for accessing *ALS* database

These environment variables are passed to container with `als_siphon.py`, so they are quite natural choice when running `als_query` from there (case #1 above). 

Hence for case #1 `als_query.py` command line would actually look like this:  
`$ docker exec SIPHON_CONTAINER als_query.py CMD ...`  
Where `...` does not contain `--server`


### `log` Command <a name="als_query_log">

`log` command retrieves JSON logs from *AFC_LOGS* database. Each JSON logs is belongs to certain *topic* (handy term, originated from Kafka). Topic is a string (***lowercase highly recommended, 'ALS' name must not be used***) that supposedly corresponds to format (content) of JSON data.

Topic specifies a name of table inside *AFC_LOGS* database.

Since content of JSON may be any and PostgreSQL already provides the special 'SELECT' syntax for accessing JSON data (see e.g. [here](https://www.postgresqltutorial.com/postgresql-tutorial/postgresql-json/) and [here](https://www.javatpoint.com/postgresql-json), google for further assistance), `log` command is, in fact, thin wrapper around `SELECT` command, plus a couple of additional options.

Each table in *AFC_LOGS* has the following columns (this is important when composing `SELECT` statements):

|Column|Content|
|------|-------|
|time|Time when log record was made in (includes date, time, timezone)|
|source|Entity (e.g. WEB server) that made record|
|log|JSON log data|

Command format:  
`$ [docker exec SIPHON_CONTAINER] als_query.py log OPTIONS [SELECT_BODY]`

|Parameter|Meaning|
|---------|-------|
|--server/-s **[USER@][^]HOST_OR_CONTAINER[:PORT][?OPTIONS]**|PostgreSQL server connection parameters. See discussion in [Installing and running](#als_query_deploy) chapter. This parameter is mandatory|
|--password **PASSWORD**|PostgreSQL connection password (if required)|
|--topics|List existing topics (database tables)|
|--sources **[TOPIC]**|List sources - all or from specific topic|
|--format/-f **{bare\|json\|csv}**|Output format for SELECT-based queries: **bare** - unadorned single column output, **csv** - output as CSV table (default), **json** - output as JSON list or row dictionaries|
|**SELECT_BODY**|SQL SELECT statement body (without leading `SELECT` and trailing `;`. May be unquoted, but most likely requires quotes because of special symbols like `*`, `>`, etc.|

#### `log` Command Examples <a name="als_query_log_examples">

Suppose that:

- There are various topics (tables), among which there is topic *few* (let me remind again, that lowercase topic names are recommended), filled with JSONs with structure similar to this:  
```
{
    "a": 42,
    "b": [1, 2, 3],
    "c": {"d": 57}
}
```

- `als_query.py` runs in `regression_als_siphon_1` container (YMMV - see output of `docker-compose ps`). In this case there is no need to pass `--server` parameter, as it will be taken from environment variables.

Now, here are some possible actions:

- List all topics:  
  `$ docker exec regression_als_siphon_1 als_query.py log --topics`  
  Note that there is no `--server` parameter here, as `als_query.py` would values, passed over environment variables.

- Print content of *foo* topic (table) in its entirety, using CSV format:  
  `$ docker exec regression_als_siphon_1 als_query.py log "* from foo"`  
  This invokes `SELECT * from foo;` on *AFC_LOGS* database of PostgreSQL server.

- Print key names of JSONs of topic *foo*:  
  `$ docker exec regression_als_siphon_1 als_query.py log \ `  
  `json_object_keys(log) from foo`  
  Note that quotes may be omitted here, as there are no special symbols in select statement.

- From topic *foo* print values of *c.d* for all records, using bare (unadorned) format:   
  `$ docker exec regression_als_siphon_1 als_query.py log \ `  
  `-f bare "log->'c'->'d' from foo"`  
  Note the quotes around field names

- From topic *foo* print only values of *b[0]* for all records where *a* field equals *179*:  
  `$ docker exec regression_als_siphon_1 als_query.py log \ `  
  `"log->'b'->0 from foo where log->'a' = 179"`  
  Note the way list indexing is performed (`->0`).

- Print maximum value of column *a* in topic *foo*:  
  `$ docker exec regression_als_siphon_1 als_query.py log "MAX(log->'a') from foo"`  

- Print log records in given time range:  
  `$ docker exec regression_als_siphon_1 als_query.py log \ `  
  `"* from foo where time > '2023-02-08 23:25:54.484174+00:00'" \ `  
  `"and time < '2023-02-08 23:28:54.484174+00:00'"`
