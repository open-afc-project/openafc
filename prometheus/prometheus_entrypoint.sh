#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Runs prometheus with parameter specified over environment

if [ "$PROMETHEUS_SCRAPE_INTERVAL" != "" ]
then
  sed -i "s/scrape_interval:\\s*[0-9]\\+./scrape_interval: $PROMETHEUS_SCRAPE_INTERVAL/g" prometheus.yml
fi

if [ "$PROMETHEUS_SCRAPE_TIMEOUT" != "" ]
then
  sed -i "s/scrape_timeout:\\s*[0-9]\\+./scrape_timeout: $PROMETHEUS_SCRAPE_TIMEOUT/g" prometheus.yml
fi

prometheus --web.enable-lifecycle --storage.tsdb.path="$PROMETHEUS_DATA"
