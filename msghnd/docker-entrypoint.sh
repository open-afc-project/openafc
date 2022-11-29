#!/bin/sh

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

# gunicorn
gunicorn --config gunicorn.conf.py --log-config gunicorn.logs.conf wsgi:app

sleep infinity

exit $?
