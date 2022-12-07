#!/bin/sh
#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
	apk add build-base cmake samurai \
	boost-dev python3-dev qt5-qtbase-dev armadillo-dev minizip-dev
else
	# Size optimization
	apk del apk-tools libc-utils
#	rm -rf /usr/include /sbin/apk /etc/apk /usr/share/apk
fi

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
