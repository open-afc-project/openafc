Copyright (C) 2022 Broadcom. All rights reserved.\
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate that
owns the software below. This work is licensed under the OpenAFC Project
License, a copy of which is included with this software program.

# Rcache - AFC Response Cache

## Table of contents
- [Operation overview](#overview)
  - [Rcache parts](#rcache_parts)
  - [Rcache counterparts](#rcache_counterparts)
- [Associated containers and their configuration](#service)
- [Client API](#client_api)
- [REST API](#rest_api)
- [`rcache_tool.py` - test and manipulation tool](#rcache_tool)
- [Database schema](#db_schema)
  - [Rcache table](#rcache_table)
  - [Switches table](#switches_table)

## Operation overview <a name="overview"/>

AFC Response computation takes a lot of time, hence it is beneficial to reuse previously computed AFC Response wherever possible. Rcache (Response Cache) is a mechanism for collection and retrieval of previously computed AFC Responses.

### Rcache parts <a name="rcache_parts"/>

Rcache consists of following parts, scattered over various corners of AFC source tree:

- **Rcache service** (sources in `rcache`). REST API service that runs in a separate container and responsible for all write-related operations, namely:
  - **Update.** Writes newly computed AFC Responses to Postgres database.
  - **Invalidate.** Marks as invalid cache entries affected by FS (aka ULS) data change or by AFC Config changes.
  - **Precompute.** Invalidated cache entries recomputed in order to avoid Message Handler /RaServer delay when AFC Request will arrive next time. This is called precomputation (as it happens before AFC Request arrival).

- **Rcache client library** (own and shared sources located in `src/afc-packages/rcache`). All AFC Services that interoperate with Rcache do it through this client library.  
  Rcache library not only communicates with Rcache service, but also directly interoperates with other parts of AFC system:
  - With **Postgres database** - to perform cache lookup
  - With **RabbitMQ** - to pass computed AFC Response from Worker to Message Handler / Rat Server.

- **Rcache tool** (sources located in `tools/rcache`). Script for Rcache testing - functional and load.  
  This script is intended to be executed in its own container (for which there is `Dockerfile` in sources) that can be hitched to main AFC docker composition (for which there is `compose_rcache.yaml` in sources).  
  Also it is included in Rcache service container - and may be ran from there.

### Rcache counterparts <a name="rcache_counterparts"/>

Following services interoperate with Rcache service or library:

- **Postgres database**. Rcache client library performs cache lookup directly in database (on behalf of Message Handler and Rat Server), Rcache service performs write operations.

- **Message Handler / Rat Server**. Uses Rcache client library to:

  - Perform Rcache lookup
  - Receive computed AFC Response sent by Worker via RabbitMQ. This route is used for synchronous non-GUI requests only. Responses on asynchronous and GUI requests passed in a legacy way - via ObjStore (without Rcache library involvement)

- **Worker**. Uses Rcache client library to:

  - Pass (via RabbitMQ) computed synchronous non-GUI AFC Response to Message Handler or Rat Server
  - Write (via Rcache REST API service) computed AFC Response to Postgres database.

- **RabbitMQ**. Used by Rcache client library to pass computed synchronous non-GUI AFC Responses from Worker to Message Handler or Rat Server

- **FS (aka ULS) downloader**. Uses Rcache library to notify Rcache service which regions were invalidated by changes in FS data.

- **???**. For the following actions there are REST APIs in Rcache service (but *no* Rcache client library routines), however as of time of this writing it is not quite clear who'll drive them:

   - Invalidation on AFC Config changes.
   - Full invalidation.
   - Setting precompute quota (maximum number of simultaneous precomputations).

## Associated containers and their configuration <a name="service"/>

All Rcache service, Rcache client library and, to certain extent, Rcache tool are configured by means of environment variables, passed to containers. All these variables have `RCACHE_` prefix. Here are these environment variables

|Name|Trivia|Meaning|
|----|------|-------|
|<small>**RCACHE_ENABLED**</small>|<small>*Current value*: True<br>*Defined in*: .env<br>*Used in images*: rcache, msghnd, rat_server, worker, uls_downloader</small>|TRUE to use Rcache for synchronous non-GUI requests. FALSE to use legacy ObjStore-based cache|
|<small>**RCACHE_CLIENT_PORT**</small>|<small>*Current value*: 8000<br>*Defined in*: .env, docker-compose.yaml, tools/rcache/Dockerfile<br>*Used in images*: rcache, rcache_tool</small>|Port on which Rcache REST API service is listening. Also this port is a part of **RCACHE_SERVICE_URL** environment variable, passed to other containers (see below)|
|<small>**RCACHE_POSTGRES_DSN**</small>|<small>*Current value*: postgresql://postgres:postgres@bulk_postgres/rcache<br>*Defined in*: docker-compose.yaml, tools/rcache/Dockerfile<br>*Used in images*: rcache, rat_server, msghnd, rcache_tool</small>|Connection string to Postgres database that stores cache|
|<small>**RCACHE_POSTGRES_PASSWORD_FILE**</small>|<small>*Current value*: <SECRETS_DIRECTORY>/RCACHE_DB_PASSWORD<br>*Defined in*: docker-compose.yaml, tools/rcache/Dockerfile<br>*Used in images*: rcache, rat_server, msghnd, rcache_tool</small>|File containing password for Rcache DB DSN|
|<small>**AFC_DB_CREATOR_URL**</small>|<small>*Current value*: http://rat_server/fbrat/admin/CreateDb<br>*Defined in*: docker-compose.yaml<br>*Used in images*: rcache</small>|REST API URL for Postgres database creation|
|<small>**RCACHE_SERVICE_URL**</small>|<small>*Current value*: http://rcache:$\{RCACHE_CLIENT_PORT\}<br>*Defined in*: docker-compose.yaml, tools/rcache/Dockerfile<br>*Used in images*: msghnd, rat_server, worker, uls_downloader, rcache_tool</small>|Rcache service REST API URL|
|<small>**RCACHE_RMQ_DSN**</small>|<small>*Current value*: amqp://rcache:rcache@<br>rmq:5672/rcache<br>*Defined in*: docker-compose.yaml<br>*Used in images*: msghnd, rat_server, worker</small>|RabbitMQ AMQP URL Worker uses to send and msghnd/rat_server to receive AFC Response. Note that user and vhost (rcache and rcache in this URL) should be properly configured in RabbitMQ service|
|<small>**RCACHE_AFC_REQ_URL**</small>|<small>*Current value*: http://msghnd:8000/fbrat/ap-afc/availableSpectrumInquiry?nocache=True<br>*Defined in*: docker-compose.yaml<br>*Used in images*: rcache</small>|REST API Rcache service uses to launch precomputation requests. No precomputation if this environment variable empty or not defined|
|<small>**RCACHE_RULESETS_URL**</small>|<small>*Current value*: http://rat_server/fbrat/ratapi/v1/GetRulesetIDs<br>*Defined in*: docker-compose.yaml<br>*Used in images*: rcache</small>|REST API URL to request list of Ruleset IDs in use. If empty or not defined default AFC distance (130km as of time of this writing) is used for spatial invalidation|
|<small>**RCACHE_CONFIG_RETRIEVAL_URL**</small>|<small>*Current value*: http://rat_server/fbrat/ratapi/v1/GetAfcConfigByRulesetID<br>*Defined in*: docker-compose.yaml<br>*Used in images*: rcache</small>|REST API URL to request AFC Config by for Ruleset ID. If empty or not defined default AFC distance (130km as of time of this writing) is used for spatial invalidation|
|<small>**RCACHE_PRECOMPUTE_QUOTA**</small>|<small>*Current value*: 10<br>*Defined in*: rcache/Dockerfile<br>*Used in images*: rcache</small>|Maximum number of parallel precompute requests. Also can be changed on the fly with Rcache service REST API|
|<small>**RCACHE_UVICORN_PARAMS**</small>|<small>*Current value*: "--no-access-log --log-level info"<br>*Defined in*: rcache/Dockerfile<br>*Used in images*: rcache</small>||
|<small>**RCACHE_ALEMBIC_CONFIG**</small>|<small>*Current value*: /wd/migrations/alembic.ini<br>*Defined in*: rcache/Dockerfile<br>*Used in images*: rcache</small>|Alembic config file in image filesystem. If undefined - no Alembic upgrades is performed|
|<small>**RCACHE_ALEMBIC_INITIAL_VERSION**</small>|<small>*Current value*: 5663cf8afbd7<br>*Defined in*: rcache/Dockerfile<br>*Used in images*: rcache</small>|Assumed alembic version of alembicless database|
|<small>**RCACHE_ALEMBIC_HEAD_VERSION**</small>|<small>*Current value*: head<br>*Defined in*: nowhere<br>*Used in images*: rcache</small>|Latest/current Alembic version|



## Client API <a name="client_api"/>

All Rcache client API presented by `RcacheClient` class, defined in shared `rcache_client` module (that should be imported) - and subsequently will (at least - should) be added there.

Data structures, used in certain interfaces of this class (as well as in Rcache REST APIs, database manipulations, etc.), are defined in `rcache_models` module (that also should be imported).

Among other data structures, defined in `rcache_models` module there is `RcacheClientSettings` class, containing Rcache client parameters. By default these parameters are taken from environment variables ([see above](#service)), but they also can be explicitly specified.


## REST API <a name="rest_api"/>

Rcache service is FastAPI-based, so all its REST APIs are self-documented on `/docs`, `/redoc` and even `/openapi.json` endpoints (see <https://fastapi.tiangolo.com/tutorial/metadata/>).

Here is one possible way to reach this trove of knowledge (uppercase are values you need to figure out):

1. Start AFC docker compose.  
   There are no sane ways of doing it - pick your favorite insane.
2. Inspect `rcache` docker container:  
   `docker container inspect AFCPROJECTNAME_rcache_INSTANCEINDEX`
3. Find `rcache` container IP in host file system:  
   In printed JSON it is at `[0]["NetworkSettings"]["Networks"][DEFAULTNETWORKNAME]["IPAddress"]`
4. Do ssh or plink local port forwarding from your client machine to rcache container IP, port 8000 (or whatever `RCACHE_CLIENT_PORT` is set to):  
  `SSHORPLINK -L LOCALPORT:RCACHEIPADDRESS:8000`
4. On client machine browse to `localhost:LOCALPORT/redoc`

## `rcache_tool.py` - test and manipulation tool <a name="rcache_tool"/>

Rcache tool requires certain uncommon Python modules to be installed (pydantic, aiohttp, sqlalchemy, tabulate). Also it uses shared Rcache sources. Hence this script supposed to be run from container.

Dockerfile for this container can be found in `tools/rcache` - it should be built from top level AFC checkout directory. To run properly this container should be connected to AFC internal network. This might be achieved with `docker run --network NETWORKNAME ...` or by hitching this container to AFC docker compose (`tools/rcache/compose_rcache.yaml` may be used for this).

Besides its own container this script exists in Rcache service container - and may be ran from there.

General invocation format:
`./rcache_tool.py SUBCOMMAND PARAMETERS`

List of available subcommands may be obtained with  
`./rcache_tool.py --help`

Help on individual subcommand parameters may be obtained with  
`./rcache_tool.py help SUBCOMMAND`

Some practical use cases (all examples assumed to be executed from rcache container, so rcache service location is guessed automagically):

- **Print service status**. Once:  
  `./rcache_tool.py status`  
  ... and once per second (e.g. in parallel with some test):
  `./rcache_tool.py status --interval 1`  

- **Disable cache invalidation** (e.g. by ULS downloader). Useful to prevent ULS Downloader's influence on performance testing:  
  `./rcache_tool.py invalidate --disable`  
  ... and re-enable it back:  
  `./rcache_tool.py invalidate --enable`  
  ***Restoring original state is essential, as enable/disable state is persisted in database***

- **Invalidate all cache** *AND* prevent precomputer from its re-validation:  
  `./rcache_tool.py precompute --disable`  
  `./rcache_tool.py invalidate --all`  
  ... and restore precomputation back:  
  `./rcache_tool.py precompute --enable`  
  ***Restoring original state is essential, as enable/disable state is persisted in database***

- **Disable caching**:  
  `./rcache_tool.py precompute --disable`  
  `./rcache_tool.py update --disable`  
  `./rcache_tool.py invalidate --all`  
  ... and restore status quo ante:  
  `./rcache_tool.py precompute --enable`  
  `./rcache_tool.py update --enable`  
  ***Restoring original state is essential, as enable/disable state is persisted in database***

- **Do Rcache update stress test** (`afc_load_tool.py` also can do it), parallel writing from 20 streams:  
  `./rcache_tool.py mass_fill --max_idx 1000000 --threads 20`


## Database schema<a name="db_schema"/>

Rcache uses database, consisting of two tables.

### Rcache table <a name="rcache_table">

Table name is `aps`, each of its rows represent data on a single AP. It has following columns:

|Column|Is<br>primary<br>key|Has<br>index|Data<br>type|Comment|
|------|--------------------|------------|------------|-------|
|serial_number|Yes|Yes|String|AP Serial number|
|rulesets|Yes|Yes|String|Request Ruleset IDs, concatenated with `|`. Kinda AP region|
|cert_ids|Yes|Yes|String|Request Certification IDs, concatenated with `|`. Kinda AP manufacturer|
|state|No|Yes|Enum<br>(Valid,<br>Invalid,<br>Precomp)|Row state. **Valid** - may be used, **Invalid** - invalidated, to be precomputed, **Precomp** - precomputation is in progress|
|config_ruleset|No|Yes|String|Ruleset ID used for computation. Used for invalidation by Ruleset ID (i.e. by AFC Config change)|
|coordinates|No|Yes|geography(POINT,4326)|AP coordinate. Used for spatial invalidation (on FS data change)|
|last_update|No|Yes|DateTime|Timetag of last update - eventually may be used for removal of old rows|
|req_cfg_digest|No|Yes|String|Hash, computed over AFC Request and AFC Config. Used as key for cache lookup|
|validity_period_sec|No|No|Float|Validity period of original response. Used to compute expiration time of response retrieved from cache|
|request|No|No|String|AFC Request as string. Used for precomputation|
|response|No|No|String|AFC Response as string. The meat of all this story|

### Switches table <a name="switches_table">

Table name is `switches`, each of its rows represent enable/disable setting. It has following columns:

|Column|Comment|
|------|-------|
|name|Setting name. This is a primary key. As of time of this writing, names were: *Update*, *Invalidate*, *Precompute*|
|state|*True* if enabled, *False* if disabled. Default is *True*|
