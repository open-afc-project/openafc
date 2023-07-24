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
    AFC_MSGHND_LOG_LEVEL="debug"
    echo "AFC_MSGHND_BIND = ${AFC_MSGHND_BIND}"
    echo "AFC_MSGHND_PID = ${AFC_MSGHND_PID}"
    echo "AFC_MSGHND_ACCESS_LOG = ${AFC_MSGHND_ACCESS_LOG}"
    echo "AFC_MSGHND_ERROR_LOG = ${AFC_MSGHND_ERROR_LOG}"
    echo "AFC_MSGHND_TIMEOUT = ${AFC_MSGHND_TIMEOUT}"
    echo "AFC_MSGHND_WORKERS = ${AFC_MSGHND_WORKERS}"
    echo "AFC_MSGHND_LOG_LEVEL = ${AFC_MSGHND_LOG_LEVEL}"
    echo "AFC_MSGHND_RATAFC_TOUT = ${AFC_MSGHND_RATAFC_TOUT}"
    ;;
  "production")
    echo "Running production profile"
    AFC_MSGHND_LOG_LEVEL="info"
    ;;
  *)
    echo "Uknown profile"
    AFC_MSGHND_LOG_LEVEL="info"
    ;;
esac

gunicorn \
--bind "${AFC_MSGHND_BIND}" \
--pid "${AFC_MSGHND_PID}" \
--workers "${AFC_MSGHND_WORKERS}" \
--timeout "${AFC_MSGHND_TIMEOUT}" \
--access-logfile "${AFC_MSGHND_ACCESS_LOG}" \
--error-logfile "${AFC_MSGHND_ERROR_LOG}" \
--log-level "${AFC_MSGHND_LOG_LEVEL}" \
--worker-class gevent \
wsgi:app

#
sleep infinity

exit $?
