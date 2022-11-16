This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Setup**](#setup)
  - [The file storage HTTP server configuration](#the-file-storage-http-server-configuration)
  - [The history view HTTP server configuration](#the-history-view-http-server-configuration)
  - [Building HTTP servers docker image](#building-http-servers-docker-image)
  - [RATAPI setup](#ratapi-setup)
  - [docker-compose examples](#docker-compose-examples)

- [Back to main readme](/README.md)
<br /><br />

# **Introduction**

This document describes the usage of the HTTP servers as storage for the RATAPI dynamic data.

The file storage HTTP server implements file exchange between HTTPD, celery workers, and the web client.
The history view server provides read access to the debug files.

# **Setup**
## The file storage HTTP server configuration
The file storage HTTP server receive its configuration from the following environment variables:
- **FILESTORAGE_HOST** - file storage server host (default: 0.0.0.0).
- **FILESTORAGE_PORT** - file storage server post (default: 5000).
    - The file storage Dockerfile exposes port 5000 for access to the file storage server.
- **OBJSTORAGE** - The media used for storing files by the server (default: LocalFS). The possible values are
    - **LocalFS** - store files on docker's FS.
    - **GoogleCloudBucket** - store files on Google Store.
- **FILESTORAGE_DIR** - file system path to stored files in file storage docker (default: /storage). Used only when **OBJSTORAGE**=**LocalFS**

Using Google Storage bucket as file storage requires creating the bucket ([Create storage buckets](https://cloud.google.com/storage/docs/creating-buckets)),
the service account ([Service accounts](https://cloud.google.com/iam/docs/service-accounts)),
and the service account credentials JSON file ([Create access credentials](https://developers.google.com/workspace/guides/create-credentials#service-account)).
Accordingly, the file storage server requires the following two variables:

- **GOOGLE_CLOUD_BUCKET** - Bucket name as seen in column "Name" in [Storage Browser](https://console.cloud.google.com/storage/browser)
- **GOOGLE_CLOUD_CREDENTIALS_JSON** - Path to service account credentials JSON file in the docker image.
    - The directory contains the file must be mounted to the file storage docker image.

## The history view HTTP server configuration
The history view HTTP server receive its configuration from the following environment variables:
- **HISTORY_HOST** - history view server host (currently the same as FILESTORAGE_HOST)
- **HISTORY_PORT** - history view server port. This port should be mapped in "ports:" section of docker-compose.yaml

## RATAPI server configuration
RATAPI server accesses file storage according to the following environment variables:
- **FILESTORAGE_HOST** - file storage server host
- **FILESTORAGE_PORT** - file storage server post
- **HISTORY_HOST** - history view server host (currently the same as **FILESTORAGE_HOST**)
- **FILESTORAGE_SCHEME** - indicate by which scheme the file storage is accessed from RATAPI server. Currently is always **HTTP**.

## Building HTTP servers docker image
Use Dockerfile for build or see docker-compose.yaml example below.

## RATAPI setup
RATAPI uses HTTP file storage for storing dynamic data when FILESTORAGE_HOST and FILESTORAGE_PORT environment variables are declared.
When those variables aren't declared, RATAPI uses local FS for file storage.

## docker-compose examples
This example is intended for building 3 services: Postgres DB server service (ratdb), RATAPI server service (rat_server), and file storage server service (objstorage):

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
      image: ratapi
      build:
         context: .
      ports:
      - 1080:80
      - 1443:443
      volumes:
      - /usr/share/fbrat/rat_transfer:/usr/share/fbrat/rat_transfer
      - /usr/share/fbrat/rat_transfer/proc_lidar_2019:/var/lib/fbrat/proc_lidar_2019
      - /usr/share/fbrat/rat_transfer/RAS_Database:/var/lib/fbrat/RAS_Database
      - /usr/share/fbrat/rat_transfer/ULS_Database:/var/lib/fbrat/ULS_Database
      - /usr/share/fbrat/rat_transfer/ULS_Database:/usr/share/fbrat/afc-engine/ULS_Database
      environment:
      #File storage host is a name of file storage service
      - FILESTORAGE_HOST=objstorage
      #File storage port as declared in "objstorage:environment:FILESTORAGE_PORT"
      - FILESTORAGE_PORT=5000
      #History view server host same as file storage host
      - HISTORY_HOST=objstorage
      #Use HTTP (not HTTPS) to access file storage
      - FILESTORAGE_SCHEME="HTTP"
      links:
      - ratdb
      - objstorage
   objstorage:
      image: http_servers
      environment:
      #Run on local host
      - FILESTORAGE_HOST=0.0.0.0
      #File storage port is any free port
      - FILESTORAGE_PORT=5000
      #Run on local host
      - HISTORY_HOST=0.0.0.0
      #History view server port is any free port
      - HISTORY_PORT=4999
      #Use docker FS to store files
      - OBJSTORAGE=LocalFS
      #Some folder inside the image for file storing. Ignored when OBJSTORAGE=GoogleCloudBucket
      - FILESTORAGE_DIR=/storage
      # Google Cloud credentials file. Ignored when OBJSTORAGE="GoogleCloudBucket"
      - GOOGLE_CLOUD_CREDENTIALS_JSON=/credentials/google.cert.json
      # Google Cloud bucket name. Ignored when OBJSTORAGE="GoogleCloudBucket"
      - GOOGLE_CLOUD_BUCKET=wcc-afc-objstorage
      build:
         context: ./src/objstorage
```

The same environment variables are needed for running ready docker images downloaded from AWS.

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
      image: <rat_server image name for download>
      ports:
      - 1080:80
      - 1443:443
      volumes:
      - /usr/share/fbrat/rat_transfer:/usr/share/fbrat/rat_transfer
      - /usr/share/fbrat/rat_transfer/proc_lidar_2019:/var/lib/fbrat/proc_lidar_2019
      - /usr/share/fbrat/rat_transfer/RAS_Database:/var/lib/fbrat/RAS_Database
      - /usr/share/fbrat/rat_transfer/ULS_Database:/var/lib/fbrat/ULS_Database
      - /usr/share/fbrat/rat_transfer/ULS_Database:/usr/share/fbrat/afc-engine/ULS_Database
      environment:
      #File storage host is a name of file storage service
      - FILESTORAGE_HOST=objstorage
      #File storage port as declared in "objstorage:environment:FILESTORAGE_PORT"
      - FILESTORAGE_PORT=5000
      #History view server host same as file storage host
      - HISTORY_HOST=objstorage
      #Use HTTP (not HTTPS) to access file storage
      - FILESTORAGE_SCHEME="HTTP"
      links:
      - ratdb
      - objstorage
   objstorage:
      image: <objstorage image name for download>
      environment:
      #Run on local host
      - FILESTORAGE_HOST=0.0.0.0
      #File storage port is any free port
      - FILESTORAGE_PORT=5000
      #Run on local host
      - HISTORY_HOST=0.0.0.0
      #History view server port is any free port
      - HISTORY_PORT=4999
      #Use Google Storage to store files
      - OBJSTORAGE=LocalFS
      #Some folder inside the image for file storing. Ignored when OBJSTORAGE=GoogleCloudBucket
      - FILESTORAGE_DIR=/storage
```
