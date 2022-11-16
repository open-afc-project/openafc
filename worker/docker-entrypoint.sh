#!/bin/sh

#celery
CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1"}
CELERY_LOG=${CELERY_LOG:=DEBUG}
/bin/celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG &

sleep infinity