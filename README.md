This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Contributing**](#contributing)
  - [How to contribute](#how-to-contribute)
  - [Pull request best practices](#pull-request-best-practices)
    - [Step 1: File an issue](#step-1-file-an-issue)
    - [Step 2: Clone OpenAFC GitHub repository](#step-2-clone-openafc-github-repository)
    - [Step 3: Create a temporary branch](#step-3-create-a-temporary-branch)
    - [Step 4: Commit your changes](#step-4-commit-your-changes)
    - [Step 5: Rebase](#step-5-rebase)
    - [Step 6: Run the tests](#step-6-run-the-tests)
    - [Step 7: Push your branch to GitHub](#step-7-push-your-branch-to-github)
    - [Step 8: Send the pull request](#step-8-send-the-pull-request)
      - [Change Description](#change-description)
- [**How to Build**](#how-to-build)
- [AFC Engine build in docker setup](#afc-engine-build-in-docker-setup)
  - [Installing docker engine](#installing-docker-engine)
  - [Building the Docker image](#building-the-docker-image)
    - [Prerequisites:](#prerequisites)
    - [Building Docker image from Dockerfile (can be omitted once we have Docker registry)](#building-docker-image-from-dockerfile-can-be-omitted-once-we-have-docker-registry)
    - [Pulling the Docker image from Docker registry](#pulling-the-docker-image-from-docker-registry)
  - [Building OpenAFC engine](#building-openafc-engine)
- [**OpenAFC Engine Server usage in Docker Environment**](#openafc-engine-server-usage-in-docker-environment)
- [AFC Engine build in docker](#afc-engine-build-in-docker)
  - [Building Docker Container OpenAFC engine server](#building-docker-container-openafc-engine-server)
    - [Using scripts from the code base](#using-scripts-from-the-code-base)
    - [To 'manually' build containers one by one:](#to-manually-build-containers-one-by-one)
    - [celery worker prereq containers](#celery-worker-prereq-containers)
  - [Prereqs](#prereqs)
  - [docker-compose](#docker-compose)
  - [**Environment variables**](#environment-variables)
  - [RabbitMQ settings](#rabbitmq-settings)
  - [PostgreSQL structure](#postgresql-structure)
    - [Upgrade PostgreSQL](#upgrade-postgresql)
  - [Initial Super Administrator account](#initial-super-administrator-account)
    - [Note for an existing user database](#note-for-an-existing-user-database)
  - [Managing user account](#managing-user-account)
  - [User roles](#user-roles)
  - [MTLS](#mtls)
  - [ULS database update automation](#uls-database-update-automation)

# **Introduction**

This document describes the procedure for submitting the source code changes to the TIP's openAFC github project. Procedure described in this document requires access to TIP's openAFC project and knowledge of the GIT usage. Please contact support@telecominfraproject.com in case you need access to the openAFC project.
Github.com can be referred for [details of alternate procedures for creating the pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests), developers can use any of these methods but need to include change description as part of pull requests  description.


<br /><br />

# **Contributing**

All contributions are welcome to this project.

## How to contribute

* **File an issue** - if you found a bug, want to request an enhancement, or want to implement something (bug fix or feature).
* **Send a pull request** - if you want to contribute code. Please be sure to file an issue first.

## Pull request best practices

We want to accept your pull requests. Please follow these steps:

### Step 1: File an issue

Before writing any code, please file a Jira ticket [here](https://telecominfraproject.atlassian.net/jira/software/c/projects/OA/issues) stating the problem you want to solve or the feature you want to implement. This allows us to give you feedback before you spend any time writing code. There may be a known limitation that can't be addressed, or a bug that has already been fixed in a different way. The jira ticket allows us to communicate and figure out if it's worth your time to write a bunch of code for the project.

### Step 2: Clone OpenAFC GitHub repository

OpenAFC source repository can be cloned using the below command.
```
git clone git@github.com:Telecominfraproject/open-afc.git
```
This will create your own copy of our repository.
[about remote repositories](https://docs.github.com/en/get-started/getting-started-with-git/about-remote-repositories)

### Step 3: Create a temporary branch

Create a temporary branch for making your changes.
Keep a separate branch for each issue/feature you want to address .
```
git checkout -b <JIRA-ID>-branch_name
```

Highly desirable to use branch name from Jira title, or use meaningful branch name reflecting the actual changes
```
eg. git checkout -b OA-146-update-readme-md-to-reflect-jira-and-branch-creation-procedure
```

### Step 4: Commit your changes
As you develop code, commit your changes into your local feature branch.
Please make sure to include the issue number you're addressing in your commit message.
This helps us out by allowing us to track which issue/feature your commit relates to.
Below command will commit your changes to the local branch.
Note to use JIRA-ID at the beginning of commit message.
```
git commit -a -m "<JIRA-ID>  desctiption of the change  ..."
```
### Step 5: Rebase

Before sending a pull request, rebase against upstream, such as:

```
git fetch origin
git rebase origin
```
This will add your changes on top of what's already in upstream, minimizing merge issues.

### Step 6: Run the tests

Make sure that all regression tests are passing before submitting a pull request.

### Step 7: Push your branch to GitHub

Push code to your remote feature branch.
Below command will push your local branch along with the changes to OpenAFC GitHub.
```
git push -u origin <JIRA-ID>-branch_name
```
 > NOTE: The push can include several commits (not only one), but these commits should be related to the same logical change/issue fix/new feature originally described in the [Step 1](#step-1-file-an-issue).

### Step 8: Send the pull request

Send the pull request from your feature branch to us.

#### Change Description


When submitting a pull request, please use the following template to submit the change description, risks and validations done after making the changes
(not a book, but an info required to understand the change/scenario/risks/test coverage)

- JIRA-ID number (from [Step 1](#step-1-file-an-issue)). A brief description of issue(s) being fixed and likelihood/frequency/severity of the issue, or description of new feature if it is a new feature.
- Reproduction procedure: Details of how the issue could be reproduced / procedure to reproduce the issue.
Description of Change:  A detailed description of what the change is and assumptions / decisions made
- Risks: Low, Medium or High and reasoning for the same.
- Fix validation procedure:
  - Description of validations done after the fix.
  - Required regression tests: Describe what additional tests should be done to ensure no regressions in other functionalities.
  - Sanity test results as described in the [Step 6](#step-6-run-the-tests)

> NOTE: Keep in mind that we like to see one issue addressed per pull request, as this helps keep our git history clean and we can more easily track down issues.

<br /><br />
<br /><br />

# **How to Build**
# AFC Engine build in docker setup

## Installing docker engine
Docker engine instructions specific to your OS are available on [official website](https://docs.docker.com/engine/install/)

## Building the Docker image
(Building the Docker image with build environment can be omitted once we have Docker registry)

### Prerequisites:

Currently, all the prerequisites (except docker installation) are situated in [OpenAFC project's GitHub](https://github.com/Telecominfraproject/open-afc). All you need is to clone OpenAFC locally to your working directory and start all following commands from there.
In this doc we assume to work in directory /tmp/work

### Building Docker image from Dockerfile (can be omitted once we have Docker registry)

This can take some time

```
docker build . -t afc-build -f worker/Dockerfile
```

### Pulling the Docker image from Docker registry

Not available. Possibly will be added later.

## Building OpenAFC engine

Building and runnig building and running afc-engine as standalone application.

**NB:** "-v" option maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/afc&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/afc/


goto the project dir
```
cd open-afc
```

run shell of alpine docker-for-build shell
```
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc public.ecr.aws/w9v6y1o0/openafc/worker-al-build-image:latest ash
```

inside the container's shell, execute:
```
mkdir -p -m 777 /wd/afc/build && BUILDREV=offlinebuild && cd /wd/afc/build && cmake -DCMAKE_INSTALL_PREFIX=/wd/afc/__install -DCMAKE_PREFIX_PATH=/usr -DBUILD_WITH_COVERAGE=off -DCMAKE_BUILD_TYPE=EngineRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd/afc && ninja -j$(nproc) install
```
Now the afc-engine is ready: 
```
[@wcc-afc-01 work/dimar/open-afc] > ls -l build/src/afc-engine/afc-engine
-rwxr-xr-x. 1 dr942120 dr942120 4073528 Mar  8 04:03 build/src/afc-engine/afc-engine
```
run it from the default worker container:
```
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc -v /opt/afc/databases/rat_transfer:/mnt/nfs/rat_transfer 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-worker:my_tag sh
```
inside the worker container execute the afc-engine app
```
./afc/build/src/afc-engine/afc-engine
```


# **OpenAFC Engine Server usage in Docker Environment**
# AFC Engine build in docker

## Building Docker Container OpenAFC engine server
There is a script that builds all container used by the AFC service.
This script is used by automatic test infrastructure. Please check [tests/regression](/tests/regression/) dir.

### Using scripts from the code base
to rebuild all containers use this scripts:
```
cd open-afc
tests/regression/build_imgs.sh `pwd` my_tag 0
```
after the build, check all new containers:
```
docker images | grep my_tag
```
these containes are used by [tests/regression/run_srvr.sh](/tests/regression/run_srvr.sh)
to run server using test infra scrips, please check and update [.env](/tests/regression/.env) used by [docker-compose.yaml](/tests/regression/docker-compose.yaml)
1. update path to host's AFC static data directory (where nlcd, 3dep, lidar and other stuff exists)
2. update port variables values
3. comment out tls/mtls vars if simple http connection is used
```
./tests/regression/run_srvr.sh `pwd` my_tag
```


### To 'manually' build containers one by one:
```
cd open-afc

docker build . -t rat_server -f rat_server/Dockerfile

docker build . -t worker -f worker/Dockerfile

docker build . -t uls_updater -f uls/Dockerfile-uls_updater

docker build . -t uls_service -f uls/Dockerfile-uls_service

docker build . -t msghnd -f msghnd/Dockerfile

docker build . -t ratdb -f ratdb/Dockerfile

docler build . -t objst -f objstorage/Dockerfile

docker build . -t rmq -f rabbitmq/Dockerfile

docker build . -t dispatcher -f dispatcher/Dockerfile

docker build . -t cert_db -f cert_db/Dockerfile

docker build . -t rcache -f rcache/Dockerfile

cd als && docker build . -t als_siphon   -f Dockerfile.siphon; cd ../
cd als && docker build . -t als_kafka    -f Dockerfile.kafka; cd ../
cd bulk_postgres && docker build . -t bulk_postgres -f Dockerfile; cd ../
```

### celery worker prereq containers
```
docker build . -t worker-preinst -f worker/Dockerfile.preinstall

docker build . -t worker-build -f worker/Dockerfile.build
```
to build the worker using local preq containers insted of public:
```
docker build . -t worker -f worker/Dockerfile --build-arg PRINST_NAME=worker-preinst --build-arg PRINST_TAG=local --build-arg BLD_NAME=worker-build  --build-arg BLD_TAG=local
```

Once built, docker images are usable as usual docker image.

## Prereqs
Significant to know that the container needs several mappings to work properly:

1) All databases in one folder - map to /mnt/nfs/rat_transfer
      ```
      /var/databases:/mnt/nfs/rat_transfer
      ```
      Those databases are:
      - 3dep
      - daily_uls_parse
      - databases
      - globe
      - itudata
      - nlcd
      - population
      - proc_gdal
      - proc_lidar_2019
      - RAS_Database
      - srtm3arcsecondv003
      - ULS_Database
      - nfa
      - pr


2) LiDAR Databases to /mnt/nfs/rat_transfer/proc_lidar_2019
      ```
      /var/databases/proc_lidar_2019:/mnt/nfs/rat_transfer/proc_lidar_2019
      ```
3) RAS database to /mnt/nfs/rat_transfer/RAS_Database
      ```
      /var/databases/RAS_Database:/mnt/nfs/rat_transfer/RAS_Database
      ```
4) Actual ULS Databases to /mnt/nfs/rat_transfer/ULS_Database
      ```
      /var/databases/ULS_Database:/mnt/nfs/rat_transfer/ULS_Database
      ```
5) Folder with daily ULS Parse data /mnt/nfs/rat_transfer/daily_uls_parse
      ```
      /var/databases/daily_uls_parse:/mnt/nfs/rat_transfer/daily_uls_parse
      ```
6) Folder with AFC Config data /mnt/nfs/afc_config (now can be moved to Object Storage by default)
      ```
      /var/afc_config:/mnt/nfs/afc_config
      ```
**NB: All or almost all files and folders should be owned by user and group 1003 (currently - fbrat)**

This can be applied via following command (mind the real location of these folders on your host system):

```
chown -R 1003:1003 /var/databases /var/afc_config
```

**Please post comment or open issue with request to get access to the openAFC database initial archive.**

It also needs access to the PostgreSQL 9 Database server with _fbrat_ database with special structure. (see [separate](#postgresql-structure) paragraph)

Default access to the database is described in the puppet files. However you can change it by overriding the default config (located in _/etc/xdg/fbrat/ratapi.conf_ or changing corresponding puppet variables).

## docker-compose

You would probably like to use docker-compose for setting up everything together - in this case feel free to use following docker-compose.yaml file as reference.
also check [docker-compose.yaml](/tests/regression/docker-compose.yaml) and [.env](/tests/regression/.env) files from tests/regression directory, which are used by OpenAFC CI  

```
version: '3.2'
services:
  ratdb:
    image: public.ecr.aws/w9v6y1o0/openafc/ratdb-image:${TAG:-latest}
    restart: always
    dns_search: [.]

  rmq:
    image: public.ecr.aws/w9v6y1o0/openafc/rmq-image:${TAG:-latest}
    restart: always
    dns_search: [.]

  dispatcher:
    image: public.ecr.aws/w9v6y1o0/openafc/dispatcher-image:${TAG:-latest}
    restart: always
    ports:
      - "${EXT_PORT}:80"
      - "${EXT_PORT_S}:443"
    volumes:
      - ${VOL_H_NGNX:-/tmp}:${VOL_C_NGNX:-/dummyngnx}
    environment:
      - AFC_SERVER_NAME=${AFC_SERVER_NAME:-_}
      - AFC_ENFORCE_HTTPS=${AFC_ENFORCE_HTTPS:-TRUE}
      - AFC_MSGHND_NAME=msghnd
      - AFC_MSGHND_PORT=8000
      - AFC_WEBUI_NAME=rat_server
      - AFC_WEBUI_PORT=80
      # set AFC_ENFORCE_MTLS if required to enforce mTLS check
      - AFC_ENFORCE_MTLS=true
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
    depends_on:
      - msghnd
      - rat_server
    dns_search: [.]

  rat_server:
    image: 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server:${TAG:-latest}
    volumes:
      - ${VOL_H_DB}:${VOL_C_DB}
      - ./pipe:/pipe
    depends_on:
      - ratdb
      - rmq
      - objst
      - als_kafka
      - als_siphon
      - bulk_postgres
      - rcache
    secrets:
      - NOTIFIER_MAIL.json
      - OIDC.json
      - REGISTRATION.json
      - REGISTRATION_CAPTCHA.json
    dns_search: [.]
    environment:
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # ALS params
      - ALS_KAFKA_SERVER_ID=rat_server
      - ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - ALS_KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres:/rcache
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache

  msghnd:
    image: 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-msghnd:${TAG:-latest}
    environment:
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # ALS params
      - ALS_KAFKA_SERVER_ID=msghnd
      - ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - ALS_KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres:/rcache
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache
    dns_search: [.]
    depends_on:
      - ratdb
      - rmq
      - objst
      - als_kafka
      - als_siphon
      - bulk_postgres
      - rcache

  objst:
    image: public.ecr.aws/w9v6y1o0/openafc/objstorage-image:${TAG:-latest}
    environment:
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_HIST_PORT=4999
      - AFC_OBJST_LOCAL_DIR=/storage
    dns_search: [.]
  worker:
    image: 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-worker:${TAG:-latest}
    volumes:
      - ${VOL_H_DB}:${VOL_C_DB}
      - ./pipe:/pipe
    environment:
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # worker params
      - AFC_WORKER_CELERY_OPTS=rat_1 rat_2
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # afc-engine preload lib params
      - AFC_AEP_ENABLE=1
      - AFC_AEP_DEBUG=1
      - AFC_AEP_REAL_MOUNTPOINT=${VOL_C_DB}/3dep/1_arcsec
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache
    depends_on:
      - ratdb
      - rmq
      - objst
      - rcache
      - als_kafka
    dns_search: [.]

  als_kafka:
    image: public.ecr.aws/w9v6y1o0/openafc/als-kafka-image:${TAG:-latest}
    restart: always
    environment:
      - KAFKA_ADVERTISED_HOST=${ALS_KAFKA_SERVER_}
      - KAFKA_CLIENT_PORT=${ALS_KAFKA_CLIENT_PORT_}
      - KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
    dns_search: [.]

  als_siphon:
    image: public.ecr.aws/w9v6y1o0/openafc/als-siphon-image:${TAG:-latest}
    restart: always
    environment:
      - KAFKA_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - POSTGRES_HOST=bulk_postgres
      - INIT_IF_EXISTS=skip
      - KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
    depends_on:
      - als_kafka
      - bulk_postgres
    dns_search: [.]

  bulk_postgres:
    image: public.ecr.aws/w9v6y1o0/openafc/bulk-postgres-image:${TAG:-latest}
    dns_search: [.]

  uls_downloader:
    image: public.ecr.aws/w9v6y1o0/openafc/uls-downloader:${TAG:-latest}
    restart: always
    environment:
      - ULS_STATE_DIR=${VOL_C_ULS_STATE}
      - ULS_AFC_URL=http://msghnd:8000/fbrat/ap-afc/availableSpectrumInquiryInternal?nocache=True
      - ULS_DELAY_HR=1
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_PORT}
    volumes:
      - ${VOL_H_DB}/ULS_Database:/ULS_Database
      - uls_downloader_state:${VOL_C_ULS_STATE}
    secrets:
      - NOTIFIER_MAIL.json
    dns_search: [.]

  cert_db:
    image: public.ecr.aws/w9v6y1o0/openafc/cert_db:${TAG:-latest}
    depends_on:
      - ratdb
    links:
      - ratdb
      - als_kafka
    environment:
      - ALS_KAFKA_SERVER_ID=cert_db
      - ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - ALS_KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}

  rcache:
    image: public.ecr.aws/w9v6y1o0/openafc/rcache-image:${TAG:-latest}
    restart: always
    environment:
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_PORT=${RCACHE_PORT}
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres:/rcache
      - RCACHE_AFC_REQ_URL=http://msghnd:8000/fbrat/ap-afc/availableSpectrumInquiry?nocache=True
      - RCACHE_RULESETS_URL=http://rat_server/fbrat/ratapi/v1/GetRulesetIDs
      - RCACHE_CONFIG_RETRIEVAL_URL=http://rat_server/fbrat/ratapi/v1/GetAfcConfigByRulesetID
    depends_on:
      - bulk_postgres
    dns_search: [.]

volumes:
  uls_downloader_state:

secrets:
    NOTIFIER_MAIL.json:
        file: ${VOL_H_SECRETS}/NOTIFIER_MAIL.json
    OIDC.json:
        file: ${VOL_H_SECRETS}/OIDC.json
    REGISTRATION.json:
        file: ${VOL_H_SECRETS}/REGISTRATION.json
    REGISTRATION_CAPTCHA.json:
        file: ${VOL_H_SECRETS}/REGISTRATION_CAPTCHA.json

```
`.env` file used with the docker-compose.yaml. please read comments in the file and update it accordingly 
```
# --------------------------------------------------- #
# docker-compose.yaml variables                       #
# convention: Host volume VOL_H_XXX will be mapped    #
# as container's volume VOL_C_YYY                     #
# VOL_H_XXX:VOL_C_YYY                                 #
# --------------------------------------------------- #

# -= MUST BE defined =-
# Hostname for AFC server
AFC_SERVER_NAME="_"
# Wether to forward all http requests to https
AFC_ENFORCE_HTTPS=TRUE

# Host static DB root dir
VOL_H_DB=/opt/afc/databases/rat_transfer
# Container's static DB root dir
VOL_C_DB=/mnt/nfs/rat_transfer

# http port
EXT_PORT=80

# https host port
EXT_PORT_S=443

# -= ALS CONFIGURATION STUFF =-

# Port on which ALS Kafka server listens for clients
ALS_KAFKA_CLIENT_PORT_=9092

# ALS Kafka server host name
ALS_KAFKA_SERVER_=als_kafka

# Maximum ALS message size (default 1MB is too tight for GUI AFC Response)
ALS_KAFKA_MAX_REQUEST_SIZE_=10485760

# -= FS(ULS) DOWNLOADER CONFIGURATION STUFF =-

# Symlink pointing to current ULS database
ULS_CURRENT_DB_SYMLINK=FS_LATEST.sqlite3

# Directory where to ULS status volume is mounted
VOL_C_ULS_STATE=/uls_downloader_state


# -= RCACHE SERVICE CONFIGURATION STUFF =-

# True (1, t, on, y, yes) to enable use of Rcache. False (0, f, off, n, no) to
# use legacy file-based cache. Default is True
RCACHE_ENABLED=True

# Port Rcache service listens os
RCACHE_PORT=8000


# -= SECRETS STUFF =-

# Host directory containing secret files
VOL_H_SECRETS=../../tools/secrets/empty_secrets
#VOL_H_SECRETS=/opt/afc/secrets

# Directory inside container where to secrets are mounted (always /run/secrets
# in Compose, may vary in Kubernetes)
VOL_C_SECRETS=/run/secrets


# -= OPTIONAL =-
# to work without tls/mtls,remove these variables from here  
# if you have tls/mtls configuration, keep configuration 
# files in these host volumes
VOL_H_SSL=./ssl
VOL_C_SSL=/etc/httpd/certs
VOL_H_NGNX=./ssl/nginx
VOL_C_NGNX=/certificates/servers

```


Just create this file on the same level with Dockerfile (don't forget to update paths to resources accordingly) and you are almost ready.
Just run in this folder following command and it is done:
```
docker-compose up -d
```

Keep in mind that on the first run it will build and pull all the needed containers and it can take some time (based on your machine power)

After the initial start of the server we recommend to stop it and then start again using these commands:
```
docker-compose down
docker-compose up -d
```

If you later need to rebuild the server with the changes - simply run this command:
```
docker-compose build
```
and then restart it.
To force rebuild it completely use _--no-cache_ option:

```
docker-compose build --no-cache
```

**NB: the postgres:9 container requires the folder /mnt/nfs/pgsql/data to be owned by it's internal user and group _postgres_, which both have id 999.**

You can achieve it this way  (mind the real location of these folders on your host system):
```
chown 999:999 /var/databases/pgdata
```

## **Environment variables**
|Name|Default val|Container|Notes
| :- | :- | :- | :- |
| **RabbitMQ settings**||||
|BROKER_TYPE|`internal`|rat-server,msghnd,worker | whether `internal` or `external` AFC RMQ service used|
|BROKER_PROT|`amqp` |rat-server,msghnd,worker | what protocol used for AFC RMQ service|
|BROKER_USER|`celery`|rat-server,msghnd,worker | user used for AFC RMQ service|
|BROKER_PWD |`celery`|rat-server,msghnd,worker | password used for AFC RMQ service|
|BROKER_FQDN|`localhost`|rat-server,msghnd,worker | IP/domain name of AFC RMQ service|
|BROKER_PORT|`5672`|rat-server,msghnd,worker | port of AFC RMQ service|
|RMQ_LOG_CONSOLE_LEVEL|warning|rmq|RabbitMQ console log level (debug, info, warning, error, critical, none)|
| **AFC Object Storage** |||please read [objst README.md](/objstorage/README.md)|
|AFC_OBJST_HOST|`0.0.0.0`|objst,rat-server,msghnd,worker|file storage service host domain/IP|
|AFC_OBJST_PORT|`5000`|objst,rat-server,msghnd,worker|file storage service port|
|AFC_OBJST_SCHEME|'HTTP'|rat-server,msghnd,worker|file storage service scheme. `HTTP` or `HTTPS`|
|AFC_OBJST_MEDIA|`LocalFS`|objst|The media used for storing files by the service. The possible values are `LocalFS` - store files on docker's FS. `GoogleCloudBucket` - store files on Google Store|
|AFC_OBJST_LOCAL_DIR|`/storage`|objst|file system path to stored files in file storage container. Used only when `AFC_OBJST_MEDIA` is `LocalFS`|
|AFC_OBJST_LOG_LVL|`ERROR`|objst|logging level of the file storage. The relevant values are `DEBUG` and `ERROR`|
|AFC_OBJST_HIST_PORT|`4999`|objst,rat-server,msghnd,worker|history service port|
|AFC_OBJST_WORKERS|`10`|objst|number of gunicorn workers running objst server|
|AFC_OBJST_HIST_WORKERS|`2`|objst|number of gunicorn workers runnining history server|
| **MSGHND settings**||||
|AFC_MSGHND_BIND|`0.0.0.0`|msghnd| the socket to bind. a string of the form: <host>|
|AFC_MSGHND_PORT|`8000`|msghnd| the port to use in bind. a string of the form: <port>|
|AFC_MSGHND_PID|`/run/gunicorn/openafc_app.pid`|msghnd| a filename to use for the PID file|
|AFC_MSGHND_WORKERS|`20`|msghnd| the number of worker processes for handling requests|
|AFC_MSGHND_TIMEOUT|`180`|msghnd| workers silent for more than this many seconds are killed and restarted|
|AFC_MSGHND_ACCESS_LOG||msghnd| the Access log file to write to. Default to don't. Use `/proc/self/fd/2` for console|
|AFC_MSGHND_ERROR_LOG|`/proc/self/fd/2`|msghnd| the Error log file to write to|
|AFC_MSGHND_LOG_LEVEL|`info`|msghnd| The granularity of Error log outputs (values are 'debug', 'info', 'warning', 'error', 'critical'|
| **worker settings**|||please read [afc-engine-preload README.md](/src/afc-engine-preload/README.md)|
|AFC_AEP_ENABLE|Not defined|worker|Enable the preload library if defined|
|AFC_AEP_FILELIST|`/aep/list/aep.list`|worker|Path to file tree info file|
|AFC_AEP_DEBUG|`0`|worker|Log level. 0 - disable, 1 - log time of read operations|
|AFC_AEP_LOGFILE|`/aep/log/aep.log`|worker|Where to write the log|
|AFC_AEP_CACHE|`/aep/cache`|worker|Where to store the cache|
|AFC_AEP_CACHE_MAX_FILE_SIZE|`50000000`|worker|Cache files with size less than the value|
|AFC_AEP_CACHE_MAX_SIZE|`1000000000`|worker|Max cache size|
|AFC_AEP_REAL_MOUNTPOINT|`/mnt/nfs/rat_transfer`|worker|Redirect read access to there|
|AFC_AEP_ENGINE_MOUNTPOINT|value of AFC_AEP_REAL_MOUNTPOINT|worker|Redirect read access from here|
|AFC_WORKER_CELERY_OPTS|`rat_1`|worker|Celery app instance to use|
|AFC_WORKER_CELERY_LOG|`INFO`|worker|Celery log level. `ERROR` or `INFO` or `DEBUG`|
|AFC_ENGINE_LOG_LVL|'info'|worker|afc-engine log level|
|AFC_MSGHND_NAME|msghnd|dispatcher|Message handler service hostname|
|AFC_MSGHND_PORT|8000|dispatcher|Message handler service HTTP port|
|AFC_WEBUI_NAME|rat_server|dispatcher|WebUI service hostname|
|AFC_WEBUI_PORT|80|dispatcher|WebUI service HTTP Port|
|AFC_ENFORCE_HTTPS|TRUE|dispatcher|Wether to enforce forwarding of HTTP requests to HTTPS. TRUE - for enable, everything else - to disable|
|AFC_SERVER_NAME|"_"|dispatcher|Hostname of the AFC Server, for example - "openafc.tip.build". "_" - will accept any hostname (but this is not secure)|
| **RCACHE settings** ||||
|RCACHE_ENABLED|TRUE|rcache, rat_server, msghnd, worker, uls_downloader|TRUE if Rcache enabled, FALSE to use legacy objstroage response cache|
|RCACHE_POSTGRES_DSN|Must be set|rcache, rat_server, msghnd|Connection string to Rcache Postgres database|
|RCACHE_SERVICE_URL|Must be set|rat_server, msghnd, worker, uls_downloader|Rcache service REST API base URL|
|RCACHE_RMQ_DSN|Must be set|rat_server, msghnd, worker|AMQP URL to RabbitMQ vhost that workers use to communicate computation result|
|RCACHE_UPDATE_ON_SEND|TRUE|TRUE if worker sends result to Rcache server, FALSE if msghnd/rat_server|
|RCACHE_PORT|8000|rcache|Rcache REST API port|
|RCACHE_AFC_REQ_URL||REST API Rcache precomputer uses to send invalidated AFC requests for precomputation. No precomputation if not set|
|RCACHE_RULESETS_URL||REST API Rcache spatial invalidator uses to retrieve AFC Configs' rulesets. Default invalidation distance usd if not set|
|RCACHE_CONFIG_RETRIEVAL_URL||REST API Rcache spatial invalidator uses to retrieve AFC Config by ruleset. Default invalidation distance usd if not set|


## RabbitMQ settings

There is a way to conifugre AFC server to use a RabbitMQ broker from different docker image.
Following the list of environment variables you may configure a server to use 'external' Rabbit MQ instance.
```
BROKER_TYPE = external
BROKER_PROT = amqp
BROKER_USER = celery
BROKER_PWD  = celery
BROKER_FQDN = <ip address>
BROKER_PORT = 5672
BROKER_MNG_PORT = 15672
```
Following the example to use RabbitMQ service in docker-compose.
```
  rmq:
    image: public.ecr.aws/w9v6y1o0/openafc/rmq-image:latest
    restart: always
```

## PostgreSQL structure

On the first start of the PostgreSQL server there are some initial steps to do. First to create the database. Its default name now is **fbrat**. If you are using compose script described above, everything will be done automatically.

After that, once OpenAFC server is started, you need to create DB structure for the user database. This can be done using a _rat-manage-api_ utility.

```
rat-manage-api db-create
```

If you do it with the server which is run thru the docker-compose script described above, you can do it using this command:
```
docker-compose exec server rat-manage-api db-create
```

### Upgrade PostgreSQL

When PostgreSQL is upgraded the pgdata should be converted to be compatible with the new PostgreSQL version. It can be done by tools/db_tools/update_db.sh script.
```
tools/db_tools/update_db.sh [pgdata_dir] [postgres_password] [old_postgres_version] [new_postgres_version]
```
This script makes a backup of [pgdata_dir] to [pgdata_dir].back and puts the converted db in [pgdata_dir].
This command should be run under root permissions, i.e. 'sudo tools/db_tools/update_db.sh ...'

Example: convert db which was created by PostgreSQL version 9.6 to be used by PostgreSQL version 14.7:
```
sudo tools/db_tools/update_db.sh ./pgdata qwerty 9.6 14.7
```

## Initial Super Administrator account

Once done with database and starting the server, you need to create default administrative user to handle your server from WebUI. It is done from the server console using the _rat-manage-api_ utility.

If you are running from the compose file described above, you first need to get the OpenAFC server console.
```
docker-compose exec server bash
```
it will return something like this:
```
[root@149372a2ac05 wd]#
```
this means you are in.

By default, the login uses non OIDC login method which manages user accounts locally.  You can use the following command to create an administrator for your OpenAFC server.

```
rat-manage-api user create --role Super --role Admin --role AP --role Analysis admin "Enter Your Password Here"
```

Once done, you can authorize with this user and password in WebUI.
To exit the console press Ctrl+D or type the 'exit' command.

If you would like to use OIDC login method, please read [OIDC_Login.md](/OIDC_Login.md)

### Note for an existing user database

Database format has changed over time.  If your user database uses older format, you might find errors indicating missing database fields upon bootup and login. The error message has instructions on how to migrate the database. These steps apply whether you're using OIDC or non OIDC login method.  You have sereral options:

**1. Reinitialize the database without users:**

```
rat-manage-api db-drop
rat-manage-api db-create
```

This will wipe out existing users, e.g. user acounts need to be manually recreated again.

**2. Migrate the database with users:**

```
rat-manage-api db-upgrade
```
## Managing user account
Users can be created and removed. User roles can be added and removed.
Remove user with user remove command, e.g.:
```
rat-manage-api user remove user@mycompany.com

```
Update user roles with user update command, e.g.:
```
rat-manage-api user update --role Admin --role AP --role Analysis --email "user@mycompany.com"
```
Create user with user create command.  If org argument is not given, the organization can be derived from the username if it's given in the form of an email address e.g.:
```
rat-manage-api user create --role Admin --role AP --role Analysis --org mycompany.com "username" "mypassword'

```
## User roles
Roles are: Super, admin, AP, Admin, Analysis, Trial
"Super" is the highest level role, which allows access rights to all organizations, as opposed to "Admin", which is limited to one organization.  When upgrade from older system without "Super", you will need to decide which users to be assigned role of "Super" and update their roles via the user update command.

## MTLS
Vanilla installation comes with placeholder file for client certificate bundle.

Besides the GUI, mtls certificates can be managed via CLI.
To list certificates:
```
rat-manage-api mtls list
```

To add certificates:
```
rat-manage-api mtls create --src <certificate file> --org <Organization name> --note <Short Note>
```

To remove certificates:
```
rat-manage-api mtls remove --id <certificate id obtained from list command>
```
To dump a certificate to a file:
```
rat-manage-api mtls dump --id <certificate id obtained from list command> --dst <file name>
```

Happy usage!

## ULS database update automation

ULS Database needs (preferrably daily) updates to be up-to-date with regulators requirements. So there is a set of scripts in the uls/ folder which automates this activity.

Prerequisites:
- Set up and running Jenkins server
- At least one of jenkins executors have access to the folder where the ULS database is stored (in our example it will be "/var/databases/ULS_Database") should it be the executor on the same server where the afc engine runs or it should have access to the folder thru NFS - it is up to your choice. It should have a 'uls' label to make this pipeline run on it.
- SSH key for (at least) read only access to this github repository

First Log in to your Jenkins WebUI and create a new Pipeline (Dashboard -> + New Item)
On the next screen set up the the name of the project (for example "ULS Update") and choose "Pipeline" option and click OK.

On the next screen there are a lot of buttons for configuration and set up, most of them are up to you.

It is essential to do following configuration:

    1. Check option "This project is parameterized".A button "Add parameter" will appear.
    2. Add parameter of type "Credentials Parameter". Give it name "GITHUB_CREDS_ID" (without quotes) and "SSH username with private key" as a Credential type. Check the option "Required" and set as a default value the credential mentioned in prerequisites.
    3. In the same section add another parameter. Now of type "String Parameter" with name "ULS_FOLDER" and default value mentioned in prereqisites (in this example - "/var/databases/ULS_Database")
    4. In section Pipeline choose option "Pipeline script form SCM".
        4.1 In SCM Type choose GIT.
        4.2 In repository specify the ssh path to repository (in our case - "git@github.com:Telecominfraproject/open-afc.git"). In credentials Dropdown menu choose the valid credetials to this repository.
        4.3 In "Branches to build" use "*/main" (without quotes)
        4.4 In "Script Path" specify the path from github repository root to jenkinsfile ( uls/ULS_updater.Jenkinsfile )


All the other configurations are up to you, but it is recommended to use following options:

Check option "Do not allow concurrent builds"

In Build Triggers section choose "Build Periodically" option and as a schedule type following:
```cron
H H/24 * * *
```
This will make ULS update run once every 24 hours.

After that choose "Poll SCM" and in schedule type following:
```cron
H/5 * * * *
```

this will make jenkins to check changes in SCM and run the update if any change in Pipeline had happened.

And click "Save". That's it! If everything is done right, after the next run of this pipeline, the new ULS Database will appear in the ULS_Database folder.
