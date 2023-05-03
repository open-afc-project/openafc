#!/bin/sh
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
    apk add build-base cmake samurai gdal-dev \
    boost-dev qt5-qtbase-dev armadillo-dev minizip-dev libbsd-dev \
    bash gdb musl-dbg musl-dev strace sudo \
    libxt-dev motif-dev xterm vim
    # prebuilt ddd executable installed.
    # steps to rebuild ddd binary from a stable release
    # tar zxf ddd-3.3.12.tar.gz
    # cd ddd-3.3.12
    # ./configure CXXFLAGS=-fpermissive
    # make
    # make install
    #
    cd /usr/local
    tar xvfpz /wd/ddd.tar.gz
    if [ x"$AFC_WORKER_USER" != x"root" ]; then
        set_uid=""
        if [ x"$AFC_WORKER_UID" != x"" ]; then
            set_uid="-u $AFC_WORKER_UID"
        fi
        adduser $set_uid $AFC_WORKER_USER -G fbrat -h /home/$AFC_WORKER_USER -s /bin/bash -D
        echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel
        addgroup $AFC_WORKER_USER wheel
    fi
else
    apk del apk-tools libc-utils py3-pip
fi

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
