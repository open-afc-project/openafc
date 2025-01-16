#!/bin/sh

rat-manage-api db-create --if_absent

postfix start &

echo "httpd $HTTPD_OPTIONS -DFOREGROUND >"
exec httpd $HTTPD_OPTIONS -DFOREGROUND
