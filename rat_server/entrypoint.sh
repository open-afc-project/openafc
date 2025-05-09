#!/bin/sh

AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Running debug profile"
    AFC_WEBUI_LOG_LEVEL="debug"
    echo "AFC_WEBUI_PORT = ${AFC_WEBUI_PORT}"
    echo "AFC_WEBUI_BIND = ${AFC_WEBUI_BIND}"
    echo "AFC_WEBUI_PID = ${AFC_WEBUI_PID}"
    echo "AFC_WEBUI_ACCESS_LOG = ${AFC_WEBUI_ACCESS_LOG}"
    echo "AFC_WEBUI_ERROR_LOG = ${AFC_WEBUI_ERROR_LOG}"
    echo "AFC_WEBUI_TIMEOUT = ${AFC_WEBUI_TIMEOUT}"
    echo "AFC_WEBUI_WORKERS = ${AFC_WEBUI_WORKERS}"
    echo "AFC_WEBUI_LOG_LEVEL = ${AFC_WEBUI_LOG_LEVEL}"
    echo "FLASK_AFC_WEBUI_RATAFC_TOUT = ${FLASK_AFC_WEBUI_RATAFC_TOUT}"
    ;;
  "production")
    echo "Running production profile"
    AFC_WEBUI_LOG_LEVEL="info"
    ;;
  *)
    echo "Uknown profile"
    AFC_WEBUI_LOG_LEVEL="info"
    ;;
esac


rat-manage-api db-create --if_absent
rat-manage-api db-upgrade

postfix start &
exec gunicorn \
--bind "${AFC_WEBUI_BIND}:${AFC_WEBUI_PORT}" \
--pid "${AFC_WEBUI_PID}" \
--workers "${AFC_WEBUI_WORKERS}" \
--timeout "${AFC_WEBUI_TIMEOUT}" \
${AFC_WEBUI_ACCESS_LOG:+--access-logfile "$AFC_WEBUI_ACCESS_LOG"} \
--error-logfile "${AFC_WEBUI_ERROR_LOG}" \
--log-level "${AFC_WEBUI_LOG_LEVEL}" \
--worker-class gevent \
--forwarder-headers SCRIPT_NAME,PATH_INFO,REMOTE_USER \
wsgi:app
