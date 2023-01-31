#!/bin/sh

#if [[ ! -z "$HISTORY_HOST" ]]; then
#	sed -i "/<\/VirtualHost>/i  ProxyPassReverse \/dbg http:\/\/$HISTORY_HOST:4999\/dbg\n  ProxyPass \/dbg http:\/\/$HISTORY_HOST:4999\/dbg\n  ProxyPreserveHost On\n  ProxyRequests Off" $(ls /etc/httpd/conf.d/*-notls.conf)
#	sed -i "/<\/VirtualHost>/i  ProxyPassReverse \/dbg http:\/\/$HISTORY_HOST:4999\/dbgs\n  ProxyPass \/dbg http:\/\/$HISTORY_HOST:4999\/dbgs\n  ProxyPreserveHost On\n  ProxyRequests Off" $(ls /etc/httpd/conf.d/*-tls.conf)
#fi
#HTTPD_OPTIONS=${HTTPD_OPTIONS}
#echo "/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND >"
#/usr/sbin/httpd $HTTPD_OPTIONS -DFOREGROUND &
#
sleep infinity

exit $?
