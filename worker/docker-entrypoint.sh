#!/bin/sh
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ -n ${AFC_AEP_ENABLE+x} ] && [ -n "$AFC_AEP_REAL_MOUNTPOINT" ]; then
	if [ -n "$AFC_AEP_FILELIST" ]; then
		/usr/bin/parse_fs.py "$AFC_AEP_REAL_MOUNTPOINT" "$AFC_AEP_FILELIST"
	else
		/usr/bin/parse_fs.py "$AFC_AEP_REAL_MOUNTPOINT" /mnt/nfs/rat_transfer/aep.list
		export AFC_AEP_FILELIST=/mnt/nfs/rat_transfer/aep.list
	fi
fi

#celery
CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1"}
CELERY_LOG=${CELERY_LOG:=DEBUG}
celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG &

sleep infinity
