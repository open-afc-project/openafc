#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
set -x
hostname
wd=${1}
rand=${2}
TAG=${rand}
export TAG
cd $wd/tests/regression
docker ps -a
docker-compose logs
docker-compose down
docker ps -a
