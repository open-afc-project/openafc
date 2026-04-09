#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
# Pin Alpine 3.21: rolling python:3.12-alpine can pick newer CMake that rejects
# cmake_minimum_required() after project() in root CMakeLists.txt.
FROM python:3.12-alpine3.21@sha256:d4f9227f21409479c7fe92288f2e40b1a56ae591c8ad3b5446dfeaef90f67857
# This variable should be set to used Python version (major.minor only)
ENV AFC_PYTHONVERSION=3.12

ENV PYTHONUNBUFFERED=1
RUN apk upgrade --no-cache && \
    apk add --update --no-cache python3=~${AFC_PYTHONVERSION} postfix

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv
# This explicitly set variable and its forceful injection in some places helps
# to alleviate various venv shortcomings
ENV PYTHONPATH=/venv/lib/python${AFC_PYTHONVERSION}/site-packages:/usr/lib/python${AFC_PYTHONVERSION}/site-packages:${PYTHONPATH}

# Private PyPI mirror credential is passed via BuildKit secret
# (--secret id=pip_config,src=<pip.conf>) so it is never baked into image
# layers or build-arg history. Omit --secret to use public PyPI.
# Configure npm to use package registry proxy (optional)
ARG NPM_REGISTRY
ENV NPM_REGISTRY=${NPM_REGISTRY}

# Installing Python libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev libffi
COPY rat_server/requirements.txt /wd/
# NOTE: requirements.txt is fully ==-pinned but not yet hash-locked
# (--require-hashes, as objstorage/Dockerfile and cert_db/Dockerfile were
# hardened to do per S0127-14) because generating a correct hash-locked
# file for this dependency closure requires resolving the full transitive
# dependency graph against a package registry, which is not available in
# this build environment. Do not add --require-hashes without a properly
# generated (pip-compile --generate-hashes) lockfile covering every
# transitive dependency - a partially-hashed file would either fail the
# build or silently omit hash checks for the un-hashed remainder.
# The unconditional, unpinned pip/setuptools self-upgrade is dropped
# (mirrors the hardened objstorage/cert_db Dockerfiles, which install
# straight from the base image's bundled pip/setuptools) rather than left
# floating.
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    apk add --update --no-cache python3-dev=~${AFC_PYTHONVERSION} build-base \
    expat expat-dev libffi-dev && \
    pip3 install --no-cache-dir --require-hashes -r /wd/requirements.txt && \
    apk del --purge python3-dev build-base expat-dev libffi-dev && \
    rm -f /wd/requirements.txt

RUN mkdir -p /run/gunicorn /etc/xdg/fbrat
COPY gunicorn/wsgi.py /wd/
COPY config/ratapi.conf /etc/xdg/fbrat/
RUN echo "DEFAULT_ULS_DIR = '/mnt/nfs/rat_transfer/ULS_Database'" >> /etc/xdg/fbrat/ratapi.conf
RUN echo "AFC_APP_TYPE = 'server'" >> /etc/xdg/fbrat/ratapi.conf

#
# Build Message Handler application
#
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild
ARG NODE_OPTIONS=--openssl-legacy-provider

#
# Install Message Handler application
#
# NOTE: the NPM/registry auth token (if any) is provided via a BuildKit secret
# mount rather than COPY .npmrc, so the credential never persists in an image
# layer (a `rm` in a later RUN layer does not remove bytes committed by COPY).
# Build with:
#   DOCKER_BUILDKIT=1 docker build --secret id=npmrc,src=${HOME}/.npmrc ...
# The mount is optional (required=false): builds without a private registry
# simply have no /root/.npmrc.
RUN --mount=type=secret,id=npmrc,target=/root/.npmrc,required=false \
    apk list -I && \
    apk add --update --no-cache build-base cmake ninja yarn \
        apr-util-dev python3-dev=~${AFC_PYTHONVERSION} py3-setuptools && \
    yarn config set network-timeout 600000 && \
    yarn config set network-concurrency 1 && \
    yarn config set ignore-optional true && \
    yarn config set ignore-scripts true && \
    echo "network-timeout 600000" > /wd/src/web/.yarnrc && \
    echo "network-concurrency 1" >> /wd/src/web/.yarnrc && \
    echo "ignore-optional true" >> /wd/src/web/.yarnrc && \
    echo "ignore-scripts true" >> /wd/src/web/.yarnrc && \
    (if [ -n "${NPM_REGISTRY}" ]; then \
        yarn config set registry ${NPM_REGISTRY} && \
        echo "registry \"${NPM_REGISTRY}\"" >> /wd/src/web/.yarnrc && \
        if [ -f /wd/src/web/yarn.lock ]; then \
            sed -i -E "s|https://(registry\.yarnpkg\.com\|registry\.npmjs\.org)/|${NPM_REGISTRY}|g" /wd/src/web/yarn.lock; \
        fi; \
    fi) && \
    cd /wd/build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiWebDebug -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
    mkdir -p /wd/build/src/web/pkg && \
    cp -f /wd/src/web/.yarnrc /wd/build/src/web/pkg/.yarnrc && \
    (if [ -n "${NPM_REGISTRY}" ] && [ -f /wd/build/src/web/pkg/yarn.lock ]; then \
        sed -i -E "s|https://(registry\.yarnpkg\.com\|registry\.npmjs\.org)/|${NPM_REGISTRY}|g" /wd/build/src/web/pkg/yarn.lock; \
    fi) && \
    env PYTHONPATH=${PYTHONPATH} \
        NPM_REGISTRY=${NPM_REGISTRY} \
        ninja -j$(nproc) install && \
    yarn cache clean && \
    apk del --purge build-base cmake ninja yarn apr-util-dev python3-dev py3-setuptools && \
    rm -rf /wd/cmake/ /wd/pkg/ /wd/src/ /wd/build

