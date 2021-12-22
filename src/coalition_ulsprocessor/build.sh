#!/bin/bash
# Script to build each project and install into ./testroot
set -e

ROOTDIR=$(readlink -f $(dirname "${BASH_SOURCE[0]}"))
BUILDTYPE="RelWithDebInfo"
CMAKE="cmake3 \
    -DCMAKE_BUILD_TYPE=${BUILDTYPE} \
    -DCMAKE_INSTALL_PREFIX=${ROOTDIR}/testroot \
    -DCMAKE_PREFIX_PATH=${ROOTDIR}/testroot \
    -G Ninja"
NINJA="ninja-build"

# CPOBG libraries
BUILDDIR=${ROOTDIR}/build
mkdir -p $BUILDDIR
pushd $BUILDDIR

${CMAKE} ..
${NINJA}
${NINJA} install

popd