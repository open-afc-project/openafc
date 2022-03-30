#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x
echo `pwd`
source ./tests/regression/regression.sh
  file=${1}    # Name of the Dockerfile
  image=${2}   # Name and optionally a tag in the 'name:tag' format
#  args=${3:-" "}    # extra args for docker build command
  docker_build_and_push ${file} ${image} "$3"
