#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

AFC_OBJST_WORKERS=${AFC_OBJST_WORKERS:=10}
AFC_OBJST_HIST_WORKERS=${AFC_OBJST_HIST_WORKERS:=2}

gunicorn --workers ${AFC_OBJST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_PORT} afcobjst:objst_app &
gunicorn --workers ${AFC_OBJST_HIST_WORKERS} --worker-class gevent --bind 0.0.0.0:${AFC_OBJST_HIST_PORT} afcobjst:hist_app &

sleep infinity
