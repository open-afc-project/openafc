#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM alpine:3.18

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

# Root of static data files in Worker's file system
# ENV NFS_MOUNT_PATH

# ALS client parameters (see arg_dscs in als.py)
# ENV ALS_KAFKA_SERVER_ID
# ENV ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS
# ENV ALS_KAFKA_MAX_REQUEST_SIZE

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

RUN apk add --update --no-cache python3=~3.11 py3-sqlalchemy=~1.4 py3-pip \
    py3-pydantic=~1.10 py3-psycopg2 py3-greenlet curl

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.afc_server \
    && rm -rf /wd/afc-packages

# Installing Confluent Kafka Python client. Eventually, hopefully, it will be
# installed as Alpine package (thus not requiring gcc and musl-dev install)
RUN apk add --update --no-cache python3-dev=~3.11 gcc musl-dev \
    librdkafka-dev=~2.1.1 && \
    pip3 install confluent-kafka==2.1.1 && \
    apk del python3-dev gcc musl-dev

COPY afc_server/requirements.txt /wd
RUN pip3 install --root-user-action=ignore -r /wd/requirements.txt && \
    rm -f /wd/requirements.txt

COPY afc_server/afc_server_app.py afc_server/afc_server_compute.py \
    afc_server/afc_server_db.py afc_server/afc_server_models.py \
    afc_server/afc_server_msg_proc.py afc_server/entrypoint.sh /wd/
RUN chmod a+x /wd/afc_server_app.py /wd/entrypoint.sh

RUN mkdir -p `dirname $AFC_SERVER_GUNICORN_PID`

CMD ["/wd/entrypoint.sh"]

HEALTHCHECK --start-period=20s \
    CMD curl -f http://localhost:${AFC_SERVER_PORT}/healthy || exit 1
