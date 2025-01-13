#!/usr/bin/env bash
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -e
set -x

# In compose environment landing docker unix socket to TCP port (socket must be
# mapped inside container for this)
if [[ -e /var/run/docker.sock ]] && \
        [[ "${GRAFANA_DOCKER_SOCKET_PORT}" != "" ]]; then
    socat UNIX-CONNECT:/var/run/docker.sock TCP-LISTEN:${GRAFANA_DOCKER_SOCKET_PORT},fork,reuseaddr &
fi

grafana_tool.py jinja --recursive --strip_ext=.template $WORKDIR/templates $WORKDIR
grafana_tool.py create_db
grafana_tool.py reset_admin_password

env

cd $GF_PATHS_HOME && /run.sh