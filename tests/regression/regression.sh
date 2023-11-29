#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

PRIV_REPO="${PRIV_REPO:=110738915961.dkr.ecr.us-east-1.amazonaws.com}"
PUB_REPO="${PUB_REPO:=public.ecr.aws/w9v6y1o0/openafc}"

SRV="${PRIV_REPO}/afc-server"                                         # server image
MSGHND="${PRIV_REPO}/afc-msghnd"                             # msghnd image
OBJST="${PUB_REPO}/objstorage-image"                         # object storage
RATDB=${PUB_REPO}"/ratdb-image"                                  # ratdb image
RMQ="${PUB_REPO}/rmq-image"                                        # rabbitmq image
DISPATCHER="${PUB_REPO}/dispatcher-image"               # dispatcher image
ALS_SIPHON="${PUB_REPO}/als-siphon-image"               # ALS Siphon image
ALS_KAFKA="${PUB_REPO}/als-kafka-image"                   # Kafka for ALS
BULK_POSTGRES="${PUB_REPO}/bulk-postgres-image" # PostgreSQL for bulk stuff (ALS, req cache, etc.)
RCACHE="${PUB_REPO}/rcache-image"                             # Request cache
GRAFANA="${PUB_REPO}/grafana-image"                           # Grafana
PROMETHEUS="${PUB_REPO}/prometheus-image"                     # Prometheus
CADVISOR="${PUB_REPO}/cadvisor-image"                         # Cadvisor
NGINXEXPORTER="${PUB_REPO}/nginxexporter-image"               # Nginx-exporter

WORKER=${PRIV_REPO}"/afc-worker"                                    # msghnd image
WORKER_AL_D4B="${PUB_REPO}/worker-al-build-image" # Alpine worker build img
WORKER_AL_PRINST="${PUB_REPO}/worker-al-preinstall" # Alpine worker preinst

ULS_UPDATER=${PRIV_REPO}"/uls-updater"                # ULS Updater image
ULS_DOWNLOADER="${PUB_REPO}/uls-downloader" # ULS Downloader image

CERT_DB="${PUB_REPO}/cert_db"                    # CERT DB image
RTEST_DI="rtest"                                                 # regression tests image


# FUNCS
msg() {
   echo -e "\e[34m \e[1m$1\e[0m"
}
err() {
  echo -e "\e[31m$1\e[0m"
}
ok() {
  echo -e "\e[32m$1\e[0m"
}

check_ret() {
  ret=${1}  # only 0 is OK

  if [ ${ret} -eq 0 ]; then
    ok "OK"
    else err "FAIL"; exit ${ret}
  fi
}

docker_build() {
  file=${1}    # Name of the Dockerfile
  image=${2}   # Name and optionally a tag in the 'name:tag' format
  args=${3}
  msg "docker build ${file} file into ${image} image extra args: ${args}"
  docker build . -f ${file} -t ${image} ${args}
  check_ret $?
}

docker_push() {
  image=${1}      # Name and optionally a tag in the 'name:tag' format

  msg "docker push  ${image} image"
  docker push ${image}
  check_ret $?
}

docker_build_and_push() {
  file=${1}     # Name of the Dockerfile
  image=${2}    # Name and optionally a tag in the 'name:tag' format
  push=${3:-1}  # whether push new docker images into repo [0/1]
  args=${4}     # extra arguments

  msg " docker_build_and_push push:${push} args:${args}"

  docker_build ${file} ${image} "${args}"

  if [ $? -eq 0 ]; then
    if [ ${push} -eq 1 ]; then
      docker_push ${image}
    fi
  fi
}
docker_login () {
  pub_repo_login=${1}
  priv_repo_login=${2}
  if test ${pub_repo_login}; then
    err "FAIL \"${pub_repo_login}" not defined"; exit $?
  fi
  if test ${priv_repo_login}; then
    err "FAIL \"${priv_repo_login}" not defined"; exit $?
  fi
  "${pub_repo_login}" && "${priv_repo_login}"
  check_ret $?
}


