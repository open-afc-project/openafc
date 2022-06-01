#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program


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
retval=0
check_ret() {
  ret=${1}  # only 0 is OK

  if [ ${ret} -eq 0 ]; then
    ok "OK"
    else err "FAIL"; retval=1
  fi
}

trap 'kill 0' SIGINT
echo `pwd`
wd=${1}
di_name=${2}
addr=${3}
port=${4:-443}

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
            docker run --rm ${di_name} --cfg addr=${addr} port=${port} nodbg nogui --test r=${i} &
            pids+=( $! )
        done
        s=$((s + ${step}))
        # wait for all pids
        for pid in ${pids[*]}; do
            wait $pid
            check_ret $?
        done
        unset pids
    done
    return $retval
}

cd $wd/tests
loop 58 20


# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
