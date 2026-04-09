#!/bin/bash
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
AFC_SERVER="afcserver-image"
ALS_KAFKA="als-kafka-image"
ALS_SIPHON="als-siphon-image"
BULK_POSTGRES="bulk-postgres-image"
CADVISOR="cadvisor-image"
CERT_DB="cert_db"
DISPATCHER="dispatcher-image"
GEO_CONVERTERS="geo-converters-image"
GRAFANA="grafana-image"
LOKI="loki-image"
MSGHND="afc-msghnd"
NGINXEXPORTER="nginxexporter-image"
OBJST="objstorage-image"
PROMETHEUS="prometheus-image"
PROMTAIL="promtail-image"
RATDB="ratdb-image"
RCACHE="rcache-image"
RMQ="rmq-image"
ULS_DOWNLOADER="uls-downloader"
ULS_UPDATER="uls-updater"
WEBUI="webui-image"
WORKER="afc-worker"

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
  ${LOKI}
  ${MSGHND}
  ${NGINXEXPORTER}
  ${OBJST}
  ${PROMETHEUS}
  ${PROMTAIL}
  ${RATDB}
  ${RCACHE}
  ${RMQ}
  ${ULS_DOWNLOADER}
  ${ULS_UPDATER}
  ${WEBUI}
  ${WORKER}
)

for img in ${images[@]}; do
    src_ref="${REPO}/${img}:${from}"
    digest=$(docker buildx imagetools inspect --format '{{.Manifest.Digest}}' \
        "${src_ref}" 2>/dev/null)
    if [ -z "${digest}" ]; then
        echo "ERROR: could not resolve digest for ${src_ref} — skipping" >&2
        continue
    fi
    # Pull by digest so we get exactly the manifest we inspected above,
    # not whatever the mutable tag points to at pull time.
    pinned_ref="${REPO}/${img}@${digest}"
    echo "Pulling ${pinned_ref}  (tag: ${from})"
    docker pull "${pinned_ref}"
    docker tag  "${pinned_ref}" "${REPO}/${img}:${to}"
    docker tag  "${pinned_ref}" "${REPO}/${img}:latest"
    docker push "${REPO}/${img}:${to}"
    docker push "${REPO}/${img}:latest"
done
