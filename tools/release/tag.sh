#!/bin/bash
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

#aws ecr-public get-login-password --region us-east-1 | docker login --username AWS --password-stdin public.ecr.aws/w9v6y1o0
#aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin 110738915961.dkr.ecr.us-east-1.amazonaws.com

#images=(${RCACHE} ${CERT_DB} ${RATDB} ${WEBUI} ${MSGHND} ${WORKER} ${WORKER_AL_D4B} ${WORKER_AL_PRINST} ${OBJST} ${RMQ} ${DISPATCHER} ${ALS_SIPHON} ${ALS_KAFKA} ${BULK_POSTGRES} ${ULS_DOWNLOADER} ${GRAFANA} ${PROMETHEUS} ${CADVISOR} ${AFC_SERVER} ${NGINXEXPORTER} ${GEO_CONVERTERS})
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





#docker images | grep ${from} 
#docker tag dr942120/srv_di:${from} 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server:${to}
#docker tag dr942120/srv_di:${from} 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server:latest
#docker tag dr942120/centos-build-image:${from} public.ecr.aws/w9v6y1o0/openafc/centos-build-image:${to}
#docker tag dr942120/centos-build-image:${from} public.ecr.aws/w9v6y1o0/openafc/centos-build-image:latest
#docker tag dr942120/centos-preinstall-image:${from} public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image:${to}
#docker tag dr942120/centos-preinstall-image:${from} public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image:latest


#docker images | grep ${to} 
#echo docker push 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server:${to}
#echo docker push 110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server:latest
#echo docker push public.ecr.aws/w9v6y1o0/openafc/centos-build-image:${to}
#echo docker push public.ecr.aws/w9v6y1o0/openafc/centos-build-image:latest
#echo docker push public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image:${to}
#echo docker push public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image:latest

