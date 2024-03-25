#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -x
wd=${1}       # full path to the afc project dir
tag=${2}      # tag to be used for new docker images
push=${3:-1}  # whether push new docker images into repo [0/1]
source $wd/tests/regression/regression.sh

build_dev_server $1 $2 $3

# Local Variables:
# vim: sw=2:et:tw=80:cc=+1