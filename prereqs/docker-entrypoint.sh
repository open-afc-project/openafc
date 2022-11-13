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
DISABLE_HTTPD=${DISABLE_HTTPD:=no}
CELERY_TYPE=${CELERY_TYPE:=internal}
if [ "$CELERY_TYPE" == "internal" ]; then
	CELERY_LOG=${CELERY_LOG:=INFO}
	if [ "$DISABLE_HTTPD" == "no" ]; then
		CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10"}
		/bin/celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG &
	else
		CELERY_OPTIONS=${CELERY_OPTIONS:="rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10"}
		/bin/celery multi start $CELERY_OPTIONS -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=$CELERY_LOG
		sleep infinity
	fi
fi
if [ "$DISABLE_HTTPD" == "no" ]; then
	# apache
	HTTPD_OPTIONS=${HTTPD_OPTIONS}
	echo "/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND >"
	/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND &
	wait
	exit $?
fi
