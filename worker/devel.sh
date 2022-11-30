#!/bin/sh
#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
	mkdir -p /wd/repo/
	cd /wd/repo/
	curl --silent --location https://dl.yarnpkg.com/rpm/yarn.repo | tee /etc/yum.repos.d/yarn.repo
	rpm --import https://dl.yarnpkg.com/rpm/pubkey.gpg
	yum -y install epel-release centos-release-scl
	yum -y install cmake3 ninja-build minizip-devel yarn subversion devtoolset-11-toolchain qt5-qtbase-devel armadillo-devel gdal-devel
	rm -rf /wd/repo
	cp /wd/.bashrc /root/
fi

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
