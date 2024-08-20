#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Running debug profile" 
    AFC_SERVER_LOG_LEVEL="debug"
    echo "AFC_SERVER_PORT = ${AFC_SERVER_PORT}"
    echo "RCACHE_POSTGRES_PASSWORD_FILE = ${RCACHE_POSTGRES_PASSWORD_FILE}"
    echo "AFC_SERVER_RATDB_DSN = ${AFC_SERVER_RATDB_DSN}"
    echo "AFC_SERVER_RATDB_PASSWORD_FILE = ${AFC_SERVER_RATDB_PASSWORD_FILE}"
    echo "RCACHE_RMQ_DSN = ${RCACHE_RMQ_DSN}"
    echo "RCACHE_RMQ_PASSWORD_FILE = ${RCACHE_RMQ_PASSWORD_FILE}"
    echo "AFC_SERVER_REQUEST_TIMEOUT = ${AFC_SERVER_REQUEST_TIMEOUT}"
    echo "AFC_SERVER_REQUEST_TIMEOUT_EDEBUG = ${AFC_SERVER_REQUEST_TIMEOUT_EDEBUG}"
    echo "AFC_SERVER_CONFIG_REFRESH = ${AFC_SERVER_CONFIG_REFRESH}"
    echo "AFC_SERVER_GUNICORN_PID = ${AFC_SERVER_GUNICORN_PID}"
    echo "AFC_SERVER_GUNICORN_WORKERS = ${AFC_SERVER_GUNICORN_WORKERS}"
    echo "AFC_SERVER_ACCESS_LOG = ${AFC_SERVER_ACCESS_LOG}"
    echo "AFC_SERVER_ERROR_LOG = ${AFC_SERVER_ERROR_LOG}"
    echo "NFS_MOUNT_PATH = ${NFS_MOUNT_PATH}"
    echo "ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS = ${ALS_KAFKA_CLIENT_BOOTSTRAP_SERVERS}"
    echo "ALS_KAFKA_MAX_REQUEST_SIZE = ${ALS_KAFKA_MAX_REQUEST_SIZE}"
    echo "BROKER_PROT = ${BROKER_PROT}"
    echo "BROKER_USER = ${BROKER_USER}"
    echo "BROKER_PWD = ${BROKER_PWD}"
    echo "BROKER_FQDN = ${BROKER_FQDN}"
    echo "BROKER_PORT = ${BROKER_PORT}"
    echo "BROKER_VHOST = ${BROKER_VHOST}"
    echo "AFC_OBJST_HOST = ${AFC_OBJST_HOST}"
    echo "AFC_OBJST_PORT = ${AFC_OBJST_PORT}"
    echo "AFC_OBJST_SCHEME = ${AFC_OBJST_SCHEME}"
    ;;
  "production")
    echo "Running production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

exec gunicorn \
    --worker-class uvicorn.workers.UvicornWorker \
    --bind 0.0.0.0:${AFC_SERVER_PORT} \
    --pid "${AFC_SERVER_GUNICORN_PID}" \
    --workers ${AFC_SERVER_GUNICORN_WORKERS:-`python3 -c "import multiprocessing; print(multiprocessing.cpu_count() + 1)"`} \
    ${AFC_SERVER_ACCESS_LOG:+--access-logfile "$AFC_SERVER_ACCESS_LOG"} \
    --error-logfile "${AFC_SERVER_ERROR_LOG}" \
    --log-level ${AFC_SERVER_LOG_LEVEL} \
    afc_server_app:app
#
sleep infinity

exit $?
