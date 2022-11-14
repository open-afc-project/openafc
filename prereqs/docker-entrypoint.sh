#!/bin/sh

# postfix
/usr/sbin/postfix start

BROKER_TYPE=${BROKER_TYPE:=internal}
if [ "$BROKER_TYPE" == "internal" ]; then
	#rabbitmq
	rabbitmq-server &
	sleep 2
	rabbitmqctl add_vhost fbrat
	rabbitmqctl add_user celery celery
	rabbitmqctl set_user_tags celery administrator celery
	rabbitmqctl delete_user guest
	rabbitmqctl set_permissions celery '.*' '.*' '.*'
	rabbitmqctl set_permissions -p fbrat celery '.*' '.*' '.*'
	rabbitmqctl list_users
fi

#celery
CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10"}
CELERY_LOG=${CELERY_LOG:=INFO}
/bin/celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG &
if [ "$AFC_PROD_ENV" == "final" ]; then
    # gunicorn
    echo "GUNICORN"
    gunicorn --config gunicorn.conf.py --log-config gunicorn.logs.conf wsgi:app
else
    echo "APACHE"
    # apache
    HTTPD_OPTIONS=${HTTPD_OPTIONS}
    echo "/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND >"
    /usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND &
fi

wait

exit $?
