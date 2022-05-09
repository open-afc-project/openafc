#!/bin/bash
# Build the RPM packages for CPO.
# Any arguments to this script are passed along to each of the "rpmbuild" commands.
set -e

N_THREADS=$(nproc --all --ignore=2)
ROOTDIR=$(readlink -f $(dirname "${BASH_SOURCE[0]}"))
CMAKE="cmake3 -G Ninja"
NINJA="ninja-build -j$N_THREADS"
RPMBUILD="rpmbuild -ba --without apidoc $@"
RPMLINT="rpmlint"

if [[ -d "/usr/include/boost169" ]]; then
 CMAKE="${CMAKE} -DBOOST_INCLUDEDIR=/usr/include/boost169 -DBOOST_LIBRARYDIR=/usr/lib64/boost169"
fi

BUILDDIR=${ROOTDIR}/build
mkdir -p $BUILDDIR
pushd $BUILDDIR

${CMAKE} ..
rm -rf dist
${NINJA} rpm-prep
# Run rpmbuild directly to get unbuffered output
${RPMBUILD} --define "_topdir ${PWD}/dist" dist/SPECS/*.spec

popd
cd / && ${RPMLINT} --file fbrat.rpmlintrc build/dist/SRPMS build/dist/RPMS
