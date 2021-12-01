#!/bin/sh
N_THREADS=$(nproc --all --ignore=2)
mkdir -p ${FBRAT_WORKING_DIR}/${AFC_SUFX}/build
cd ${FBRAT_WORKING_DIR}/${AFC_SUFX}/build
cmake3 -DCMAKE_INSTALL_PREFIX=${FBRAT_WORKING_DIR}/${AFC_SUFX}/build/testroot \
    -DCMAKE_PREFIX_PATH=${FBRAT_WORKING_DIR}/${AFC_SUFX}/build/testroot -DAPIDOC_INSTALL_PATH=${FBRAT_WORKING_DIR}/${AFC_SUFX}/build/testroot/share/doc/fbrat-api \
    -DBOOST_INCLUDEDIR=/usr/include/boost169 -DBOOST_LIBRARYDIR=/usr/lib64/boost169 \
    -DBUILD_WITH_COVERAGE=on -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja ..
cd ${FBRAT_WORKING_DIR}/${AFC_SUFX}/build
ninja-build -j$N_THREADS
ninja-build install -j$N_THREADS
