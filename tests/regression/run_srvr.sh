#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x
hostname
wd=${1}
rand=${2}

# build regr test docker
cd $wd/test
docker build -t afc-regression-test .

# export test dut configuration
docker run --rm -v `pwd`/pipe:/pipe afc-regression-test --db e=/pipe/export_admin_cfg.json

# copy regr server config and run srvr
cp -a ~/template_regrtest/*  $wd/test/regression
cd $wd/test/regression
docker tag dr942120/srv_di:${rand} srvr_di
docker-compose down && docker-compose up -d && docker ps -a

# set default srvr configuration
docker-compose exec rat_server rat-manage-api db-create
docker-compose exec rat_server rat-manage-api cfg add src=/pipe/export_admin_cfg.json
