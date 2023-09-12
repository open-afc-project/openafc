#!/bin/bash
# Portions copyright © 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate
# affiliate that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy
# of which is included with this software program.

# run: runAllTests.sh <csv_file> <outdir> <db_file> <configfile> <email>
# db_file will be created automatically if not exist.  db_file tracks previous already run requests.
#
python3 csv_to_request_json.py  $1
python3 send_requests.py --csv $1 --res $2 --db $3 --config $4 --email $5


