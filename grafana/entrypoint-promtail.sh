#!/usr/bin/env bash
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -e
set -x

promtail -config.file=${PROMTAIL_CONFIG} -config.expand-env
