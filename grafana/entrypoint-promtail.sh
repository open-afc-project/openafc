#!/usr/bin/env bash
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -e
set -x

jinja --env-regex GRAFANA_.* -o ${PROMTAIL_CONFIG} ${PROMTAIL_CONFIG}

promtail -config.file=${PROMTAIL_CONFIG}
