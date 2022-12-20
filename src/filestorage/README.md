This work is licensed under the OpenAFC Project License, a copy of which is included with this software program.

<br />
<br />

## Table of Contents
- [**Introduction**](#introduction)
- [**Setup**](#setup)
  - [The object storage HTTP server configuration](#the-object-storage-http-server-configuration)
  - [The history view HTTP server configuration](#the-history-view-http-server-configuration)
  - [Configuration of service which uses object storage](#configuration-of-docker-service-which-uses-object-storage)
  - [Building HTTP servers docker image](#building-http-servers-docker-image)
  - [docker-compose examples](#docker-compose-example)
  - [Apache configuration](#apache-configuration)

- [Back to main readme](/README.md)
<br /><br />

# **Introduction**

This document describes the usage of the HTTP servers as storage for the RATAPI dynamic data.

The object storage HTTP server implements file exchange between HTTPD, celery workers, and the web client.
The history view server provides read access to the debug files.

# **Setup**
## The object storage HTTP server configuration
The object storage HTTP server receive its configuration from the following environment variables:
- **AFC_OBJST_HOST** - file storage server host (default: 0.0.0.0).
- **AFC_OBJST_PORT** - file storage server post (default: 5000).
    - The object storage Dockerfile exposes port 5000 for access to the object storage server.
- **AFC_OBJST_MEDIA** - The media used for storing files by the server (default: LocalFS). The possible values are
    - **LocalFS** - store files on docker's FS.
    - **GoogleCloudBucket** - store files on Google Store.
- **AFC_OBJST_LOCAL_DIR** - file system path to stored files in file storage docker (default: /storage). Used only when **AFC_OBJST_MEDIA**=**LocalFS**
- **AFC_OBJST_LOG_LVL** - logging level of the file storage. The relevant values are DEBUG and ERROR.

Using Google Storage bucket as file storage requires creating the bucket ([Create storage buckets](https://cloud.google.com/storage/docs/creating-buckets)),
the service account ([Service accounts](https://cloud.google.com/iam/docs/service-accounts)),
and the service account credentials JSON file ([Create access credentials](https://developers.google.com/workspace/guides/create-credentials#service-account)).
Accordingly, the file storage server requires the following two variables:

- **AFC_OBJST_GOOGLE_CLOUD_BUCKET** - Bucket name as seen in column "Name" in [Storage Browser](https://console.cloud.google.com/storage/browser)
- **AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON** - Path to service account credentials JSON file in the docker image.
    - The directory contains the file must be mounted to the file storage docker image.

## The history view HTTP server configuration
The history view HTTP server receive its configuration from the following environment variables:
- **AFC_OBJST_HIST_HOST** - history view server host (default: the same as AFC_OBJST_HOST)
- **AFC_OBJST_HIST_PORT** - history view server port (default: 4999)
    - The object storage Dockerfile exposes port 4999 for access to the history server.

## Configuration of docker service which uses object storage
The docker service accesses file storage according to the following environment variables:
- **AFC_OBJST_HOST** - file storage server host
- **AFC_OBJST_PORT** - file storage server post
- **AFC_OBJST_HIST_HOST** - history view server host (currently the same as **AFC_OBJST_HOST**)
- **AFC_OBJST_SCHEME** - file storage server scheme, HTTP or HTTPS

**AFC_OBJST_HOST** and **AFC_OBJST_PORT** environment variables have to be declared to enable using HTTP object storage.

## Building HTTP servers docker image
Use Dockerfile for build or see docker-compose.yaml example below.

## docker-compose example

```
version: '3.2'
services:
   objst:
      image: http_servers
      environment:
      # Run on local host
      - AFC_OBJST_HOST=0.0.0.0
      # Object storage port
      - AFC_OBJST_PORT=5000
      # Run on local host
      - AFC_OBJST_HIST_HOST=0.0.0.0
      # History view server port
      - AFC_OBJST_HIST_PORT=4999
      # Use docker FS to store files
      - AFC_OBJST_MEDIA=LocalFS
      # Some folder inside the image for file storing. Ignored when AFC_OBJST_MEDIA=GoogleCloudBucket
      - AFC_OBJST_LOCAL_DIR=/storage
      # Google Cloud credentials file. Ignored when AFC_OBJST_MEDIA="GoogleCloudBucket"
      - AFC_OBJST_GOOGLE_CLOUD_CREDENTIALS_JSON=/credentials/google.cert.json
      # Google Cloud bucket name. Ignored when AFC_OBJST_MEDIA="GoogleCloudBucket"
      - AFC_OBJST_GOOGLE_CLOUD_BUCKET=wcc-afc-objstorage
      build:
         context: .

   service_which_stores_in_objst:
      image: some_name
      environment:
      # Object storage host is a name of objst service
      - AFC_OBJST_HOST=objst
      # Object storage port as declared in "objst:environment:AFC_OBJST_PORT"
      - AFC_OBJST_PORT=5000
      # History view server host same as file storage host
      - AFC_OBJST_HIST_HOST=objst
      # Use HTTP (not HTTPS) to access file storage
      - AFC_OBJST_SCHEME=HTTP
```

## Apache configuration
To enable "Debug Files" link in GUI, the Apache server must forward history HTTP(S) requests to the history server.
To do this, the Apache proxy module should be configured in the following way:
```
<VirtualHost APACHE_HOSTNAME:80>
  ProxyPassReverse /dbg http://HISTORY_HOSTNAME:4999/dbg
  ProxyPass /dbg http://HISTORY_HOSTNAME:4999/dbg
  ProxyPreserveHost On
  ProxyRequests Off
</VirtualHost>
<VirtualHost APACHE_HOSTNAME:443>
  ProxyPassReverse /dbg http://HISTORY_HOSTNAME:4999/dbgs
  ProxyPass /dbg http://HISTORY_HOSTNAME:4999/dbgs
  ProxyPreserveHost On
  ProxyRequests Off
</VirtualHost>
```
where APACHE_HOSTNAME is the name of the Apache host and HISTORY_HOSTNAME is the name of the history host.

That is, HTTP accesses to "/dbg" and HTTPS accesses to "/dbgs" are forwarded to the history server.
