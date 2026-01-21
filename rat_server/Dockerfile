#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM python:3.12-alpine3.21
# This variable should be set to used Python version (major.minor only)
ENV AFC_PYTHONVERSION=3.12

ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3=~${AFC_PYTHONVERSION} postfix

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv
# This explicitly set variable and its forceful injection in some places helps
# to alleviate various venv shortcomings
ENV PYTHONPATH=/venv/lib/python${AFC_PYTHONVERSION}/site-packages:/usr/lib/python${AFC_PYTHONVERSION}/site-packages:${PYTHONPATH}

# Configure pip and npm to use package registry proxies (optional)
ARG PIP_INDEX_URL
ARG NPM_REGISTRY
ENV PIP_INDEX_URL=${PIP_INDEX_URL}
ENV NPM_REGISTRY=${NPM_REGISTRY}

# Installing Python libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev libffi
COPY rat_server/requirements.txt /wd/
RUN apk add --update --no-cache python3-dev=~${AFC_PYTHONVERSION} build-base \
    expat expat-dev libffi-dev && \
    pip3 install --upgrade pip setuptools && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
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
RUN apk list -I && \
    apk add --update --no-cache build-base cmake ninja yarn \
        apr-util-dev python3-dev=~${AFC_PYTHONVERSION} && \
    (if [ -n "${NPM_REGISTRY}" ]; then \
        yarn config set registry ${NPM_REGISTRY} && \
        # Network timeout set to 10 minutes (600000ms) to handle slow corporate proxies
        # and large package downloads. Default is 60s which may be insufficient.
        yarn config set network-timeout 600000 && \
        # Limit concurrency to 1 to avoid overwhelming proxy servers
        yarn config set network-concurrency 1 && \
        echo "registry \"${NPM_REGISTRY}\"" > /wd/src/web/.yarnrc && \
        echo "network-timeout 600000" >> /wd/src/web/.yarnrc && \
        echo "network-concurrency 1" >> /wd/src/web/.yarnrc && \
        if [ -f /wd/src/web/yarn.lock ]; then \
            sed -i "s|https://registry\.yarnpkg\.com/|${NPM_REGISTRY}|g" /wd/src/web/yarn.lock; \
        fi; \
    fi) && \
    cd /wd/build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiWebDebug -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
    (if [ -n "${NPM_REGISTRY}" ] && [ -f /wd/build/src/web/pkg/yarn.lock ]; then \
        sed -i "s|https://registry\.yarnpkg\.com/|${NPM_REGISTRY}|g" /wd/build/src/web/pkg/yarn.lock; \
    fi) && \
    env PYTHONPATH=${PYTHONPATH} \
        NPM_REGISTRY=${NPM_REGISTRY} \
        ninja -j$(nproc) install && \
    yarn cache clean && \
    apk del --purge build-base cmake ninja yarn apr-util-dev python3-dev && \
    rm -rf /wd/cmake/ /wd/pkg/ /wd/src/ /wd/build

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
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

RUN mkdir /wd/private
COPY rat_server/copy-private.sh private* /wd/private/
RUN chmod +x /wd/private/copy-private.sh
RUN /wd/private/copy-private.sh
RUN rm -rf /wd/private

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
ENTRYPOINT ["/entrypoint.sh"]
HEALTHCHECK --start-interval=1m CMD wget --no-verbose --tries=1 --spider 127.0.0.1/fbrat/ratapi/v1/healthy || exit 1
#
