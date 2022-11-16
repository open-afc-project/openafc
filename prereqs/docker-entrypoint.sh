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

# apache
HTTPD_OPTIONS=${HTTPD_OPTIONS}
echo "/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND >"
/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND &

wait
exit $?
