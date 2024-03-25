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
    echo "AFC_OBJST_PORT = ${AFC_OBJST_PORT}"
    echo "AFC_OBJST_WORKERS = ${AFC_OBJST_WORKERS}"
    echo "AFC_OBJST_HIST_PORT = ${AFC_OBJST_HIST_PORT}"
    echo "AFC_OBJST_HIST_WORKERS = ${AFC_OBJST_HIST_WORKERS}"
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

gunicorn --workers ${AFC_OBJST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_PORT} afcobjst:objst_app &
gunicorn --workers ${AFC_OBJST_HIST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_HIST_PORT} afcobjst:hist_app &

sleep infinity
