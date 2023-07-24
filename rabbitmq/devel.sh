#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Debug profile" 
    set eux
    rabbitmq-plugins enable --offline rabbitmq_management
    rm -f /etc/rabbitmq/conf.d/20-management_agent.disable_metrics_collector.conf
    cp /plugins/rabbitmq_management-*/priv/www/cli/rabbitmqadmin /usr/local/bin/rabbitmqadmin
    chmod +x /usr/local/bin/rabbitmqadmin
    apk add --no-cache python3
    rabbitmqadmin --version
    cat << EOF >> /etc/rabbitmq/rabbitmq.conf
log.console.level = debug
log.connection.level = debug
log.channel.level = debug
log.queue.level = debug
log.default.level = debug
EOF
    ;;
  "production")
    echo "Production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

exit $?
