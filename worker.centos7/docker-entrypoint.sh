#!/bin/sh

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
	source /opt/rh/$(scl -l)/enable
fi

#celery
CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1"}
CELERY_LOG=${CELERY_LOG:=DEBUG}
/bin/celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG &

sleep infinity