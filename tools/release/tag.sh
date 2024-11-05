#!/bin/bash
#!/bin/sh
#
# Copyright (C) 2024 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

[ -z "$1" ] && echo "provide from tag name "&&exit || echo   "from $1"
[ -z "$2" ] && echo "provide to tag name "&&exit || echo "  to $2"

REPO="ghcr.io/open-afc-project"

# please keep sorted: 
AFC_SERVER="afcserver-image"     # AFC Server image
ALS_KAFKA="als-kafka-image"       # Kafka for ALS
ALS_SIPHON="als-siphon-image"     # ALS Siphon image
BULK_POSTGRES="bulk-postgres-image" # PostgreSQL for ALS
CADVISOR="cadvisor-image"
CERT_DB="cert_db"                 # cert db
DISPATCHER="dispatcher-image"     # dispatcher image
GEO_CONVERTERS="geo-converters-image"             # Geodetic converters
GRAFANA="grafana-image"
MSGHND="afc-msghnd"               # msghnd image
NGINXEXPORTER="nginxexporter-image"               # Nginx-exporter
OBJST="objstorage-image"          # object storage
PROMETHEUS="prometheus-image"
RATDB="ratdb-image"               # ratdb image
RCACHE="rcache-image"             # Respo Cache service
RMQ="rmq-image"                   # rabbitmq image
ULS_DOWNLOADER="uls-downloader"   # uls downloader
WEBUI="webui-image"               # server image
WORKER="afc-worker"        # msghnd image
WORKER_AL_D4B="worker-al-build-image"   # Alpine worker build img
WORKER_AL_PRINST="worker-al-preinstall" # Alpine worker preinst

from=$1
to=$2

docker login ${REPO}
images=(
  ${AFC_SERVER}
  ${ALS_KAFKA}
  ${ALS_SIPHON}
  ${BULK_POSTGRES}
  ${CADVISOR}
  ${CERT_DB}
  ${DISPATCHER}
  ${GEO_CONVERTERS}
  ${GRAFANA}
  ${MSGHND}
  ${NGINXEXPORTER}
  ${OBJST}
  ${PROMETHEUS}
  ${RATDB}
  ${RCACHE}
  ${RMQ}
  ${ULS_DOWNLOADER}
  ${WEBUI}
  ${WORKER}
  ${WORKER_AL_D4B}
  ${WORKER_AL_PRINST}
)

for img in ${images[@]}; do
	docker pull ${REPO}/${img}:${from} 
	docker tag ${REPO}/${img}:${from} ${REPO}/${img}:${to}
 	docker tag ${REPO}/${img}:${from} ${REPO}/${img}:latest
	docker push ${REPO}/${img}:${to}
	docker push ${REPO}/${img}:latest
done
