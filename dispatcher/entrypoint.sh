#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Running debug profile" 
    ACCEPTOR_LOG_LEVEL="--log debug"
    BIN=nginx-debug
    apk add --update --no-cache bash
    ;;
  "production")
    echo "Running production profile"
    ACCEPTOR_LOG_LEVEL=
    BIN=nginx
    ;;
  *)
    echo "Uknown profile"
    ACCEPTOR_LOG_LEVEL=
    BIN=nginx
    ;;
esac

/docker-entrypoint.sh $BIN -g "daemon off;" &

/wd/acceptor.py $ACCEPTOR_LOG_LEVEL --cmd run

exit $?
