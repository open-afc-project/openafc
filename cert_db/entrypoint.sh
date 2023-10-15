#!/bin/sh

if [[ -z "${K8S_CRON}" ]]; then
  crond -b
  sleep infinity
else
  /usr/bin/rat-manage-api cert_id sweep --country US
  /usr/bin/rat-manage-api cert_id sweep --country CA
fi