COPY src/afc-packages /wd/afc-packages
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.rat_server \
    && rm -rf /wd/afc-packages
#
# Add user and group
RUN addgroup -g 1003 fbrat && \
adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat
RUN mkdir -p /var/spool/fbrat /var/lib/fbrat /mnt/nfs /var/lib/fbrat/daily_uls_parse
RUN chown fbrat:fbrat /var/spool/fbrat /usr/share/fbrat /var/lib/fbrat /var/lib/fbrat/daily_uls_parse
#
LABEL revision="afc-rat_server"
WORKDIR /wd
EXPOSE 80

# Private branding assets (customer-specific images/templates) are injected
# by copy-private.sh, which installs them into their final locations under
# /usr/share/fbrat and site-packages. Previously the raw private* sources
# were COPY'd into /wd/private (a committed layer) and only removed by a
# later `RUN rm -rf /wd/private`: a `rm` in a later RUN layer does not
# remove bytes committed by an earlier COPY (same pitfall documented above
# for .npmrc/pip.conf), so any principal with pull access to the image could
# `docker save` and recover the staged private* content from that
# intermediate layer. Bind-mount the sources for the duration of this single
# RUN instead: BuildKit bind mounts are never committed to an image layer,
# so the raw staging content never reaches the shipped image at all, and no
# compensating rm is needed.
RUN --mount=type=bind,source=private,target=/wd/private \
    --mount=type=bind,source=rat_server/copy-private.sh,target=/wd/copy-private.sh \
    sh /wd/copy-private.sh

# msghnd environment variables default values
ENV AFC_WEBUI_PORT=${AFC_WEBUI_PORT:-"80"}
ENV AFC_WEBUI_BIND=${AFC_WEBUI_BIND:-"0.0.0.0"}
ENV AFC_WEBUI_PID=${AFC_WEBUI_PID:-"/run/gunicorn/openafc_app.pid"}
ENV AFC_WEBUI_ACCESS_LOG=
ENV AFC_WEBUI_ERROR_LOG=${AFC_WEBUI_ERROR_LOG:-"/proc/self/fd/2"}
ENV AFC_WEBUI_TIMEOUT=${AFC_WEBUI_TIMEOUT:-180}
ENV AFC_WEBUI_WORKERS=${AFC_WEBUI_WORKERS:-5}
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}

# Comma separated list of vendor extensions from previously computed responses
# to be appended to AFC requests
# ENV AFC_STATE_VENDOR_EXTENSIONS

ENV XDG_DATA_DIRS=$XDG_DATA_DIRS:/usr/local/share:/usr/share:/usr/share/fbrat:/mnt/nfs
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY rat_server/devel.sh /wd/
RUN chmod +x /wd/devel.sh
RUN /wd/devel.sh
# TODO to rename following variable after source split
ENV FLASK_AFC_MSGHND_RATAFC_TOUT=${FLASK_AFC_MSGHND_RATAFC_TOUT:-600}
ENV AFC_RATAPI_LOG_LEVEL=${AFC_RATAPI_LOG_LEVEL:-WARNING}
COPY rat_server/entrypoint.sh /
RUN chmod +x /entrypoint.sh
# Drop to non-root fbrat after all setup steps that require
# root (chown, pip install, etc.) are complete.  rat_server has no host
# bind mounts that require root-level write access at runtime.
RUN chown -R fbrat:fbrat /run/gunicorn /etc/xdg/fbrat
USER fbrat
ENTRYPOINT ["/entrypoint.sh"]
HEALTHCHECK --start-interval=1m CMD wget --no-verbose --tries=1 --spider 127.0.0.1/fbrat/ratapi/v1/healthy || exit 1
#
