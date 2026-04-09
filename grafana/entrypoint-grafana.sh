#!/usr/bin/env bash
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -e

# NOTE: the Docker socket is intentionally NOT mounted into this container.
# Grafana reaches the Docker API exclusively through the read-only socket-proxy
# over TCP (GRAFANA_DOCKER_PROXY_HOST). The previous setuid-socat bridge to
# /var/run/docker.sock has been removed (it exposed the host Docker Engine).

grafana_tool.py jinja --recursive --strip_ext=.template $WORKDIR/templates $WORKDIR
grafana_tool.py create_db
grafana_tool.py reset_admin_password

cd $GF_PATHS_HOME && /run.sh