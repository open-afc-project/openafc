#!/bin/sh

# postfix
/usr/sbin/postfix start

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

#celery
/bin/sh -c '/bin/celery multi start rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10 -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=INFO'

# apache
/usr/sbin/httpd $OPTIONS -DFOREGROUND &

wait

exit $?