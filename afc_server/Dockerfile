#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-alpine3.21@sha256:d4f9227f21409479c7fe92288f2e40b1a56ae591c8ad3b5446dfeaef90f67857

# Port REST API service listen on. Mandatory
# ENV AFC_SERVER_PORT

# Connection string to Rcache DB. Mandatory
# ENV RCACHE_POSTGRES_DSN

# File with password for Rcache DB DSN
# ENV RCACHE_POSTGRES_PASSWORD_FILE

# Connection string to RatDB. Mandatory
# ENV AFC_SERVER_RATDB_DSN

# File with password for RatDB DSN
# ENV AFC_SERVER_RATDB_PASSWORD_FILE

# AMQP DSN for receiving replies from AFC Engine worker. Mandatory
# ENV RCACHE_RMQ_DSN

# File with password for AMQP DSN
# ENV RCACHE_RMQ_PASSWORD_FILE

# Log level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
ENV AFC_SERVER_LOG_LEVEL=INFO

# AFC Request timeout in seconds
ENV AFC_SERVER_REQUEST_TIMEOUT=180

# EDEBUG AFC Request timeout in seconds
ENV AFC_SERVER_REQUEST_TIMEOUT_EDEBUG=3600

# AFC Config refresh interval in seconds
ENV AFC_SERVER_CONFIG_REFRESH=5

# PID file for Gunicorn
ENV AFC_SERVER_GUNICORN_PID=/run/gunicorn/afc_server_app.pid

# Number of worker processes. Defaukt is number of CPUs plus one
# ENV AFC_SERVER_GUNICORN_WORKERS

# Access log file (optional, no access log if unspecified)
# ENV AFC_SERVER_ACCESS_LOG

# Error log file
ENV AFC_SERVER_ERROR_LOG=/proc/self/fd/2

# Comma separated list of vendor extensions from previously computed responses
# to be appended to AFC requests
# ENV AFC_STATE_VENDOR_EXTENSIONS

# Root of static data files in Worker's file system
# ENV NFS_MOUNT_PATH

# ALS client parameters (see comments in als.py)
# ENV ALS_KAFKA_SERVER_ID
# ENV ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS
# ENV ALS_KAFKA_MAX_REQUEST_SIZE
# ENV AFC_ALS_TOPIC_NAME
# ENV AFC_JSON_TOPIC_PREFIX


# Parts of RMQ Celery URL (see BrokerConfigurator constructor)
# ENV BROKER_PROT
# ENV BROKER_USER
# ENV BROKER_PWD
# ENV BROKER_FQDN
# ENV BROKER_PORT
# ENV BROKER_VHOST

# Filestorage parameters
# ENV AFC_OBJST_HOST
# ENV AFC_OBJST_PORT
# ENV AFC_OBJST_SCHEME

WORKDIR /wd

# Prometheus stuff
COPY gunicorn/config.py /wd/gunicorn_config.py
# Directory for Prometheus's multiprocess housekeeping
ENV PROMETHEUS_MULTIPROC_DIR=/wd/prometheus_multiproc_dir
RUN mkdir -p $PROMETHEUS_MULTIPROC_DIR

RUN apk add --update --no-cache curl

# Private PyPI mirror credential is passed via BuildKit secret
# (--secret id=pip_config,src=<pip.conf>) so it is never baked into image
# layers or build-arg history. Omit --secret to use public PyPI.

COPY src/afc-packages /wd/afc-packages
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    pip3 install --use-pep517 --root-user-action=ignore \
    --no-cache-dir -r /wd/afc-packages/pkgs.afc_server \
    && rm -rf /wd/afc-packages

# Installing Pthon libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev
COPY afc_server/requirements.txt /wd
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    apk add --update --no-cache python3-dev build-base && \
    pip3 install --no-cache-dir --root-user-action=ignore --require-hashes -r /wd/requirements.txt && \
    apk del --purge python3-dev build-base && rm -f /wd/requirements.txt

COPY afc_server/afc_server_app.py afc_server/afc_server_compute.py \
    afc_server/afc_server_db.py afc_server/afc_server_models.py \
    afc_server/afc_server_msg_proc.py afc_server/entrypoint.sh /wd/
RUN chmod a+x /wd/afc_server_app.py /wd/entrypoint.sh

RUN mkdir -p `dirname $AFC_SERVER_GUNICORN_PID`

# Run as non-root fbrat (uid/gid 1003).
# afc_server has no host bind mounts, so the non-root switch is safe.
RUN addgroup -g 1003 fbrat && \
    adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat && \
    chown -R fbrat:fbrat /wd `dirname $AFC_SERVER_GUNICORN_PID`
USER fbrat

CMD ["/wd/entrypoint.sh"]

HEALTHCHECK --start-period=20s \
    CMD curl -f http://localhost:${AFC_SERVER_PORT}/healthy || exit 1
