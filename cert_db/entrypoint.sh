#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

/wd/sweep.sh
if [[ -z "${K8S_CRON}" ]]; then
  crond -b
  sleep infinity
fi
