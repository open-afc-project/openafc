#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -x

PRINST_DEV="public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image" # preinst image name
D4B_DEV="public.ecr.aws/w9v6y1o0/openafc/centos-build-image"         # dev image name
SRV_DI="110738915961.dkr.ecr.us-east-1.amazonaws.com/afc-server"     # server image
OBJST_DI="public.ecr.aws/w9v6y1o0/openafc/objstorage-image"          # object storage
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
  file=${1}    # Name of the Dockerfile
  image=${2}  # Name and optionally a tag in the 'name:tag' format
  args=${3}

  msg " docker_build_and_push args:  ${args}"

  docker_build ${file} ${image} "${args}"
  if [ $? -eq 0 ]; then
    docker_push ${image}
  fi
}

git_clone() {
  addr=${1}
  wd=${2}
  git clone ${addr} ${wd}
  check_ret $?
  # ls -la ${wd}
}

build_dev_server() {
  wd=${1}
  tag=${2}
  cd ${wd}

  aws ecr-public get-login-password --region us-east-1 | docker login --username AWS --password-stdin public.ecr.aws/w9v6y1o0
  BUILDREV=`git rev-parse --short HEAD`
  (trap 'kill 0' SIGINT; docker_build_and_push ${wd}/dockerfiles/Dockerfile-openafc-centos-preinstall-image ${PRINST_DEV}:${tag} & docker_build_and_push ${wd}/dockerfiles/Dockerfile-for-build ${D4B_DEV}:${tag})
  cd ${wd}/src/filestorage && docker_build_and_push Dockerfile ${OBJST_DI}:${tag} ; cd ${wd}
  aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin 110738915961.dkr.ecr.us-east-1.amazonaws.com
  EXT_ARGS="--build-arg BLD_TAG=${tag} --build-arg PRINST_TAG=${tag} --build-arg BLD_NAME=${D4B_DEV} --build-arg PRINST_NAME=${PRINST_DEV} --build-arg BUILDREV=${BUILDREV}"
  docker_build_and_push ${wd}/Dockerfile ${SRV_DI}:${tag} "${EXT_ARGS}"
}

build_dev_server $1 $2

# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
