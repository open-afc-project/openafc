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
  - [Building OpenAFC engine server](#building-openafc-engine-server)
- [**OpenAFC Engine usage in Docker Environment**](#openafc-engine-usage-in-docker-environment)
- [AFC Engine build in docker](#afc-engine-build-in-docker)
  - [Building Docker Container OpenAFC engine server](#building-docker-container-openafc-engine-server)
    - [Using scripts from the code base](#using-scripts-from-the-code-base)
    - [To 'manually' build containers one by one:](#to-manually-build-containers-one-by-one)
    - [celery worker prereq containers:](#celery-worker-prereq-containers)
  - [Prereqs](#prereqs)
  - [docker-compose](#docker-compose)
  - [**Environment variables**](#environment-variables)
  - [RabbitMQ settings](#rabbitmq-settings)
  - [PostgreSQL structure](#postgresql-structure)
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

TBD
Make sure that all tests are passing before submitting a pull request.

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
docker build . -t afc-build -f dockerfiles/Dockerfile-for-build
```

### Pulling the Docker image from Docker registry

Not available currently. Possibly will be added later.

## Building OpenAFC engine server

Building and installing the OpenAFC with ninja-build is seamless - if you run the container without special command - it will execute the script from the CMD directive in Dockerfile.

**NB:** "-v" option maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/afc&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/afc/

```
docker run --rm -it -v `pwd`:/wd/afc afc-build
```
If you want to build the rpm you will need to run it with Docker:

```
docker run --rm -it -v `pwd`:/wd/afc afc-build /wd/afc/build-rpm.sh
```

To run docker with your host's user you can use --user flag like:
```
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc afc-build
```
or for rpm build:
```
docker run --rm -it --user `id -u`:`id -g` --group-add `id -G | sed "s/ / --group-add /g"` -v `pwd`:/wd/afc afc-build /wd/afc/build-rpm.sh
```

# **OpenAFC Engine usage in Docker Environment**
# AFC Engine build in docker

## Building Docker Container OpenAFC engine server
There is a script that builds all container used by the AFC service.
this script is used by automatic test infrastructure. Please check [tests/regression](/tests/regression/) dir.

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

docker build . -t msghnd -f msghnd/Dockerfile

cd nginx && docker build . -t nginx ; cd ..

cd rabbitmq/ && docker build . -t rmq ; cd ..

cd src/filestorage/ && docker build . -t objst; cd ../..
```

### celery worker prereq containers:
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
    image: postgres:9
    restart: always
    environment:
      - POSTGRES_PASSWORD=N3SF0LVKJx1RAhFGx4fcw
      - PGDATA=/mnt/nfs/pgsql/data
      - POSTGRES_DB=fbrat


  rmq:
    image: public.ecr.aws/w9v6y1o0/openafc/rmq-image:latest
    restart: always


  nginx:
    image: public.ecr.aws/w9v6y1o0/openafc/ngnx-image:latest
    restart: always
    ports:
      - "${EXT_MTLS_PORT}:443"
    volumes:
      - ${VOL_H_NGNX:-/tmp}:${VOL_C_NGNX:-/dummyngnx}
    environment:
      - AFC_MSGHND_NAME=msghnd
      - AFC_MSGHND_PORT=8000
    depends_on:
      - msghnd


  rat_server:
    image: rat_server:latest
    build:
      context: .
      dockerfile: rat_server/Dockerfile
    ports:
      - "${EXT_PORT}:80"
      - "${EXT_PORT_S}:443"
    volumes:
      - ${VOL_H_DB}:${VOL_C_DB}
      - ${VOL_H_SSL:-/tmp}:${VOL_C_SSL:-/dummy1}
      - ${VOL_H_APACH:-/tmp}:${VOL_C_APACH:-/dummy2}
      - ./pipe:/pipe
    depends_on:
      - ratdb
      - rmq
      - objst
    environment:
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      - AFC_OBJST_HIST_HOST=objst
      # worker params
      - CELERY_TYPE=external


  msghnd:
    image: msghnd
    build:
      context: .
      dockerfile: msghnd/Dockerfile
    environment:
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
    depends_on:
      - ratdb
      - rmq
      - objst


  objst:
    image: public.ecr.aws/w9v6y1o0/openafc/objstorage-image:${TAG:-latest}
    environment:
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_HIST_PORT=4999
      - AFC_OBJST_LOCAL_DIR=/storage


  worker:
    image: worker
    build:
      context: .
      dockerfile: worker/Dockerfile    
    volumes:
      - ${VOL_H_DB}:${VOL_C_DB}
    environment:
      # Filestorage params:
      - AFC_OBJST_HOST=objst
      - AFC_OBJST_PORT=5000
      - AFC_OBJST_SCHEME=HTTP
      # worker params
      - CELERY_OPTIONS=rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10
      # RabbitMQ server name:
      - BROKER_TYPE=external
      - BROKER_FQDN=rmq
    depends_on:
      - ratdb
      - rmq
      - objst
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

# Host static DB root dir
VOL_H_DB=/opt/afc/databases/rat_transfer
# Container's static DB root dir
VOL_C_DB=/mnt/nfs/rat_transfer

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


# apach https ports range
EXT_PORT=172.31.11.188:80-180

# apach https host ports range
EXT_PORT_S=172.31.11.188:443-543

# nginx external (host's) ports range.
EXT_MTLS_PORT=172.31.11.188:544-644

# -= OPTIONAL =-
# to work without tls/mtls,remove these variables from here  
# if you have tls/mtls configuration, keep configuration 
# files in these host volumes
VOL_H_SSL=./ssl
VOL_C_SSL=/etc/httpd/certs
VOL_H_APACH=./apache-conf
VOL_C_APACH=/etc/httpd/conf.d
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
| **AFC Object Storage** |||please read [objst README.md](/src/filestorage/README.md)|
|AFC_OBJST_HOST|`0.0.0.0`|objst,rat-server,msghnd,worker|file storage service host domain/IP|
|AFC_OBJST_PORT|`5000`|objst,rat-server,msghnd,worker|file storage service port|
|AFC_OBJST_SCHEME|'HTTP'|rat-server,msghnd,worker|file storage service scheme. `HTTP` or `HTTPS`|
|AFC_OBJST_MEDIA|`LocalFS`|objst|The media used for storing files by the service. The possible values are `LocalFS` - store files on docker's FS. `GoogleCloudBucket` - store files on Google Store|
|AFC_OBJST_LOCAL_DIR|`/storage`|objst|file system path to stored files in file storage container. Used only when `AFC_OBJST_MEDIA` is `LocalFS`|
|AFC_OBJST_LOG_LVL|`ERROR`|objst|logging level of the file storage. The relevant values are `DEBUG` and `ERROR`|
|AFC_OBJST_HIST_HOST|`0.0.0.0`|objst,rat-server,msghnd,worker|history service host domain/IP|
|AFC_OBJST_HIST_PORT|`4999`|objst,rat-server,msghnd,worker|history service port|
| **MSGHND settings**||||
|AFC_MSGHND_BIND|`0.0.0.0:8000`|msghnd| the socket to bind. a string of the form: <host>:<port>|
|AFC_MSGHND_PID|`/run/gunicorn/openafc_app.pid`|msghnd| a filename to use for the PID file|
|AFC_MSGHND_WORKERS|`20`|msghnd| the number of worker processes for handling requests|
|AFC_MSGHND_TIMEOUT|`180`|msghnd| workers silent for more than this many seconds are killed and restarted|
|AFC_MSGHND_ACCESS_LOG|`/proc/self/fd/2`|msghnd| the Access log file to write to|
|AFC_MSGHND_ERROR_LOG|`/proc/self/fd/2`|msghnd| the Error log file to write to|
|AFC_MSGHND_LOG_LEVEL|`info`|msghnd| The granularity of Error log outputs (values are 'debug', 'info', 'warning', 'error', 'critical'|
| **worker settings**|||please read [afc-engine-preload README.md](/src/afc-engine-preload/README.md)|
|AFC_AEP_ENABLE|Not defined|worker|Enable the preload library if defined|
|AFC_AEP_FILELIST|`/aep/list/aep.list`|worker|Path to file tree info file|
|AFC_AEP_DEBUG|`0`|worker|Log level. 0 - disable, 1 - log time of read operations|
|AFC_AEP_LOGFILE|`/aep/log/aep.log`|worker|Where to write the log|
|AFC_AEP_CACHE|`/aep/cache`|worker|Where to store the cache|
|AFC_AEP_CACHE_MAX_FILE_SIZE|`60000000`|worker|Cache files with size less than the value|
|AFC_AEP_REAL_MOUNTPOINT|`/mnt/nfs/rat_transfer`|worker|Redirect read access to there|
|AFC_AEP_ENGINE_MOUNTPOINT|value of AFC_AEP_REAL_MOUNTPOINT|worker|Redirect read access from here|
|CELERY_OPTIONS|`rat_1`|worker|Celery app instance to use|
|CELERY_LOG|`INFO`|worker|Celery log level. `ERROR` or `INFO` or `DEBUG`|


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
Users can be removed. User roles can be added and removed.
Remove user with user remove command, e.g.:
```
rat-manage-api user remove user@mycompany.com

```
Update user roles with user update command, e.g.:
```
rat-manage-api user update --role Admin --role AP --role Analysis --email "user@mycompany.com"
```
## User roles
Roles are: Super, admin, AP, Admin, Analysis, Trial
"Super" is a new role, which allows access rights to all organizations, as opposed to "Admin", which is limited to one organization.  When upgrade from older system without "Super", you will need to decide which users to be assigned role of "Super" and update their roles via the user update command.

## MTLS
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
