This work is licensed under the OpenAFC Project License, a copy of which is included in the LICENSE.txt file with this software program.

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
        - [Step 4: Make your changes](#step-4-make-your-changes)
        - [Step 5: Commit your changes](#step-5-commit-your-changes)
        - [Step 6: Rebase](#step-6-rebase)
        - [Step 7: Run the tests](#step-7-run-the-tests)
        - [Step 8: Push your branch to GitHub](#step-8-push-your-branch-to-github)
        - [Step 9: Send the pull request](#step-9-send-the-pull-request)
            - [Change Description](#change-description)
- [**How to Build**](#how-to-build)
- [AFC Engine build in docker and compose setup](#afc-engine-build-in-docker-and-compose-setup)
    - [Installing docker engine](#installing-docker-engine)
    - [Building the Docker images individually](#building-the-docker-images-individually)
        - [Prerequisites:](#prerequisites)
        - [Building Docker image from Dockerfiles](#building-docker-image-from-dockerfiles)
        - [Using scripts from the code base](#using-scripts-from-the-code-base)
        - [To 'manually' build containers one by one:](#to-manually-build-containers-one-by-one)
        - [celery worker prereq containers](#celery-worker-prereq-containers)
        - [Prometheus Monitoring images](#prometheus-monitoring-images)
    - [Data mappings](#data-mappings)
        - [Building OpenAFC engine outside of the OpenAFC system](#building-openafc-engine-outside-of-the-openafc-system)
    - [docker-compose build and run](#docker-compose-build-and-run)
    - [Initial configuration and first user](#initial-configuration-and-first-user)
        - [Initial Super Administrator account](#initial-super-administrator-account)
    - [**Environment variables**](#environment-variables)
    - [RabbitMQ settings](#rabbitmq-settings)
    - [Managing the PostgreSQL database for users](#managing-the-postgresql-database-for-users)
        - [Upgrading PostgresSQL](#upgrading-postgressql)
        - [Note for an existing user database](#note-for-an-existing-user-database)
    - [Managing user accounts](#managing-user-accounts)
    - [User roles](#user-roles)
    - [MTLS](#mtls)
    - [ULS database update](#uls-database-update-automation)



# **Introduction**

This document describes the procedure for submitting the source code changes to the openAFC github project. Procedure described in this document requires access to the openAFC project and knowledge of the GIT usage. Please contact TBD@TBD.com in case you need access to the openAFC project.

Github.com can be referred for [details of alternate procedures for creating the pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests), developers can use any of these methods but need to include change description as part of pull requests description.

OpenAFC conforms to all the requirements from FCC per [6GHz Report & Order](https://docs.fcc.gov/public/attachments/DOC-363490A1.pdf) and FCC 47 CFR Part 15.407 for unlicensed standard power devices in the 6 GHz band.

In addition, OpenAFC fully conforms to WinnForum’s Functional Requirements for the U.S. 6 GHz Band under the Control of an AFC System in WINNF-TS-1014-V1.4.0 ([https://6ghz.wirelessinnovation.org/baseline-standards](https://6ghz.wirelessinnovation.org/baseline-standards)). This includes some of the implementation details – for example correction of FS parameters in the ULS database, FS antenna pattern, FS noise power and feederloss to use, calculation of near-field adjustment factor, calculation of interference to FS links with passive sites and diversity receivers, path loss models and their parameters, etc.
Finally, OpenAFC fully conforms to the implementation details specified in [WFA SUT Test Plan v1.5](https://www.wi-fi.org/file/afc-specification-and-test-plans).

OpenAFC software deployment consists of multiple containers, and it can be deployed on a standalone system for test and development purposes via the provided docker-compose based solution. Instructions on how to build the containers and a sample docker-compose.yaml can be found in the [OpenAFC Engine Server usage in Docker Environment](#afc-engine-build-in-docker).

OpenAFC software can also be deployed for production using the Kubernetes framework. Please refer to the readme-kubernetes.md for the instructions.

The sample docker-compose.yaml assumes that the required databases (e.g. terrain, landcover, winnforum databases, etc.) have been obtained and placed in an accessible folder according to the information in [database_readme.md](https://github.com/Telecominfraproject/open-afc/blob/main/database_readme.md) on Github.
Many of the components have additional README files inside folders that describe the additional configuration for each component. Default values are provided either inside the component or in the sample files that will work to stand up the system.

Note that this sample does not provide working SSL certificates for authentication to the server.

<br /><br />

# **Contributing**

All contributions are welcome to this project.

## How to contribute
- **Open a discussion** - The Discussion section of the GitHub project has recently been opened. 
    + Use the [Q&A](https://github.com/open-afc-project/openafc/discussions/categories/q-a) discussion if you have questions about how to set up or use the OpenAFC software
    + Use the [Ideas](https://github.com/open-afc-project/openafc/discussions/categories/ideas) discussion if you want to discuss possible improvements or other thoughts about how OpenAFC can improve
- **File an issue** - if you found a bug, want to request an enhancement, or want to implement something (bug fix or feature).
- **Send a pull request** - if you want to contribute code. Please be sure to file an issue first.

## Pull request best practices

We want to accept your pull requests. Please follow these steps:

### Step 1: File an issue

Before writing any code, please file an Issue ticket using this Github's repository's 'Issues' tab, stating the problem you want to solve or the feature you want to implement along with a high-level description of the resolution. This allows us to give you feedback before you spend any time writing code. There may be a known limitation that can't be addressed, or a bug that has already been fixed in a different way. The issue ticket allows us to communicate and figure out if it's worth your time to write a bunch of code for the project.

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
git checkout -b <Issue ticket number>-branch_name
```

Highly desirable to use branch name from Issue ticket title, or use meaningful branch name reflecting the actual changes

```
eg. git checkout -b 146-update-readme-md-to-reflect-issueticket-and-branch-creation-procedure
```

### Step 4: Make your changes

Review the [Readme in the tools/editing directory](tools/editing/README.md) to review the code style tools that are required. Pull requests not meeting the code style requirements will fail to build in the pull request.

### Step 5: Commit your changes

As you develop code, commit your changes into your local feature branch.
Please make sure to include the issue number you're addressing in your commit message.
This helps us out by allowing us to track which issue/feature your commit relates to.
Below command will commit your changes to the local branch.
Note to use Issue ticket number at the beginning of commit message.

```
git commit -a -m "<Issue ticket number>  desctiption of the change  ..."
```

### Step 6: Rebase

Before sending a pull request, rebase against upstream, such as:

```
git fetch origin
git rebase origin
```

This will add your changes on top of what's already in upstream, minimizing merge issues.

### Step 7: Run the tests

Run sufficient targetted tests on the change made to validate that the change works as expected. Please document and submit the test requests/results in the Issue ticket.

This includes running the regression test framework available under the 'tests > regression' directory to verify your changes have not broken other portions of the system.
Make sure that all regression tests are passing before submitting a pull request.

### Step 8: Push your branch to GitHub

Push code to your remote feature branch.
Below command will push your local branch along with the changes to OpenAFC GitHub.

```
git push -u origin <Issue ticket number>-branch_name
```

> NOTE: The push can include several commits (not only one), but these commits should be related to the same logical change/issue fix/new feature originally described in the [Step 1](#step-1-file-an-issue).

### Step 9: Send the pull request

Send the pull request from your feature branch to us.

#### Change Description

When submitting a pull request, please use the following template to submit the change description, risks and validations done after making the changes
(not a book, but an info required to understand the change/scenario/risks/test coverage)

- Issue ticket number (from [Step 1](#step-1-file-an-issue)). A brief description of issue(s) being fixed and likelihood/frequency/severity of the issue, or description of new feature if it is a new feature.
- Reproduction procedure: Details of how the issue could be reproduced / procedure to reproduce the issue.
  Description of Change: A detailed description of what the change is and assumptions / decisions made
- Risks: Low, Medium or High and reasoning for the same.
- Fix validation procedure:
  - Description of validations done after the fix.
  - Required regression tests: Describe what additional tests should be done to ensure no regressions in other functionalities.
  - Sanity test results as described in the [Step 7](#step-7-run-the-tests)

> NOTE: Keep in mind that we like to see one issue addressed per pull request, as this helps keep our git history clean and we can more easily track down issues.

<br />

# **How to Build**

# AFC Engine build in docker and compose setup

There are two options presented here to build the system in docker:

- Build each image individually and then run a docker compose script that ties them all together
- Use a docker compose script that builds all the images

In order to run the system, you will need to construct a data volume and make it available to the containers. See [database_readme.md](database_readme.md) for details on what data is required.

## Installing docker engine

Docker engine instructions specific to your OS are available on the [Docker official website](https://docs.docker.com/engine/install/)

## Building the Docker images individually

### Prerequisites:

Currently, all the prerequisites to build the containers for the system (except docker installation) are situated in this repository. All you need is to clone OpenAFC locally to your working directory and start all following commands from there.

### Building Docker image from Dockerfiles

There is a script that builds all container used by the AFC service.
This script is used by automatic test infrastructure. Please check [tests/regression](/tests/regression/) dir.

This script uses two environment variables PRIV_REPO and PUB_REPO to determine what repository to push images to. These values default to those used by the regression test infrastructure, but you should define them to refer to your repository.

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

to build the worker using local preq containers:

```
docker build . -t worker -f worker/Dockerfile
```

### Prometheus Monitoring images

If you wish to use [Prometheus](https://prometheus.io/) to montitor your system, you can build these images

```
cd prometheus && docker build . -t  Dockerfile-prometheus -t prometheus-image ; cd ../
cd prometheus && docker build . Dockerfile-cadvisor -t cadvisor-image ; cd ../
cd prometheus && docker build . Dockerfile-nginxexporter -t nginxexporter-image  ; cd ../
docker build . Dockerfile -t grafana-image
```

Once built, docker images are usable as usual docker image.


### Building OpenAFC engine outside of the OpenAFC system

If you wish to build or run the engine component outside of the entire OpenAFC system, you can build the docker images that are needed for only that component. If you are using the docker compose scripts provided, the engine is built as part of the process and you can skip this step

**NB:** "-v" option in docker maps the folder of the real machine into the insides of the docker container.

&quot;-v /tmp/work/open-afc:/wd/afc&quot; means that contents of &quot;/tmp/work/open-afc&quot; folder will be available inside of container in /wd/afc/

goto the project dir

```
cd open-afc
```

If you have not already, build the worker build image. Note that this will pull the pre-build image from the GitHub repository unless you provide the build arguments 

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

## docker-compose run

You would probably like to use docker-compose for setting up everything together - in this case feel free to use [docker-compose.yaml](/docker-compose.yaml) which we use for testing as reference. Also check [.env](/.env) file from tests/regression directory, which are used by OpenAFC CI.

Despite these files are used in our CI/CD, they are ready to be used as a starting point.

[.env](/.env) file contains variables which can be met in the [docker-compose.yaml](/docker-compose.yaml) file more then once, just to make it possible changing those values in one place. Unfortunately [docker-compose.yaml](/docker-compose.yaml) and yaml in general doesn't give such ability.

This compose file takes all the images from repository.

In order to run the system you will need to

- Set up docker-compose.yaml file suitable for your environment. We strongly recommend leave the [docker-compose.yaml](/docker-compose.yaml) unchanged and use the docker-compose override mechanism and use your own docker-compose.custom.yaml with all overrides together with [docker-compose.yaml](/docker-compose.yaml). Details about overrides in docker-compose.yaml can be found [in the corresponding article on official docker website](https://docs.docker.com/compose/how-tos/multiple-compose-files/merge/).
- Set up a .env file that defines certain global environment variables. 
- Set up a local set of secrets containing various passwords and other private information. See the [secrets readme](/tools/secrets/README.md) for details
- Data mappings as described in [Data mappings](#data-mappings)

Just run in this folder following command and it is done:

```
docker-compose up -d
```

if you are using merged docker-compose.yaml files the command for start should be something like this:

```
docker-compose -f docker-compose.yaml -f docker-compose.custom.yaml up -d
```


Keep in mind that on the first run it will pull all the needed containers and it can take some time. 

After the initial start of the server we recommend to stop it and then start again using these commands:

```
docker-compose down
docker-compose up -d
```

**NB: the postgres container requires the folder /mnt/nfs/pgsql/data to be owned by it's internal user and group _postgres_, which both have id 999.**

You can achieve it this way (mind the real location of these folders on your host system):

```
chown 999:999 /var/databases/pgdata
```
## Data mappings

OpenAFC containers needs several mappings to work properly. Assuming that you are using /var/databases on your host to store the databases, you can select either option 1 here (which is assumed in the docker compose shown below) or set mappings individually as shown in 2-6.

1. All databases in one folder - map to /mnt/nfs/rat_transfer

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

2. LiDAR Databases to /mnt/nfs/rat_transfer/proc_lidar_2019
   ```
   /var/databases/proc_lidar_2019:/mnt/nfs/rat_transfer/proc_lidar_2019
   ```
3. RAS database to /mnt/nfs/rat_transfer/RAS_Database
   ```
   /var/databases/RAS_Database:/mnt/nfs/rat_transfer/RAS_Database
   ```
4. Actual ULS Databases to /mnt/nfs/rat_transfer/ULS_Database
   ```
   /var/databases/ULS_Database:/mnt/nfs/rat_transfer/ULS_Database
   ```
5. Folder with daily ULS Parse data /mnt/nfs/rat_transfer/daily_uls_parse
   ```
   /var/databases/daily_uls_parse:/mnt/nfs/rat_transfer/daily_uls_parse
   ```
6. Folder with AFC Config data /mnt/nfs/afc_config (now can be moved to Object Storage by default)
   `      /var/afc_config:/mnt/nfs/afc_config
     `
   **NB: All or almost all files and folders should be owned by user and group 1003 (currently - fbrat)**

This can be applied via following command (mind the real location of these folders on your host system):

```
chown -R 1003:1003 /var/databases /var/afc_config
```

## Initial configuration and first user

On the first start of the PostgreSQL server there are some initial steps to do. First to create the database. Its default name now is **fbrat**. If you are using compose script described above, everything will be done automatically to prepare the database for intialization.

After that, once OpenAFC server is started, you need to create DB structure for the user database. This can be done using a _rat-manage-api_ utility.

```
rat-manage-api db-create
```

If you do it with the server which is run thru the docker-compose script described above, you can do it using this command:

```
docker-compose exec rat_server rat-manage-api db-create [--if_absent]
```

Here --if_absent means to do nothing if database already exists.

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

By default, the login uses non OIDC login method which manages user accounts locally. You can use the following command to create an administrator for your OpenAFC server.

```
rat-manage-api user create --role Super --role Admin --role AP --role Analysis admin "Enter Your Password Here"
```

Once done, you can authorize with this user and password in WebUI.
To exit the console press Ctrl+D or type the 'exit' command.

If you would like to use OIDC login method, please read [OIDC_Login.md](/OIDC_Login.md)

## **Environment variables**

| Name                                   | Default val                      | Container                                                                                                                | Notes                                                                                                                                                              |
| :------------------------------------- | :------------------------------- | :----------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **RabbitMQ settings**                  |                                  |                                                                                                                          |                                                                                                                                                                    |
| BROKER_TYPE                            | `internal`                       | rat-server,msghnd,worker                                                                                                 | whether `internal` or `external` AFC RMQ service used                                                                                                              |
| BROKER_PROT                            | `amqp`                           | rat-server,msghnd,worker                                                                                                 | what protocol used for AFC RMQ service                                                                                                                             |
| BROKER_USER                            | `celery`                         | rat-server,msghnd,worker                                                                                                 | user used for AFC RMQ service                                                                                                                                      |
| BROKER_PWD                             | `celery`                         | rat-server,msghnd,worker                                                                                                 | password used for AFC RMQ service                                                                                                                                  |
| BROKER_FQDN                            | `localhost`                      | rat-server,msghnd,worker                                                                                                 | IP/domain name of AFC RMQ service                                                                                                                                  |
| BROKER_PORT                            | `5672`                           | rat-server,msghnd,worker                                                                                                 | port of AFC RMQ service                                                                                                                                            |
| RMQ_LOG_CONSOLE_LEVEL                  | warning                          | rmq                                                                                                                      | RabbitMQ console log level (debug, info, warning, error, critical, none)                                                                                           |
| **AFC Object Storage**                 |                                  |                                                                                                                          | please read [objst README.md](/objstorage/README.md)                                                                                                               |
| AFC_OBJST_HOST                         | `0.0.0.0`                        | objst,rat-server,msghnd,worker                                                                                           | file storage service host domain/IP                                                                                                                                |
| AFC_OBJST_PORT                         | `5000`                           | objst,rat-server,msghnd,worker                                                                                           | file storage service port                                                                                                                                          |
| AFC_OBJST_SCHEME                       | 'HTTP'                           | rat-server,msghnd,worker                                                                                                 | file storage service scheme. `HTTP` or `HTTPS`                                                                                                                     |
| AFC_OBJST_MEDIA                        | `LocalFS`                        | objst                                                                                                                    | The media used for storing files by the service. The possible values are `LocalFS` - store files on docker's FS. `GoogleCloudBucket` - store files on Google Store |
| AFC_OBJST_LOCAL_DIR                    | `/storage`                       | objst                                                                                                                    | file system path to stored files in file storage container. Used only when `AFC_OBJST_MEDIA` is `LocalFS`                                                          |
| AFC_OBJST_LOG_LVL                      | `ERROR`                          | objst                                                                                                                    | logging level of the file storage. The relevant values are `DEBUG` and `ERROR`                                                                                     |
| AFC_OBJST_HIST_PORT                    | `4999`                           | objst,rat-server,msghnd,worker                                                                                           | history service port                                                                                                                                               |
| AFC_OBJST_WORKERS                      | `10`                             | objst                                                                                                                    | number of gunicorn workers running objst server                                                                                                                    |
| AFC_OBJST_HIST_WORKERS                 | `2`                              | objst                                                                                                                    | number of gunicorn workers runnining history server                                                                                                                |
| **MSGHND settings**                    |                                  |                                                                                                                          |                                                                                                                                                                    |
| AFC_MSGHND_BIND                        | `0.0.0.0`                        | msghnd                                                                                                                   | the socket to bind. a string of the form: <host>                                                                                                                   |
| AFC_MSGHND_PORT                        | `8000`                           | msghnd                                                                                                                   | the port to use in bind. a string of the form: <port>                                                                                                              |
| AFC_MSGHND_PID                         | `/run/gunicorn/openafc_app.pid`  | msghnd                                                                                                                   | a filename to use for the PID file                                                                                                                                 |
| AFC_MSGHND_WORKERS                     | `20`                             | msghnd                                                                                                                   | the number of worker processes for handling requests                                                                                                               |
| AFC_MSGHND_TIMEOUT                     | `180`                            | msghnd                                                                                                                   | workers silent for more than this many seconds are killed and restarted                                                                                            |
| AFC_MSGHND_ACCESS_LOG                  |                                  | msghnd                                                                                                                   | the Access log file to write to. Default to don't. Use `/proc/self/fd/2` for console                                                                               |
| AFC_MSGHND_ERROR_LOG                   | `/proc/self/fd/2`                | msghnd                                                                                                                   | the Error log file to write to                                                                                                                                     |
| AFC_MSGHND_LOG_LEVEL                   | `info`                           | msghnd                                                                                                                   | The granularity of Error log outputs (values are 'debug', 'info', 'warning', 'error', 'critical'                                                                   |
| FLASK_SQLALCHEMY_DATABASE_URI          |                                  | msghnd,rat-server,objst,cert_db                                                                                          | Ratdb DSN. Overrides SQLALCHEMY_DATABASE_URI of ratapi.conf                                                                                                        |
| FLASKFILE_SQLALCHEMY_DATABASE_PASSWORD |                                  | msghnd,rat-server,objst,cert_db                                                                                          | File with password for ratdb DSN. Overrides SQLALCHEMY_DATABASE_PASSWORD of ratapi.conf                                                                            |
| FLASKFILE_SECRET_KEY                   |                                  | msghnd,rat-server,objst,cert_db                                                                                          | File with Flask secret key. Overrides SECRET_KEY of ratapi.conf                                                                                                    |
| FLASKFILE_GOOGLE_APIKEY                |                                  | msghnd,rat-server,objst,cert_db                                                                                          | File with Google API key. Overrides GOOGLE_APIKEY of ratapi.conf                                                                                                   |
| FLASK_USER_EMAIL_SENDER_EMAIL          |                                  | msghnd,rat-server,objst,cert_db                                                                                          | Sender of notification emails. Overrides USER_EMAIL_SENDER_EMAIL of ratapi.conf                                                                                    |
| **worker settings**                    |                                  |                                                                                                                          | please read [afc-engine-preload README.md](/src/afc-engine-preload/README.md)                                                                                      |
| AFC_AEP_ENABLE                         | Not defined                      | worker                                                                                                                   | Enable the preload library if defined                                                                                                                              |
| AFC_AEP_FILELIST                       | `/aep/list/aep.list`             | worker                                                                                                                   | Path to file tree info file                                                                                                                                        |
| AFC_AEP_DEBUG                          | `0`                              | worker                                                                                                                   | Log level. 0 - disable, 1 - log time of read operations                                                                                                            |
| AFC_AEP_LOGFILE                        | `/aep/log/aep.log`               | worker                                                                                                                   | Where to write the log                                                                                                                                             |
| AFC_AEP_CACHE                          | `/aep/cache`                     | worker                                                                                                                   | Where to store the cache                                                                                                                                           |
| AFC_AEP_CACHE_MAX_FILE_SIZE            | `50000000`                       | worker                                                                                                                   | Cache files with size less than the value                                                                                                                          |
| AFC_AEP_CACHE_MAX_SIZE                 | `1000000000`                     | worker                                                                                                                   | Max cache size                                                                                                                                                     |
| AFC_AEP_REAL_MOUNTPOINT                | `/mnt/nfs/rat_transfer`          | worker                                                                                                                   | Redirect read access to there                                                                                                                                      |
| AFC_AEP_ENGINE_MOUNTPOINT              | value of AFC_AEP_REAL_MOUNTPOINT | worker                                                                                                                   | Redirect read access from here                                                                                                                                     |
| AFC_WORKER_CELERY_WORKERS              | `rat_1`                          | worker                                                                                                                   | Celery worker name(s) to use                                                                                                                                       |
| AFC_WORKER_CELERY_OPTS                 |                                  | worker                                                                                                                   | Additional celery worker options                                                                                                                                   |
| AFC_WORKER_CELERY_LOG                  | `INFO`                           | worker                                                                                                                   | Celery log level. `ERROR` or `INFO` or `DEBUG`                                                                                                                     |
| AFC_ENGINE_LOG_LVL                     | 'info'                           | worker                                                                                                                   | afc-engine log level                                                                                                                                               |
| AFC_MSGHND_NAME                        | msghnd                           | dispatcher                                                                                                               | Message handler service hostname                                                                                                                                   |
| AFC_MSGHND_PORT                        | 8000                             | dispatcher                                                                                                               | Message handler service HTTP port                                                                                                                                  |
| AFC_WEBUI_NAME                         | rat_server                       | dispatcher                                                                                                               | WebUI service hostname                                                                                                                                             |
| AFC_WEBUI_PORT                         | 80                               | dispatcher                                                                                                               | WebUI service HTTP Port                                                                                                                                            |
| AFC_ENFORCE_HTTPS                      | TRUE                             | dispatcher                                                                                                               | Wether to enforce forwarding of HTTP requests to HTTPS. TRUE - for enable, everything else - to disable                                                            |
| AFC_SERVER_NAME                        | "\_"                             | dispatcher                                                                                                               | Hostname of the AFC Server, for example - "openafc.tip.build". "\_" - will accept any hostname (but this is not secure)                                            |
| **RCACHE settings**                    |                                  |                                                                                                                          |                                                                                                                                                                    |
| RCACHE_ENABLED                         | TRUE                             | rcache, rat_server, msghnd, worker, uls_downloader                                                                       | TRUE if Rcache enabled, FALSE to use legacy objstroage response cache                                                                                              |
| RCACHE_POSTGRES_DSN                    | Must be set                      | rcache, rat_server, msghnd                                                                                               | Connection string to Rcache Postgres database (with default credentials)                                                                                           |
| RCACHE_POSTGRES_PASWORD_FILE           |                                  | rcache, rat_server, msghnd                                                                                               | Name of file with password for Rcache database DSN                                                                                                                 |
| RCACHE_SERVICE_URL                     | Must be set                      | rat_server, msghnd, worker, uls_downloader                                                                               | Rcache service REST API base URL                                                                                                                                   |
| RCACHE_RMQ_DSN                         | Must be set                      | rat_server, msghnd, worker                                                                                               | AMQP URL to RabbitMQ vhost that workers use to communicate computation result                                                                                      |
| RCACHE_UPDATE_ON_SEND                  | TRUE                             | TRUE if worker sends result to Rcache server, FALSE if msghnd/rat_server                                                 |
| RCACHE_CLIENT_PORT                     | 8000                             | rcache                                                                                                                   | Rcache REST API port                                                                                                                                               |
| RCACHE_AFC_REQ_URL                     |                                  | REST API Rcache precomputer uses to send invalidated AFC requests for precomputation. No precomputation if not set       |
| RCACHE_RULESETS_URL                    |                                  | REST API Rcache spatial invalidator uses to retrieve AFC Configs' rulesets. Default invalidation distance usd if not set |
| RCACHE_CONFIG_RETRIEVAL_URL            |                                  | REST API Rcache spatial invalidator uses to retrieve AFC Config by ruleset. Default invalidation distance usd if not set |

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
    image: ghcr.io/open-afc-project/rmq-image:latest
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

Database format has changed over time. If your user database uses older format, you might find errors indicating missing database fields upon bootup and login. The error message has instructions on how to migrate the database. These steps apply whether you're using OIDC or non OIDC login method. You have sereral options:

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

Create user with user create command. If org argument is not given, the organization can be derived from the username if it's given in the form of an email address e.g.:

```
rat-manage-api user create --role Admin --role AP --role Analysis --org mycompany.com "username" "mypassword'

```

## User roles

Roles are: Super, admin, AP, Admin, Analysis, Trial
"Super" is the highest level role, which allows access rights to all organizations, as opposed to "Admin", which is limited to one organization. When upgrade from older system without "Super", you will need to decide which users to be assigned role of "Super" and update their roles via the user update command.

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

Happy usage !

## ULS database update automation

ULS Database needs (preferrably daily) updates to be up-to-date with regulators requirements. See [README in uls](uls/README.md "ULS Service ReadMe") for instructions and configuration. The docker compose given above will create a container that runs the daily ULS update service.
