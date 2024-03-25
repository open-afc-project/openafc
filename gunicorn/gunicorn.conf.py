#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
bind = '0.0.0.0:8000'
workers = 20
deamon = True
pidfile = '/run/gunicorn/openafc_app.pid'
timeout = 120
