#!/bin/bash
#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

set -x

# 1.1 start new instances for build and dev_srvr on AWS (afc-build,afc-dev )

# BUILD Images
# 1.2 ssh afc-build: git clone test_branch
# 1.3 ssh afc-build: build_and_push d4b_dev, build_and_push preinst_dev (in parallel)
# 1.4 ssh afc-build: buid_and_push srvr_di (d4b_dev, preinst_dev)
# 1.5 ssh afc-dev: reload srv_di

# RUN TESTS
# 1.6 githubA:  run_regression on afc-dev

# IF PASSED
# 1.7 ssh afc-build: push d4b_dev:latest, d4b_dev:GitHash to docker_hub_repo
# 1.8 ssh afc-build: push preinst_dev:latest, preinst_dev:GitHash to docker_hub_repo
# 1.9 Ssh afc-build: push srvr_di:latest,srvr_di:latest:GitHash to the repo (for prod)

# 1.10 cleanup
#   remove from docker: repo d4b_dev, preinst_dev, srvr_di
#   kill (afc-build)
#   kill (afc-dev)
#   rm test branch

#    * build_and_push - pushes docker image to dev_test repo for tests
#    * afc-build, afc0-dev can be the same or different.
#    * afc-build can be different at every step
#    * d4b_dev - Docker-for-Build
#    * preinst_dev - centos-preinstall image
#    * srv_di - OpenAFC server docker image with latest changes


# 1.2 ssh afc-build: git clone test_branch

PRINST_DEV="dr942120/centos-preinstall-image" # Dockerfile-openafc-centos-preinstall dev image name
D4B_DEV="dr942120/centos-build-image"         # Dockerfile-for-build dev image name
SRV_DI="dr942120/srv_di"                   # Dockerfile dev image name
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

docker_build_and_push_server () {
  file=${1}         # Name of the Dockerfile
  image=${2}        # Name and optionally a tag in the 'name:tag' format
  d4b_tag=${3}      # base docker_for_build tag
  preinst_tag=${4}  # base preinst image tag
  d4b_name=${5}     # base docker_for_build image name
  preinst_name=${6} # base preinst image name

  docker_build ${file} ${image} "--build-arg BLD_TAG=${d4b_tag} --build-arg PRINST_TAG=${preinst_tag} --build-arg BLD_NAME=${d4b_name} --build-arg PRINST_NAME=${preinst_name}"
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
  rand=${2}
  cd ${wd}
  (trap 'kill 0' SIGINT; docker_build_and_push ${wd}/dockerfiles/Dockerfile-openafc-centos-preinstall-image ${PRINST_DEV}:${rand} & docker_build_and_push ${wd}/dockerfiles/Dockerfile-for-build ${D4B_DEV}:${rand})
  EXT_ARGS="--build-arg BLD_TAG=${rand} --build-arg PRINST_TAG=${rand} --build-arg BLD_NAME=${D4B_DEV} --build-arg PRINST_NAME=${PRINST_DEV}"
  docker_build_and_push ${wd}/Dockerfile ${SRV_DI}:${rand} "${EXT_ARGS}"
  BUILDREV=`git rev-parse --short HEAD`
  docker_build ${wd}/Dockerfile ${SRV_DI}:${rand} "--build-arg BLD_TAG=${rand} --build-arg PRINST_TAG=${rand} --build-arg BLD_NAME=${D4B_DEV} --build-arg PRINST_NAME=${PRINST_DEV} --build-arg BUILDREV=${BUILDREV}"
}

build_dev_server $1 $2

# Local Variables:
# vim: sw=2:et:tw=80:cc=+1
