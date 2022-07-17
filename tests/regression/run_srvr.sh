#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x
SRV_DI="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server"     # server image

# FUNCS
msg() {
   echo -e "\e[34m \e[1m$1\e[0m"
}
err() {
  echo -e "\e[31m$1\e[0m"
}
ok() {
  echo -e "\e[32m$1\e[0m"
}
check_ret() {
  ret=${1}  # only 0 is OK

  if [ ${ret} -eq 0 ]; then
    ok "OK"
    else err "FAIL"; exit ${ret}
  fi
}

hostname
wd=${1}
tag=${2}

# build regr test docker
cd $wd/tests
docker build -t afc-regression-test .
check_ret $?
# export test dut configuration
cd $wd/tests/regression
mkdir pipe
mkdir afc_config
docker run --rm -v `pwd`/pipe:/pipe afc-regression-test --cmd exp_adm_cfg --outfile /pipe/export_admin_cfg.json
check_ret $?
# copy regr server config and run srvr
cp -a ~/template_regrtest/*  $wd/tests/regression
docker tag ${SRV_DI}:${tag} srvr_di
check_ret $?
docker-compose down && docker-compose up -d && docker ps -a
check_ret $?
sleep 5
# set default srvr configuration
docker-compose exec -T rat_server rat-manage-api db-create
docker-compose exec -T -u 1003:1003 rat_server rat-manage-api cfg add src=/pipe/export_admin_cfg.json
