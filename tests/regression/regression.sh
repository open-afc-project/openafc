#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

SRV="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server"    # server image
D4B="public.ecr.aws/w9v6y1o0/openafc/centos-build-image"         # CentOS dev image name
PRINST="public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image" # preinst image name

MSGHND="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-msghnd"         # msghnd image
MSGHND_PRINST="public.ecr.aws/w9v6y1o0/openafc/centos-msghnd-preinstall" # msghnd preinstall image name

WORKER="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-worker"        # msghnd image
WORKER_AL_D4B="public.ecr.aws/w9v6y1o0/openafc/worker-al-build-image"   # Alpine worker build img
WORKER_AL_PRINST="public.ecr.aws/w9v6y1o0/openafc/worker-al-preinstall" # Alpine worker preinst

OBJST="public.ecr.aws/w9v6y1o0/openafc/objstorage-image"          # object storage
RMQ="public.ecr.aws/w9v6y1o0/openafc/rmq-image"                   # rabbitmq image
NGNX="public.ecr.aws/w9v6y1o0/openafc/ngnx-image"                 # ngnx image
RTEST_DI="rtest"                                                  # regression tests image

# TODO: deprecated, will be removed in future release
PRINST_WRKR_DI="public.ecr.aws/w9v6y1o0/openafc/centos-worker-preinstall" # worker preinst image name


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

docker_login() {
# TODO: currently it is hardcoded to login into openAFC repo
# should be implemented in more generic way
  aws ecr-public get-login-password --region us-east-1 | docker login --username AWS --password-stdin public.ecr.aws/w9v6y1o0
  check_ret $?
  aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin 110738915961.dkr.ecr.us-east-1.amazonaws.com
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

  if [ ${push} -eq 1 ]; then
    docker_login
  fi

  # build regression test docker image
  cd ${wd}/tests && docker_build Dockerfile ${RTEST_DI}:${tag}; cd ${wd}

  # build in parallel server docker prereq images (preinstall and docker_for_build)
    docker_build_and_push ${wd}/dockerfiles/Dockerfile-openafc-centos-preinstall-image ${PRINST}:${tag} ${push} &
    docker_build_and_push ${wd}/dockerfiles/Dockerfile-for-build ${D4B}:${tag} ${push} &
    docker_build_and_push ${wd}/worker/Dockerfile.build ${WORKER_AL_D4B}:${tag} ${push} &
    docker_build_and_push ${wd}/worker/Dockerfile.preinstall ${WORKER_AL_PRINST}:${tag} ${push} &
    docker_build_and_push ${wd}/msghnd/Dockerfile.preinstall ${MSGHND_PRINST}:${tag} ${push} &

  msg "wait for prereqs to be built"
  # wait for background jobs to be done
  wait
  msg "prereqs are built"

  # build msghnd  (flask + gunicorn)
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${D4B} --build-arg PRINST_NAME=${MSGHND_PRINST} --build-arg BUILDREV=msghnd"
  docker_build_and_push ${wd}/msghnd/Dockerfile ${MSGHND}:${tag} ${push} "${EXT_ARGS}" &

  # build worker image
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${WORKER_AL_D4B} --build-arg PRINST_NAME=${WORKER_AL_PRINST} --build-arg BUILDREV=worker"
  docker_build_and_push ${wd}/worker/Dockerfile   ${WORKER}:${tag} ${push} "${EXT_ARGS}" &

  # build afc dynamic data storage image
  cd ${wd}/src/filestorage && docker_build_and_push Dockerfile ${OBJST}:${tag} ${push}&
  cd ${wd}

  # build afc rabbit MQ docker image
  cd ${wd}/rabbitmq && docker_build_and_push Dockerfile ${RMQ}:${tag} ${push} &
  cd ${wd}

  # build afc nginx docker image
  cd ${wd}/nginx && docker_build_and_push Dockerfile ${NGNX}:${tag} ${push} &
  cd ${wd}

  # build afc server docker image
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${D4B} --build-arg PRINST_NAME=${PRINST} --build-arg BUILDREV=${BUILDREV}"
  docker_build_and_push Dockerfile ${SRV}:${tag}  ${push} "${EXT_ARGS}" &
  msg "wait for prereqs to be built"

  msg "wait for all images to be built"
  wait
  msg "-done-"
}

# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
