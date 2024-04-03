#!/bin/bash
#
# Copyright  2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -x
docker_login() {
  docker login ghcr.io
}

docker_login