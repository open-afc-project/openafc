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
  - [Prereqs](#prereqs)
  - [docker-compose](#docker-compose)
  - [PostgreSQL structure](#postgresql-structure)


- [Database info](/database_readme.md)
- [Release Notes](/ReleaseNote.md)
<br /><br />

# **Introduction**

This document describes the procedure for submitting the source code changes to the TIP's openAFC github project. Procedure described in this document requires access to TIP's openAFC project and knowledge of the GIT usage. Please contact support@telecominfraproject.com in case you need access to the openAFC project.
Github.com can be referred for [details of alternate procedures for creating the pull requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests), developers can use any of these methods but need to include change description as part of pull requests description.


<br /><br />

# **Contributing**

All contributions are welcome to this project.

## How to contribute

* **File an issue** - if you found a bug, want to request an enhancement, or want to implement something (bug fix or feature).
* **Send a pull request** - if you want to contribute code. Please be sure to file an issue first.

## Pull request best practices

We want to accept your pull requests. Please follow these steps:

### Step 1: File an issue

Before writing any code, please file an issue stating the problem you want to solve or the feature you want to implement. This allows us to give you feedback before you spend any time writing code. There may be a known limitation that can't be addressed, or a bug that has already been fixed in a different way. The issue allows us to communicate and figure out if it's worth your time to write a bunch of code for the project.

### Step 2: Clone OpenAFC GitHub repository

OpenAFC source repository can be cloned using the below command.
```
git clone git@github.com:Telecominfraproject/open-afc.git
```
This will create your own copy of our repository.
[about remote repositories](https://docs.github.com/en/get-started/getting-started-with-git/about-remote-repositories)

### Step 3: Create a temporary branch

Create a temporary branch for making your changes.
Keep a separate branch for each issue/feature you want to address.
```
git checkout -b my_changes
```

### Step 4: Commit your changes
As you develop code, commit your changes into your local feature branch.
Please make sure to include the issue number you're addressing in your commit message.
This helps us out by allowing us to track which issue/feature your commit relates to.
Below command will commit your changes to the local branch.
```
git commit -a -m "Issue:123  desctiption of the change  ..."
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
git push -u  origin my_change
```
 > NOTE: The push can include several commits (not only one), but these commits should be related to the same logical change/issue fix/new feature originally described in the [Step 1](#step-1-file-an-issue).

### Step 8: Send the pull request

Send the pull request from your feature branch to us.

#### Change Description


When submitting a pull request, please use the following template to submit the change description, risks and validations done after making the changes
(not a book, but an info required to understand the change/scenario/risks/test coverage)

- Issue Description: Issue # (from [Step 1](#step-1-file-an-issue)). A brief description of issue(s) being fixed and likelihood/frequency/severity of the issue,   or description of new feature if it is a new feature.
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

Building the docker container with Monolitic OpenAFC is straitforward - in the root folder of the OpenAFC Project run default docker build command:

```
docker build .
```
If you want to build image with some special tag just add the -t \<tag\> option

```
docker build . -t openafc
```

Once built, docker image is usable as usual docker image.

## Prereqs
Significant to know that the container needs several mappings to work properly:

1) All databases in one folder - map to /usr/share/fbrat/rat_transfer
      ```
      /var/databases:/usr/share/fbrat/rat_transfer
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


2) LiDAR Databases to /var/lib/fbrat/proc_lidar_2019
      ```
      /var/databases/proc_lidar_2019:/var/lib/fbrat/proc_lidar_2019
      ```
3) RAS database to /var/lib/fbrat/RAS_Database
      ```
      /var/databases/RAS_Database:/var/lib/fbrat/RAS_Database
      ```
4) Actual ULS Databases to /var/lib/fbrat/ULS_Database
      ```
      /var/databases/ULS_Database:/var/lib/fbrat/ULS_Database
      ```
5) Same Actual ULS Databases to /usr/share/fbrat/afc-engine/ULS_Database (temporary requirement, to be removed in next versions)
      ```
      /var/databases/ULS_Database:/usr/share/fbrat/afc-engine/ULS_Database
      ```
6) Folder with daily ULS Parse data /var/lib/fbrat/daily_uls_parse
      ```
      /var/databases/daily_uls_parse:/var/lib/fbrat/daily_uls_parse
      ```
7) Folder with AFC Config data /var/lib/fbrat/afc_config
      ```
      /var/afc_config:/var/lib/fbrat/afc_config
      ```
**Please post comment or open issue with request to get access to the openAFC database initial archive.**

It also needs access to the PostgreSQL 9 Database server with _fbrat_ database with special structure. (see separate )

Default access to the database is described in the puppet files. However you can change it by overriding the default config (located in _/etc/xdg/fbrat/ratapi.conf_ or changing corresponding puppet variables).

## docker-compose

You would probably like to use docker-compose for setting up everything together - in this case feel free to use following docker-compose.yaml file as reference:

```
version: '3.2'

services:
  ratdb:
    image: postgres:9
    restart: always
    volumes:
      - /var/databases/pgdata:/var/lib/pgsql/data
    environment:
      POSTGRES_PASSWORD: N3SF0LVKJx1RAhFGx4fcw
      PGDATA: /var/lib/pgsql/data
      POSTGRES_DB: fbrat
  rat_server:
    build:
      context: .
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - /var/databases:/usr/share/fbrat/rat_transfer
      - /var/databases/proc_lidar_2019:/var/lib/fbrat/proc_lidar_2019
      - /var/databases/RAS_Database:/var/lib/fbrat/RAS_Database
      - /var/databases/ULS_Database:/var/lib/fbrat/ULS_Database
      - /var/databases/ULS_Database:/usr/share/fbrat/afc-engine/ULS_Database
      - /var/databases/daily_uls_parse:/var/lib/fbrat/daily_uls_parse
      - /var/afc_config:/var/lib/fbrat/afc_config
    links:
      - ratdb
```
Just create this file on the same level with Dockerfile (don't forget to update paths to resources accordingly) and you are almost ready.
Just run in this folder following command and it is done:
```
docker-compose up -d
```

Keep in mind that on the first run it will build and pull all the needed containers and it can take some time (based on your machine power)

## PostgreSQL structure

If you start the server for the first time you need to create special DB structure

```
# \dt
              List of relations
 Schema |       Name       | Type  |  Owner
--------+------------------+-------+----------
 public | aaa_role         | table | postgres
 public | aaa_user         | table | postgres
 public | aaa_user_role    | table | postgres
 public | access_point     | table | postgres
 public | alembic_version  | table | postgres
 public | blacklist_tokens | table | postgres
 public | limits           | table | postgres
(7 rows)
```

aaa_role:
```
 Column |         Type          |                       Modifiers
--------+-----------------------+-------------------------------------------------------
 id     | integer               | not null default nextval('aaa_role_id_seq'::regclass)
 name   | character varying(50) |
Indexes:
    "aaa_role_pkey" PRIMARY KEY, btree (id)
    "aaa_role_name_key" UNIQUE CONSTRAINT, btree (name)
Referenced by:
    TABLE "aaa_user_role" CONSTRAINT "aaa_user_role_role_id_fkey" FOREIGN KEY (role_id) REFERENCES aaa_role(id) ON DELETE CASCADE
```
aaa_user:
```
       Column       |            Type             |                       Modifiers
--------------------+-----------------------------+-------------------------------------------------------
 id                 | integer                     | not null default nextval('aaa_user_id_seq'::regclass)
 email              | character varying(255)      | not null
 email_confirmed_at | timestamp without time zone |
 password           | character varying(255)      | not null
 active             | boolean                     |
 first_name         | character varying(50)       |
 last_name          | character varying(50)       |
Indexes:
    "aaa_user_pkey" PRIMARY KEY, btree (id)
    "aaa_user_email_key" UNIQUE CONSTRAINT, btree (email)
Referenced by:
    TABLE "aaa_user_role" CONSTRAINT "aaa_user_role_user_id_fkey" FOREIGN KEY (user_id) REFERENCES aaa_user(id) ON DELETE CASCADE
    TABLE "access_point" CONSTRAINT "access_point_user_id_fkey" FOREIGN KEY (user_id) REFERENCES aaa_user(id) ON DELETE CASCADE
```
aaa_user_role:
```
 Column  |  Type   |                         Modifiers
---------+---------+------------------------------------------------------------
 id      | integer | not null default nextval('aaa_user_role_id_seq'::regclass)
 user_id | integer |
 role_id | integer |
Indexes:
    "aaa_user_role_pkey" PRIMARY KEY, btree (id)
    "aaa_user_role_user_id_role_id_key" UNIQUE CONSTRAINT, btree (user_id, role_id)
Foreign-key constraints:
    "aaa_user_role_role_id_fkey" FOREIGN KEY (role_id) REFERENCES aaa_role(id) ON DELETE CASCADE
    "aaa_user_role_user_id_fkey" FOREIGN KEY (user_id) REFERENCES aaa_user(id) ON DELETE CASCADE
```
access_point:
```
      Column      |         Type          |                         Modifiers
------------------+-----------------------+-----------------------------------------------------------
 id               | integer               | not null default nextval('access_point_id_seq'::regclass)
 serial_number    | character varying(64) | not null
 model            | character varying(64) |
 manufacturer     | character varying(64) |
 certification_id | character varying(64) |
 user_id          | integer               |
Indexes:
    "access_point_pkey" PRIMARY KEY, btree (id)
    "access_point_serial_number_key" UNIQUE CONSTRAINT, btree (serial_number)
    "ix_access_point_serial_number" btree (serial_number)
Foreign-key constraints:
    "access_point_user_id_fkey" FOREIGN KEY (user_id) REFERENCES aaa_user(id) ON DELETE CASCADE
```
alembic_version:
```
   Column    |         Type          | Modifiers
-------------+-----------------------+-----------
 version_num | character varying(32) | not null
```
blacklist_tokens:
```
     Column     |            Type             |                           Modifiers
----------------+-----------------------------+---------------------------------------------------------------
 id             | integer                     | not null default nextval('blacklist_tokens_id_seq'::regclass)
 token          | character varying(500)      | not null
 blacklisted_on | timestamp without time zone | not null
Indexes:
    "blacklist_tokens_pkey" PRIMARY KEY, btree (id)
    "blacklist_tokens_token_key" UNIQUE CONSTRAINT, btree (token)
```
limits:
```  Column  |     Type      | Modifiers
----------+---------------+-----------
 id       | integer       | not null
 min_eirp | numeric(50,0) |
 enforce  | boolean       |
Indexes:
    "limits_pkey" PRIMARY KEY, btree (id)
```

Also you need to perform following steps on newly created PostgreSQL database for stable work of OpenAFC server

```
\dt
INSERT into aaa_role (id, name) VALUES ('2','Analysis');
INSERT into aaa_role (id, name) VALUES ('3','AP');
Check if previous added
select * from aaa_role;
CREATE TABLE limits (id Integer PRIMARY KEY, min_eirp Numeric(50), enforce Boolean);
\qt
logout
```

Automatic default skeleton DB structure generation to be added in future.
