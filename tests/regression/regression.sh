#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

PRINST_DEV="public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image" # preinst image name
D4B_DEV="public.ecr.aws/w9v6y1o0/openafc/centos-build-image"         # dev image name
SRV_DI="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server"     # server image
OBJST_DI="public.ecr.aws/w9v6y1o0/openafc/objstorage-image"          # object storage
RMQ_DI="public.ecr.aws/w9v6y1o0/openafc/rmq-image"                   # rabbitmq image
NGNX_DI="public.ecr.aws/w9v6y1o0/openafc/ngnx-image"                 # ngnx image
RTEST_DI="rtest"                                                     # regression tests image
# ADDR="git@github.com:Telecominfraproject/open-afc.git"


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
  (trap 'kill 0' SIGINT; docker_build_and_push ${wd}/dockerfiles/Dockerfile-openafc-centos-preinstall-image ${PRINST_DEV}:${tag} ${push} & docker_build_and_push ${wd}/dockerfiles/Dockerfile-for-build ${D4B_DEV}:${tag} ${push})

  # build afc dynamic data storage image
  cd ${wd}/src/filestorage && docker_build_and_push Dockerfile ${OBJST_DI}:${tag} ${push}; cd ${wd}

  # build afc rabbit MQ docker image
  cd ${wd}/rabbitmq && docker_build_and_push Dockerfile ${RMQ_DI}:${tag} ${push}; cd ${wd}

  # build afc nginx docker image
  cd ${wd}/nginx && docker_build_and_push Dockerfile ${NGNX_DI}:${tag} ${push}; ${wd}

  # build afc server docker image
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${D4B_DEV} --build-arg PRINST_NAME=${PRINST_DEV} --build-arg BUILDREV=${BUILDREV}"
  docker_build_and_push Dockerfile ${SRV_DI}:${tag}  ${push} "${EXT_ARGS}"
}

# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
