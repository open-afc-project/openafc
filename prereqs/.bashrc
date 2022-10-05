#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
# .bashrc

if [ -f /etc/bashrc ]; then
	. /etc/bashrc
fi

# User specific aliases and functions
EDITOR=vim
set -o vi

#
# AFC project
#
# Defines directory where cloned source code
export BRCM_OPENAFC_PRJ=/wd/afc/open-afc
# Defines directory where placed built objects and binaries
export BRCM_WORKINGDIR=/wd/afc/oafc_bld

alias afc_help='echo -e " \
	afc_wd - cd to workdir and prepare dev env\n \
	afc_cm - run cmake on sources\n \
	afc_bld - run ninja-build \n \
	afc_ins - run ninja-build install \n \
    BRCM_OPENAFC_PRJ = ${BRCM_OPENAFC_PRJ} \n \
    BRCM_WORKINGDIR = ${BRCM_WORKINGDIR} \n \
    N_THREADS = ${N_THREADS} \n \
    APIDOCDIR = ${APIDOCDIR} \n \
    LD_LIBRARY_PATH = ${LD_LIBRARY_PATH} \n \
"'
alias afc_wd='pushd ${BRCM_OPENAFC_PRJ} && \
source /opt/rh/devtoolset-11/enable && \
popd && \
pushd ${BRCM_WORKINGDIR} && \
export N_THREADS=$(nproc --all) && \
export APIDOCDIR=${BRCM_WORKINGDIR}/testroot/share/doc/fbrat-api && \
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${BRCM_WORKINGDIR}/testroot/lib64
'
alias afc_cm='cmake3 -DCMAKE_INSTALL_PREFIX=/usr \
             -DCMAKE_PREFIX_PATH=/usr \
             -DAPIDOC_INSTALL_PATH=${APIDOCDIR} \
             -DBOOST_INCLUDEDIR=/usr/include/boost169 \
             -DBOOST_LIBRARYDIR=/usr/lib64/boost169 \
             -DBUILD_WITH_COVERAGE=off \
             -G Ninja ${BRCM_OPENAFC_PRJ}'
alias afc_bld='ninja-build -j$N_THREADS'
alias afc_ins='ninja-build install -j$N_THREADS'

#GREP
alias g='grep'
alias gk='grep -r --exclude-dir={.git,.svn} --exclude={tags,*~,*.swp,*.o,*.ko,cscope.out}'
alias gkl='grep -rl --exclude-dir={.git,.svn} --exclude={tags,*~,*.swp,*.o,*.ko,cscope.out}'
#GIT
alias gb='git branch'
alias gba='git branch -a'
alias gc='git commit -v'
alias gd='git diff'
alias gl='git pull'
alias gp='git push'
alias gst='git status'
alias glo='git log --oneline'

alias rm='rm -i'
alias mv='mv -i'
alias cp='cp -i'
alias ll='ls -l'
alias lla='ls -la'
