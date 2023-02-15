#!/bin/sh
#
#
AFC_MSGHND_BIND=${AFC_MSGHND_BIND:="0.0.0.0:8000"}
AFC_MSGHND_PID=${AFC_MSGHND_PID:="/run/gunicorn/openafc_app.pid"}
AFC_MSGHND_ACCESS_LOG=${AFC_MSGHND_ACCESS_LOG:="/proc/self/fd/2"}
AFC_MSGHND_ERROR_LOG=${AFC_MSGHND_ERROR_LOG:="/proc/self/fd/2"}
AFC_MSGHND_LOG_LEVEL=${AFC_MSGHND_LOG_LEVEL:="info"}
AFC_MSGHND_TIMEOUT=${AFC_MSGHND_TIMEOUT:=180}
AFC_MSGHND_WORKERS=${AFC_MSGHND_WORKERS:=20}

gunicorn \
--bind "${AFC_MSGHND_BIND}" \
--pid "${AFC_MSGHND_PID}" \
--workers "${AFC_MSGHND_WORKERS}" \
--timeout "${AFC_MSGHND_TIMEOUT}" \
--access-logfile "${AFC_MSGHND_ACCESS_LOG}" \
--error-logfile "${AFC_MSGHND_ERROR_LOG}" \
--log-level "${AFC_MSGHND_LOG_LEVEL}" \
wsgi:app

#
sleep infinity

exit $?
