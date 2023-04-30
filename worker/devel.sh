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
        bash gdb musl-dbg musl-dev strace
        adduser userafc -h /home/userafc -s /bin/bash -D
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
