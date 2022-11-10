#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x

wd=${1}    # full path to the afc project path
TAG=${2}   # docker images tag
export TAG

source $wd/tests/regression/regression.sh

hostname
# export test dut configuration
cd $wd/tests/regression_${TAG}
mkdir pipe
mkdir afc_config
docker run --rm -v `pwd`/pipe:/pipe ${RTEST_DI}:${TAG} --cmd exp_adm_cfg --outfile /pipe/export_admin_cfg.json
check_ret $?
# copy regr server config and run srvr
cp -a ~/template_regrtest/*  .
mv docker-compose_mtls.yaml docker-compose.yaml
check_ret $?
docker-compose down && docker-compose up -d && docker ps -a
check_ret $?
sleep 5
# set default srvr configuration
docker-compose exec -T rat_server rat-manage-api db-create
docker-compose exec -T -u 1003:1003 rat_server rat-manage-api cfg add src=/pipe/export_admin_cfg.json
