#!/bin/sh
#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
	apk add build-base cmake samurai gdal-dev bash gdb xterm vim \
	boost-dev qt5-qtbase-dev armadillo-dev minizip-dev libbsd-dev  \
    libxt-dev motif-dev

    cd /wd
    tar xvfpz ddd-3.3.12.tar.gz
    cd ddd-3.3.12
    ./configure CXXFLAGS=-fpermissive
    make
    make install
    cd /wd
    rm -rf ddd-3.3.12

    addgroup -g 500 mmandell
    adduser -u 1000 -G mmandell -H -D mmandell -s /bin/bash
    addgroup fbrat mmandell
    printf "asdf\nasdf\n" | passwd root
    printf "asdf\nasdf\n" | passwd mmandell
    chmod u+s /bin/su
else
	apk del apk-tools libc-utils py3-pip
#	rm -rf /usr/include /sbin/apk /etc/apk /usr/share/apk
fi

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
