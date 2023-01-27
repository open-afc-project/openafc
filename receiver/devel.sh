#!/bin/sh
#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
AFC_DEVEL_ENV=${AFC_DEVEL_ENV:=production}
case "$AFC_DEVEL_ENV" in
  "devel")
    echo "Debug profile" 
    export NODE_OPTIONS='--openssl-legacy-provider'
    apk add --update --no-cache cmake ninja yarn bash
    echo -e 'pushd ./afc/open-afc && export GXK_OPENAFC_PROJ=`pwd` && \
popd && \
pushd ./afc/oafc_bld && export WORKINGDIR=`pwd` && \
export N_THREADS=$(nproc --all) && \
export APIDOCDIR=${WORKINGDIR}/testroot/share/doc/fbrat-api && \
export LD_LIBRARY_PATH=${WORKINGDIR}/testroot/lib64 && \
export NODE_OPTIONS=--openssl-legacy-provider && \
ninja -j $N_THREADS && \
ninja install
' > /usr/local/bin/afc_rcv_all.sh
    chmod +x /usr/local/bin/afc_rcv_all.sh
    ;;
  "production")
    echo "Production profile"
    ;;
  *)
    echo "Uknown profile"
    ;;
esac

exit $?
