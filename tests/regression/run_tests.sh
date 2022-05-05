#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
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
        done
        unset pids
    done
}

cd $wd/tests
loop 58 20


# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
