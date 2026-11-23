#!/bin/sh
#
# Strip monitoring upstreams and location blocks from the generated nginx config
# when AFC_GRAFANA_ENABLED is not 'true'. Runs after envsubst has processed the
# template, so it operates on the final config in /etc/nginx/conf.d/.
#
set -e

ME=$(basename "$0")

if [ "${AFC_GRAFANA_ENABLED}" = "true" ]; then
    echo "$ME: AFC_GRAFANA_ENABLED=true, keeping monitoring blocks"
    exit 0
fi

echo "$ME: AFC_GRAFANA_ENABLED=${AFC_GRAFANA_ENABLED:-<unset>}, stripping monitoring blocks"

CONF="/etc/nginx/conf.d/nginx.conf"
if [ ! -f "$CONF" ]; then
    exit 0
fi

sed -i '/# MONITORING_BEGIN/,/# MONITORING_END/d' "$CONF"
