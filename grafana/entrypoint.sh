#!/bin/bash
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -e
set -x

grafana_tool.py jinja --recursive $WORKDIR/templates $WORKDIR
grafana_tool.py create_db
grafana_tool.py reset_admin_password

cd $GF_PATHS_HOME && /run.sh