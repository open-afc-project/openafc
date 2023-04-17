#!/bin/sh
#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ ! -z ${AFC_AEP_ENABLE+x} ]; then
	if [ -z "$AFC_AEP_DEBUG" ]; then
		export AFC_AEP_DEBUG=0
	fi
	if [ -z "$AFC_AEP_LOGFILE" ]; then
		export AFC_AEP_LOGFILE=/aep/log/aep.log
	fi
	mkdir -p $(dirname "$AFC_AEP_LOGFILE")
	if [ -z "$AFC_AEP_REAL_MOUNTPOINT" ]; then
		export AFC_AEP_REAL_MOUNTPOINT=/mnt/nfs/rat_transfer
	fi
	if [ -z "$AFC_AEP_ENGINE_MOUNTPOINT" ]; then
		export AFC_AEP_ENGINE_MOUNTPOINT=$AFC_AEP_REAL_MOUNTPOINT
	fi
	if [ -z "$AFC_AEP_FILELIST" ]; then
		export AFC_AEP_FILELIST=/aep/list/aep.list
	fi
	mkdir -p $(dirname "$AFC_AEP_FILELIST")
	if [ -z "$AFC_AEP_CACHE_MAX_FILE_SIZE" ]; then
		#50M
		export AFC_AEP_CACHE_MAX_FILE_SIZE=50000000
	fi
	if [ -z "$AFC_AEP_CACHE_MAX_SIZE" ]; then
		#1G
		export AFC_AEP_CACHE_MAX_SIZE=1000000000
	fi
	if [ -z "$AFC_AEP_CACHE" ]; then
		export AFC_AEP_CACHE=/aep/cache
	fi
	mkdir -p $AFC_AEP_CACHE
	/usr/bin/parse_fs.py "$AFC_AEP_REAL_MOUNTPOINT" "$AFC_AEP_FILELIST"
	export AFC_ENGINE="/usr/bin/afc-engine.sh"
else
	export AFC_ENGINE="/usr/bin/afc-engine"
fi

#celery
CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1"}
CELERY_LOG=${CELERY_LOG:=WARNING}
celery multi start $CELERY_OPTIONS -A afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/1/fd/2 --loglevel=$CELERY_LOG &

sleep infinity
