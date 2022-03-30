#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x
source ./tests/regression/regression.sh
  file=${1}         # Name of the Dockerfile
  image=${2}        # Name and optionally a tag in the 'name:tag' format
  d4b_tag=${3}      # base docker_for_build tag
  preinst_tag=${4}  # base preinst image tag
  d4b_name=${5}     # base docker_for_build image name
  preinst_name=${6} # base preinst image name
  docker_build_and_push_server ${file} ${image} ${d4b_tag} ${preinst_tag} ${d4b_name} ${preinst_name}
