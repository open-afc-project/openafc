#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

trap 'kill 0' SIGINT
echo `pwd`
wd=${1}
di_name=${2}
addr=${3}
port=${4:-443}
prot=${5:-"https"}
burst=${6:-8}
testcase_ids=${7}
ext_args=${8}
ap_count=$(docker run --rm ${di_name} --cmd get_nbr_testcases;echo $?)

source $wd/tests/regression/regression.sh
results_dir=$wd/results
mkdir -p $results_dir


retval=0
check_retval() {
  ret=${1}  # only 0 is OK

  if [ ${ret} -eq 0 ]; then
    ok "OK"
    else err "FAIL"; retval=1
  fi
}

verify_tls=''
if [ "$prot" == "https" ]; then
    verify_tls='--verif'
fi
# Disable TLS verification to get server working without proper certs
verify_tls=""

echo "verify_tls - $verify_tls"


afc_tests() {
    # docker run --network='host' --rm ${di_name} --addr=${addr} --port=${port} --prot=${prot} \
    #     --cmd=run --prefix_cmd  /usr/app/certs.sh cert_client \
    #     ${verify_tls} ${ext_args} \
    #     --cli_cert /usr/app/test_cli/test_cli_crt.pem \
    #     --cli_key /usr/app/test_cli/test_cli_key.pem "$@"

  docker run --network='host' --mount "type=bind,source=${results_dir},target=${results_dir}" --rm ${di_name} --addr=${addr} \
     --port=${port} --prot=${prot} --cmd=run ${verify_tls} ${ext_args} "$@"
}

loop() {
    start=0
    max=${1}
    step=${2:-10}
    s=${start}
    while [ $s -le $max ]
    do
        e=$(($((s + ${step}))<=${max} ? $((s + ${step})) : ${max}))
        echo "from $s  to $e"
        # run processes and store pids in array
        for i in `seq $((s+1)) ${e}`; do
            afc_tests --testcase_indexes=${i} --outfile=${results_dir}/test-output.csv &
            pids+=( $! )
        done
        s=$((s + ${step}))
        # wait for all pids
        for pid in ${pids[*]}; do
            wait $pid
            check_retval $?
        done
        unset pids
    done
    return $retval
}

cd $wd
if [ "${testcase_ids}" == "" ]; then
    loop $ap_count ${burst}
else
    afc_tests --testcase_ids=${testcase_ids}  --outfile=${results_dir}/test-output.csv
    check_retval $?
    return $retval
fi


# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
