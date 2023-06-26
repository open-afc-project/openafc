#!/bin/sh

echo "httpd $HTTPD_OPTIONS -DFOREGROUND >"
httpd $HTTPD_OPTIONS -DFOREGROUND &
#
postfix start
sleep infinity

exit $?
