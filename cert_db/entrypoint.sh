#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

echo Delaying for $SWEEP_DELAY
sleep $SWEEP_DELAY

while true
do
  /wd/sweep.sh
  echo Sleping for $SWEEP_PERIOD
  sleep $SWEEP_PERIOD
done
