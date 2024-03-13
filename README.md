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

This document describes the procedure for submitting the source code changes to the openAFC github project. Procedure described in this document requires access to the openAFC project and knowledge of the GIT usage. Please contact TBD@TBD.com in case you need access to the openAFC project.

Github.com can be referred for [details of alternate procedures for creating the pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests), developers can use any of these methods but need to include change description as part of pull requests  description.

OpenAFC conforms to all the requirements from FCC per [6GHz Report & Order](https://docs.fcc.gov/public/attachments/DOC-363490A1.pdf) and FCC 47 CFR Part 15.407 for unlicensed standard power devices in the 6 GHz band.

In addition, OpenAFC fully conforms to WinnForum’s Functional Requirements for the U.S. 6 GHz Band under the Control of an AFC System in WINNF-TS-1014-V1.4.0 ([https://6ghz.wirelessinnovation.org/baseline-standards](https://6ghz.wirelessinnovation.org/baseline-standards)). This includes some of the implementation details – for example correction of FS parameters in the ULS database, FS antenna pattern, FS noise power and feederloss to use, calculation of near-field adjustment factor, calculation of interference to FS links with passive sites and diversity receivers, path loss models and their parameters, etc.
Finally, OpenAFC fully conforms to the implementation details specified in [WFA SUT Test Plan v1.5](https://www.wi-fi.org/file/afc-specification-and-test-plans).

OpenAFC software deployment consists of multiple containers, and it can be deployed on a standalone system for test and development purposes via the provided docker-compose based solution.  Instructions on how to build the containers and a sample docker-compose.yaml can be found in the [OpenAFC Engine Server usage in Docker Environment](#afc-engine-build-in-docker).

OpenAFC software can also be deployed for production using the Kubernetes framework. Please refer to the readme-kubernetes.md for the instructions.

The sample docker-compose.yaml assumes that the required databases (e.g. terrain, landcover, winnforum databases, etc.) have been obtained and placed in an accessible folder according to the information in [database_readme.md](https://github.com/Telecominfraproject/open-afc/blob/main/database_readme.md
) on Github.
	
Many of the components have additional README files inside folders that describe the additional configuration for each component.  Default values are provided either inside the component or in the sample files that will work to stand up the system.

Note that this sample does not provide working SSL certificates for authentication to the server.

<br /><br />

# **Contributing**

All contributions are paused while we evaluate a new process for maintaining changes.

<br /><br />

# **How to Build**
# AFC Engine build in docker and compose setup

## Installing docker engine
Docker engine instructions specific to your OS are available on the [Docker official website](https://docs.docker.com/engine/install/)

## Building the Docker images

### Prerequisites:

Currently, all the prerequisites to build the containers for the system (except docker installation) are situated in this repository. All you need is to clone OpenAFC locally to your working directory and start all following commands from there.

In order to run the system, you will need to construct a data volume and make it available to the containers. See [database_readme.md](database_readme.md) for details on what data is required.

### Building Docker image from Dockerfiles

There is a script that builds all container used by the AFC service.
This script is used by automatic test infrastructure. Please check [tests/regression](/tests/regression/) dir.

This script uses two environment variables PRIV_REPO and PUB_REPO to determine what repository to push images to. These values default to those used by the regression test infrastructure, but you should define them to refer to your repository. Note that the script 

### Using scripts from the code base

To rebuild and tag all containers in your local docker repository, use this script:
```
cd open-afc
tests/regression/build_imgs.sh `pwd` my_tag 0
```
after the build, check all new containers:
```
docker images | grep my_tag
```
these containers are used by [tests/regression/run_srvr.sh](/tests/regression/run_srvr.sh)

### To 'manually' build containers one by one:
```
cd open-afc

docker build . -t rat_server -f rat_server/Dockerfile

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
to build the worker using local preq containers:
```
docker build . -t worker -f worker/Dockerfile --build-arg PRINST_NAME=worker-preinst --build-arg PRINST_TAG=local --build-arg BLD_NAME=worker-build  --build-arg BLD_TAG=local
```
### Prometheus Monitoring images
If you wish to use [Prometheus](https://prometheus.io/) to montitor your system, you can build these images
```
cd prometheus && docker build . -t  Dockerfile-prometheus -t prometheus-image ; cd ../
cd /prometheus && docker build . Dockerfile-cadvisor -t cadvisor-image ; cd ../
cd /prometheus && docker build . Dockerfile-nginxexporter -t nginxexporter-image  ; cd ../
cd /prometheus && docker build . Dockerfile-grafana -t grafana-image ; cd ../
```

Once built, docker images are usable as usual docker image.

## Building OpenAFC engine

If you wish to build or run the engine component outside of the entire OpenAFC system, you can build the docker images that are needed for only that component.

**NB:** "-v" option in docker maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/afc&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/afc/


goto the project dir
```
cd open-afc
```
If you have not already, build the worker build image
```
docker build . -t worker-build -f worker/Dockerfile.build
```

run shell of alpine docker-for-build shell
```
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc worker-build:latest ash
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
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc -v /opt/afc/worker:latest sh
```
inside the worker container execute the afc-engine app
```
./afc/build/src/afc-engine/afc-engine
```

## Prereqs
OpenAFC containers needs several mappings to work properly.  Assuming that you are using /var/databases on your host to store the databases, you can select either option 1 here (which is assumed in the docker compose shown below) or set mappings individually as shown in 2-6.   

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

## docker-compose

You would probably like to use docker-compose for setting up everything together - in this case feel free to use following docker-compose.yaml file as reference.
also check [docker-compose.yaml](/tests/regression/docker-compose.yaml) and [.env](/tests/regression/.env) files from tests/regression directory, which are used by OpenAFC CI.

Note that the image tags here are the ones from the [manual build](#to-manually-build-containers-one-by-one) not the ones used by the [script build](#using-scripts-from-the-code-base).

```
version: '3.2'
services:
  ratdb:
    image: ratdb:${TAG:-latest}
    restart: always
    dns_search: [.]

  rmq:
    image: rmq:${TAG:-latest}
    restart: always
    dns_search: [.]

  dispatcher:
    image: dispatcher:${TAG:-latest}
    restart: always
    ports:
      - "${EXT_PORT}:80"
      - "${EXT_PORT_S}:443"
    volumes:
      - ${VOL_H_NGNX:-/tmp}:${VOL_C_NGNX:-/dummyngnx}
    environment:
      - AFC_SERVER_NAME=${AFC_SERVER_NAME:-_}
      - AFC_ENFORCE_HTTPS=${AFC_ENFORCE_HTTPS:-TRUE}
      # set to true if required to enforce mTLS check
      - AFC_ENFORCE_MTLS=false
      - AFC_MSGHND_NAME=msghnd
      - AFC_MSGHND_PORT=8000
      - AFC_WEBUI_NAME=rat_server
      - AFC_WEBUI_PORT=80
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
    depends_on:
      - msghnd
      - rat_server
    dns_search: [.]

  rat_server:
    image: rat_server:${TAG:-latest}
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
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache

  msghnd:
    image: msghnd:${TAG:-latest}
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
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}
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
    image: objst:${TAG:-latest}
    environment:
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_HIST_PORT=4999
      - AFC_OBJST_LOCAL_DIR=/storage
    dns_search: [.]

  worker:
    image: worker:${TAG:-latest}
    volumes:
      - ${VOL_H_DB}:${VOL_C_DB}
      - ./pipe:/pipe
    environment:
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # worker params
      - AFC_WORKER_CELERY_WORKERS=rat_1 rat_2
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # afc-engine preload lib params
      - AFC_AEP_ENABLE=1
      - AFC_AEP_DEBUG=1
      - AFC_AEP_REAL_MOUNTPOINT=${VOL_C_DB}/3dep/1_arcsec
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}
      - RCACHE_RMQ_DSN=amqp://rcache:rcache@rmq:5672/rcache
      # ALS params
      - ALS_KAFKA_SERVER_ID=worker
      - ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS=${ALS_KAFKA_SERVER_}:${ALS_KAFKA_CLIENT_PORT_}
      - ALS_KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
    depends_on:
      - ratdb
      - rmq
      - objst
      - rcache
      - als_kafka
    dns_search: [.]

  als_kafka:
    image: als-kafka:${TAG:-latest}
    restart: always
    environment:
      - KAFKA_ADVERTISED_HOST=${ALS_KAFKA_SERVER_}
      - KAFKA_CLIENT_PORT=${ALS_KAFKA_CLIENT_PORT_}
      - KAFKA_MAX_REQUEST_SIZE=${ALS_KAFKA_MAX_REQUEST_SIZE_}
    dns_search: [.]

  als_siphon:
    image: als-siphon:${TAG:-latest}
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
      - ULS_SERVICE_STATE_DB_DSN=postgresql://postgres:postgres@bulk_postgres/fs_state
      - ULS_AFC_URL=http://msghnd:8000/fbrat/ap-afc/availableSpectrumInquiryInternal?nocache=True
      - ULS_DELAY_HR=1
      - ULS_PROMETHEUS_PORT=8000
      # Rcache parameters
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}
    volumes:
      - ${VOL_H_DB}/ULS_Database:/rat_transfer/ULS_Database
      - ${VOL_H_DB}/RAS_Database:/rat_transfer/RAS_Database
    secrets:
      - NOTIFIER_MAIL.json
    dns_search: [.]

  cert_db:
    image: cert_db:${TAG:-latest}
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
    image: rcache:${TAG:-latest}
    restart: always
    environment:
      - RCACHE_ENABLED=${RCACHE_ENABLED}
      - RCACHE_CLIENT_PORT=${RCACHE_CLIENT_PORT}
      - RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache
      - RCACHE_AFC_REQ_URL=http://msghnd:8000/fbrat/ap-afc/availableSpectrumInquiry?nocache=True
      - RCACHE_RULESETS_URL=http://rat_server/fbrat/ratapi/v1/GetRulesetIDs
      - RCACHE_CONFIG_RETRIEVAL_URL=http://rat_server/fbrat/ratapi/v1/GetAfcConfigByRulesetID
    depends_on:
      - bulk_postgres
    dns_search: [.]

  grafana:
    image: grafana-image:${TAG:-latest}
    restart: always
    depends_on:
      - prometheus
      - bulk_postgres
    dns_search: [.]

  prometheus:
    image: prometheus-image:${TAG:-latest}
    restart: always
    depends_on:
      - cadvisor
      - nginxexporter
    dns_search: [.]

  cadvisor:
    image:  cadvisor-image:${TAG:-latest}
    restart: always
    volumes:
    - /:/rootfs:ro
    - /var/run:/var/run:rw
    - /sys:/sys:ro
    - /var/lib/docker/:/var/lib/docker:ro
    - /dev/disk/:/dev/disk:ro
    dns_search: [.]

  nginxexporter:
    image: nginxexporter-image:${TAG:-latest}
    restart: always
    depends_on:
      - dispatcher
    dns_search: [.]

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
VOL_H_DB=/var/databases/rat_transfer

# Container's static DB root dir (dont change it !)
VOL_C_DB=/mnt/nfs/rat_transfer

#RAT user to be used in containers
UID=1003
GID=1003

# AFC service external PORTs configuration
# syntax:
# [IP]:<port | portN-portM>
# like 172.31.11.188:80-180
# where:
#  IP is  172.31.11.188
#  port range is 80-180

# Here we configuring range of external ports to be used by the service
# docker-compose randomly uses one port from the range

# Note 1:
# The IP arrdess can be skipped if there is only one external
# IP address (i.e. 80-180 w/o IP address is acceptable as well)

# Note 2:
# range of ports can be skipped . and just one port is acceptable as well

# all these valuase are acaptable:
# PORT=172.31.11.188:80-180
# PORT=172.31.11.188:80
# PORT=80-180
# PORT=80


# http ports range
EXT_PORT=80

# https host ports range
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


# -= RCACHE SERVICE CONFIGURATION STUFF =-

# True (1, t, on, y, yes) to enable use of Rcache. False (0, f, off, n, no) to
# use legacy file-based cache. Default is True
RCACHE_ENABLED=True

# Port Rcache service listens os
RCACHE_CLIENT_PORT=8000


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
VOL_C_SSL=/usr/share/ca-certificates/certs
VOL_H_NGNX=./ssl/nginx
VOL_C_NGNX=/certificates/servers


```


Just create this file on the same level with Dockerfile and you are almost ready. Verify that the VOL_H_DB setting in the .env file is pointing at your host directory with the databases.

Just run in this folder following command and it is done:
```
docker-compose up -d
```

Keep in mind that on the first run it will build and pull all the needed containers and it can take some time (based on your machine power).  You may want to do a build (see [Building Docker image from Dockerfiles](#building-docker-image-from-dockerfiles))

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

**NB: the postgres container requires the folder /mnt/nfs/pgsql/data to be owned by it's internal user and group _postgres_, which both have id 999.**

You can achieve it this way  (mind the real location of these folders on your host system):
```
chown 999:999 /var/databases/pgdata
```
## Initial configuration and first user

On the first start of the PostgreSQL server there are some initial steps to do. First to create the database. Its default name now is **fbrat**. If you are using compose script described above, everything will be done automatically to prepare the database for intialization.

After that, once OpenAFC server is started, you need to create DB structure for the user database. This can be done using a _rat-manage-api_ utility.

```
rat-manage-api db-create
```

If you do it with the server which is run thru the docker-compose script described above, you can do it using this command:
```
docker-compose exec rat_server rat-manage-api db-create
```
### Initial Super Administrator account

Once done with database and starting the server, you need to create default administrative user to handle your server from WebUI. It is done from the server console using the _rat-manage-api_ utility.

If you are running from the compose file described above, you first need to get the OpenAFC server console.
```
docker-compose exec rat_server bash
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
|AFC_WORKER_CELERY_WORKERS|`rat_1`|worker|Celery worker name(s) to use|
|AFC_WORKER_CELERY_OPTS||worker|Additional celery worker options|
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
|RCACHE_CLIENT_PORT|8000|rcache|Rcache REST API port|
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

## Managing the PostgreSQL database for users

### Upgrading PostgresSQL
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
## Managing user accounts
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

ULS Database needs (preferrably daily) updates to be up-to-date with regulators requirements.  See [README in uls](uls/README.md "ULS Service ReadMe") for instructions and configuration.  The docker compose given above will create a container that runs the daily ULS update service.
