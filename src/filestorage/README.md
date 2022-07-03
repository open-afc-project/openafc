This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Setup**](#setup)
  - [HTTP servers configuration](#http-servers-configuration)
  - [Building HTTP servers docker image](#building-http-servers-docker-image)
  - [RATAPI setup](#ratapi-setup)
  - [docker-compose example](#docker-compose-example)

- [Back to main readme](/README.md)
<br /><br />

# **Introduction**

This document describes the usage of the HTTP servers as storage for the RATAPI dynamic data.

The file storage HTTP server implements file exchange between HTTPD, celery workers, and the web client.
The history view server provides read access to the debug files.

# **Setup**
## HTTP servers configuration
The file storage and the history view HTTP servers receive their configuration from the following environment variables:
- FILESTORAGE_HOST - file storage server host
- FILESTORAGE_PORT - file storage server post
- HISTORY_HOST - history view server host
- HISTORY_PORT - history view server port
- FILESTORAGE_DIR - file system path to stored files

## Building HTTP servers docker image
Use Dockerfile for build or see docker-compose.yaml example below.

## RATAPI setup
RATAPI uses HTTP file storage for storing dynamic data when FILESTORAGE_HOST and FILESTORAGE_PORT environment variables are declared.
When those variables aren't declared, RATAPI uses local FS for file storage.

## docker-compose example
This example is intended for building 3 images: Postgres DB server image (ratdb), RATAPI server image (afc), and file storage server image (filestorage):
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
   afc:
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
      - /centos7/var/lib/fbrat/afc_config:/var/lib/fbrat/afc_config
      environment:
      - FILESTORAGE_HOST=filestorage
      - FILESTORAGE_PORT=5000
      - HISTORY_HOST=filestorage
      - HISTORY_EXTERNAL_PORT=14999
      links:
      - ratdb
      - filestorage
   filestorage:
      image: http_servers
      environment:
      - FILESTORAGE_HOST=0.0.0.0
      - FILESTORAGE_PORT=5000
      - HISTORY_HOST=0.0.0.0
      - HISTORY_PORT=4999
      - FILESTORAGE_DIR=/storage
      build:
         context: ./src/filestorage
      ports:
      - 14999:4999 # afc/HISTORY_EXTERNAL_PORT:afc/HISTORY_PORT
```

