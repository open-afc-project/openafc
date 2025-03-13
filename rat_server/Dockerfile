#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM httpd:2.4.63-alpine3.21
# This variable should be set to default Python version for chosen Alpine
# version. Unfortunately there is no way to get it automagically
ENV PYTHONVERSION=3.12

ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3=~${PYTHONVERSION} postfix

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv
# This explicitly set variable and its forceful injection in some places helps
# to alleviate various venv shortcomings
ENV PYTHONPATH=/venv/lib/python${PYTHONVERSION}/site-packages:/usr/lib/python${PYTHONVERSION}/site-packages:${PYTHONPATH}

# Installing Python libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev libffi
COPY rat_server/requirements.txt /wd/
RUN apk add --update --no-cache python3-dev=~${PYTHONVERSION} build-base \
    expat expat-dev libffi-dev && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
    apk del --purge python3-dev build-base expat-dev libffi-dev && \
    rm -f /wd/requirements.txt

RUN mkdir -p /etc/xdg/fbrat
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

RUN apk list -I && \
    apk add --update --no-cache build-base cmake ninja yarn \
        apr-util-dev python3-dev=~${PYTHONVERSION} && \
    mod_wsgi-express install-module && \
    cd /wd/build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiWebDebug -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
    env PYTHONPATH=${PYTHONPATH} \
        ninja -j$(nproc) install && \
    yarn cache clean && \
    apk del --purge build-base cmake ninja yarn apr-util-dev python3-dev && \
    rm -rf /wd/cmake/ /wd/pkg/ /wd/src/ /wd/build
#
# Install Message Handler application
#
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
EXPOSE 80 443
COPY rat_server/httpd.conf /usr/local/apache2/conf/
COPY rat_server/httpd-default.conf rat_server/httpd-info.conf rat_server/httpd-vhosts.conf \
rat_server/httpd-mpm.conf rat_server/httpd-ssl.conf /usr/local/apache2/conf/extra/
COPY rat_server/fbrat.wsgi /usr/local/apache2/fbrat.wsgi
RUN mkdir /wd/private
COPY rat_server/copy-private.sh private* /wd/private/
RUN chmod +x /wd/private/copy-private.sh
RUN /wd/private/copy-private.sh
RUN rm -rf /wd/private

RUN mkdir -p /usr/share/ca-certificates/certs
COPY rat_server/http.key rat_server/http.crt /usr/share/ca-certificates/certs/
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
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider localhost/fbrat/ratapi/v1/healthy || exit 1
#
