Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Tools For Working With Log Databases

## Table of Contents
- [Tools For Working With Log Databases](#tools-for-working-with-log-databases)
  - [Table of Contents](#table-of-contents)
  - [Databases ](#databases-)
    - [*ALS* Database ](#als-database-)
    - [*AFC\_LOGS* Database ](#afc_logs-database-)
    - [Initial Database ](#initial-database-)
    - [Template Databases ](#template-databases-)
  - [`als_siphon.py` - Moving Logs From Kafka To Postgres ](#als_siphonpy---moving-logs-from-kafka-to-postgres-)
  - [`als_query.py` - Querying Logs From Postgres Database ](#als_querypy---querying-logs-from-postgres-database-)
    - [Installing and running ](#installing-and-running-)
    - [`log` Command ](#log-command-)
      - [`log` Command Examples ](#log-command-examples-)


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


### Installing and running <a name="als_query_deploy">

`als_query.py` requires Python 3 with reasonably recent *sqlalchemy*, *psycopg2*, *geoalchemy2* modules installed (latter is optional - not required for e.g. `log` command). 

Proper installation of these modules requires too much luck to be described here (as even `venv/virtualenv` does not help always - only sometimes). If you'll succeed - fine, otherwise there is one more method: running from the container where `als_siphon.py` installed. In latter case invocation looks like this:

`$ docker exec SIPHON_CONTAINER als_query.py CMD ...`

Here `SIPHON_CONTAINER` is either value from first column of `docker ps` or from last column of `docker-compose ps`.

Another tricky aspect is how to access PostgreSQL database server where logs were placed (`--server` switch of `als_query.py`). Here are following possible cases:

1. `als_query.py` runs inside `als_siphon.py` container, PostgreSQL runs inside the container, named `als_postgres` in *docker-compose.yaml* (that's how it is named as of time of this writing):  
  `$ docker exec SIPHON_CONTAINER als_query.py CMD \ `  
  `--server [USER@]als_postrgres[:PORT][?OPTIONS] [--password PASSWORD] ...`  
  Here `USER` or `PORT` might be omitted if they are `postgres` and `5432` respectively. `--password PASSWORD` and `OPTIONS` are optional.

2. User/host/port of PostgreSQL server is known:  
  `$ [docker exec SIPHON_CONTAINER] als_query CMD \ `  
  `--server [USER@]HOST[:PORT][?OPTIONS] [--password PASSWORD] ...`  

3. `als_query.py` runs outside container, PostgreSQL runs inside container:  
  `$ als_query.py CMD \ `  
  `--server [USER@]^POSTGRES_CONTAINER[:PORT][?OPTIONS] \ `  
  `[--password PASSWORD] ...`  
  Note the `***^***` before `POSTGRES_CONTAINER`. Here, again `POSTGRES_CONTAINER` is either value from first column of `docker ps` or from last column of `docker-compose ps` for container running PostgreSQL

I expect #1 to be the common case for development environment, #2 - for deployment environment, #3 - for illustrations (for sake of brevity) or for some lucky conditions.

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

- For sake command line brevity suppose that `als_query.py` runs in the wild (not from container) and PostgreSQL is in `als_postgres_1` container (see [Installing and running](#als_query_deploy) case #1 for more realistic scenario).

Now, here are some possible actions:

- List all topics:  
  `$ als_query.py -s ^als_postgres_1 --topics`

- Print content of *foo* topic (table) in its entirety, using CSV format:  
  `$ als_query.py -s ^als_postgres_1 "* from foo"`

- Print key names of JSONs of topic *foo*:  
  `$ als_query.py -s ^als_postgres_1 json_object_keys(log) from foo`  
  Note that quotes may be omitted here, as there are no special symbols in select statement.

- From topic *foo* print values of *c.d* for all records, using bare (unadorned) format:   
  `$ als_query.py -s ^als_postgres_1 -f bare "log->'c'->'d' from foo"`  
  Note the quotes around field names

- From topic *foo* print only values of *b[0]* for all records where *a* field equals *179*:  
  `$ als_query.py -s ^als_postgres_1 "log->'b'->0 from foo \ `  
  `where log->'a' = 179"`  
  Note the way list indexing is performed (`->0`).

- Print maximum value of column *a* in topic *foo*:  
  `$ als_query.py -s ^als_postgres_1 "MAX(log->'a') from foo"`  

- Print log records in given time range:  
  `$ als_query.py -s ^als_postgres_1 "* from foo " \ `  
  `"where time > '2023-02-08 23:25:54.484174+00:00'" \ `  
   `"and time < '2023-02-08 23:28:54.484174+00:00'"`