build_dev_server() {
  wd=${1}       # full path to the afc project dir
  tag=${2}      # tag to be used for new docker images
  push=${3:-1}  # whether push new docker images into repo [0/1]
  # cd to a work dir
  cd ${wd}
  # get last git commit hash number
  BUILDREV=`git rev-parse --short HEAD`

# if login if docker push required
#  if [ ${push} -eq 1 ]; then
#    docker_login "${pub_repo_login}" "${priv_repo_login}"
#  fi

  # build regression test docker image
  cd ${wd}
  docker_build tests/Dockerfile ${RTEST_DI}:${tag}
  check_ret $?

  # build in parallel server docker prereq images (preinstall and docker_for_build)
    docker_build_and_push ${wd}/worker/Dockerfile.build ${WORKER_AL_D4B}:${tag} ${push} &
    docker_build_and_push ${wd}/worker/Dockerfile.preinstall ${WORKER_AL_PRINST}:${tag} ${push} &

  msg "wait for prereqs to be built"
  # wait for background jobs to be done
  wait
  msg "prereqs are built"

  # build of ULS dockers
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${WORKER_AL_D4B} --build-arg PRINST_NAME=${WORKER_AL_PRINST} --build-arg BUILDREV=${BUILDREV}"
  docker_build_and_push ${wd}/uls/Dockerfile-uls_updater ${ULS_UPDATER}:${tag} ${push} "${EXT_ARGS}" &
  docker_build_and_push ${wd}/uls/Dockerfile-uls_service ${ULS_DOWNLOADER}:${tag} ${push} "${EXT_ARGS}" &

  # build msghnd  (flask + gunicorn)
  docker_build_and_push ${wd}/msghnd/Dockerfile ${MSGHND}:${tag} ${push} &

  # build worker image
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${WORKER_AL_D4B} --build-arg PRINST_NAME=${WORKER_AL_PRINST} --build-arg BUILDREV=worker"
  docker_build_and_push ${wd}/worker/Dockerfile   ${WORKER}:${tag} ${push} "${EXT_ARGS}" &

  # build afc ratdb docker image
  docker_build_and_push ${wd}/ratdb/Dockerfile ${RATDB}:${tag} ${push} &

  # build afc dynamic data storage image
  docker_build_and_push ${wd}/objstorage/Dockerfile ${OBJST}:${tag} ${push}&
  cd ${wd}

  # build afc rabbit MQ docker image
  docker_build_and_push ${wd}/rabbitmq/Dockerfile ${RMQ}:${tag} ${push} &

  # build afc dispatcher docker image
  docker_build_and_push ${wd}/dispatcher/Dockerfile ${DISPATCHER}:${tag} ${push} &

  # build afc server docker image
  EXT_ARGS="--build-arg BUILDREV=${BUILDREV}"
  docker_build_and_push ${wd}/rat_server/Dockerfile ${SRV}:${tag}  ${push} "${EXT_ARGS}"

  # build ALS-related images
  cd ${wd}/als && docker_build_and_push Dockerfile.siphon ${ALS_SIPHON}:${tag} ${push} &
  cd ${wd}/als && docker_build_and_push Dockerfile.kafka ${ALS_KAFKA}:${tag} ${push} &
  cd ${wd}/bulk_postgres && docker_build_and_push Dockerfile ${BULK_POSTGRES}:${tag} ${push} &
  cd ${wd}

   # build cert db image
  docker_build_and_push ${wd}/cert_db/Dockerfile ${CERT_DB}:${tag} ${push} &

  # Build Request Cache
  docker_build_and_push ${wd}/rcache/Dockerfile ${RCACHE}:${tag} ${push} &

  # Build Prometheus-related images
  cd ${wd}/prometheus && docker_build_and_push Dockerfile-prometheus ${PROMETHEUS}:${tag} ${push} &
  cd ${wd}/prometheus && docker_build_and_push Dockerfile-cadvisor ${CADVISOR}:${tag} ${push} &
  cd ${wd}/prometheus && docker_build_and_push Dockerfile-cadvisor ${NGINXEXPORTER}:${tag} ${push} &
  cd ${wd}/prometheus && docker_build_and_push Dockerfile-prometheus ${GRAFANA}:${tag} ${push} &

  msg "wait for all images to be built"
  wait
  msg "-done-"
}
# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
