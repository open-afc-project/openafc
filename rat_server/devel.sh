#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:=production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Debug profile" 
    export NODE_OPTIONS='--openssl-legacy-provider'
    apk add --update --no-cache cmake ninja yarn bash
    ;;
  "production")
    echo "Production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

exit $?
