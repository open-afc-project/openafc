#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
set -x
# ./run_srvr.sh /work/open-afc MY_TAG 0
# 1. starts AFC server server
# 2. export configuration form test database ( users, APs, afc-config)
# 3. integrates the settings from #2 into the AFC server to prepare it to autiomation tests

wd=${1}         # full path to the afc project path
TAG=${2}        # docker images tag
randdir=${3:-0} # whether to use a dir with random name that includes tag name.
                # like $wd/tests/regression_${TAG} If so, it should be already existing
                # Usually used by regression test infra. not requred for user's unit testing
export TAG

source $wd/tests/regression/regression.sh

hostname
# export test dut configuration
cd $wd && docker build . -t ${RTEST_DI}:${TAG} -f tests/Dockerfile

# whether to use a dir with random name
tests_dir=$wd/tests/regression
if [ ${randdir} -eq 1 ]; then
  tests_dir=$wd/tests/regression_${TAG}
fi
# go to test dir
cd $tests_dir

# create pipe and afc_config dir if does not exist
[ ! -d pipe ] && mkdir pipe

# export configuration from regression test container
docker run --rm -v `pwd`/pipe:/pipe ${RTEST_DI}:${TAG} --cmd exp_adm_cfg --outfile /pipe/export_admin_cfg.json
check_ret $?
# copy regr server tls/mtls config (if existing)
[ -d ~/template_regrtest/rat_server-conf ] && cp -ar ~/template_regrtest/rat_server-conf .
[ -d ~/template_regrtest/ssl ] && cp -a ~/template_regrtest/ssl .

# run srvr
docker-compose down -v && docker-compose -f docker-compose.yaml -f docker-compose.test_override.yaml up -d && docker ps -a
check_ret $?
sleep 5

# set default srvr configuration
docker-compose exec -T rat_server rat-manage-api db-create
docker-compose exec -T rat_server rat-manage-api cfg add src=/pipe/export_admin_cfg.json
docker-compose exec -T rat_server rat-manage-api user create --role Super --role Admin \
--role AP --role Analysis --org fcc "admin@afc.com" "openafc"
docker-compose exec -T rat_server rat-manage-api cert_id create  --location 3 \
--cert_id FsDownloaderCertIdUS --ruleset_id US_47_CFR_PART_15_SUBPART_E
docker-compose exec -T rat_server rat-manage-api cert_id create  --location 3 \
--cert_id FsDownloaderCertIdCA --ruleset_id CA_RES_DBS-06
docker-compose exec -T rat_server rat-manage-api cert_id create  --location 3 \
--cert_id FsDownloaderCertIdBR --ruleset_id BRAZIL_RULESETID


# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
