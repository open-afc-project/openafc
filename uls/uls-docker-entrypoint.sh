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
    apk add --update --no-cache bash
    ;;
  "production")
    echo "Running production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

AFC_ULS_OUTPUT=/output_folder
cd /mnt/nfs/rat_transfer/daily_uls_parse
python3 daily_uls_parse.py &&
echo "Copy results to $AFC_ULS_OUTPUT" &&
cp /mnt/nfs/rat_transfer/ULS_Database/*.sqlite3 $AFC_ULS_OUTPUT/
