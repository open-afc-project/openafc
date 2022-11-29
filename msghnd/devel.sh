#!/bin/sh
#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

if [ "$AFC_DEVEL_ENV" == "devel" ]; then
    echo "Install development env"
    yum -y install epel-release
    yum -y upgrade
    cd /wd/prereqs/repo/
    yum -y install lcov ninja-build minizip-devel boost169-devel qt5-qtbase-devel python-nose checkpolicy
    curl --silent --location https://dl.yarnpkg.com/rpm/yarn.repo | tee /etc/yum.repos.d/yarn.repo
    rpm --import https://dl.yarnpkg.com/rpm/pubkey.gpg
    yum -y install cmake3 ninja-build minizip-devel yarn subversion
    yum -y install centos-release-scl
    yum -y install devtoolset-11-toolchain
    rpm -e --nodeps gcc
fi

# Local Variables:
# mode: Python
# indent-tabs-mode: nil
# python-indent: 4
# End:
#
# vim: sw=4:et:tw=80:cc=+1
