#!/bin/sh

postfix start &

echo "httpd $HTTPD_OPTIONS -DFOREGROUND >"
exec httpd $HTTPD_OPTIONS -DFOREGROUND
