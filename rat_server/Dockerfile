#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM httpd:2.4.55-alpine as rat_server.preinstall
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 && ln -sf python3 /usr/bin/python && \
apk add --update --no-cache py3-six py3-numpy py3-cryptography py3-sqlalchemy \
py3-requests py3-flask py3-psycopg2 postfix && \
python3 -m ensurepip && \
pip3 install --no-cache --upgrade pip setuptools
COPY rat_server/requirements.txt /wd/
RUN pip3 install -r /wd/requirements.txt && mkdir -p /etc/xdg/fbrat
COPY config/ratapi.conf /etc/xdg/fbrat/
RUN echo "DEFAULT_ULS_DIR = '/mnt/nfs/rat_transfer/ULS_Database'" >> /etc/xdg/fbrat/ratapi.conf
RUN echo "AFC_APP_TYPE = 'server'" >> /etc/xdg/fbrat/ratapi.conf
#
# Build Message Handler application
#
FROM httpd:2.4.55-alpine as rat_server.build
COPY --from=rat_server.preinstall / /
# Development env
RUN apk add --update --no-cache build-base cmake ninja yarn \
    apr-util-dev python3-dev
RUN pip3 install --no-cache --upgrade mod_wsgi \
    && mod_wsgi-express install-module
#
#
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild
ARG NODE_OPTIONS=--openssl-legacy-provider
RUN cd /wd/build && \
cmake -DCMAKE_INSTALL_PREFIX=/wd/__install -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiWebDebug -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
ninja -j$(nproc) install
#
# Install Message Handler application
#
FROM httpd:2.4.55-alpine as install_image
COPY --from=rat_server.preinstall / /
COPY --from=rat_server.build /wd/__install /usr/
COPY --from=rat_server.build /usr/lib/python3.10/site-packages/mod_wsgi /usr/lib/python3.10/site-packages/mod_wsgi
#
COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
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
RUN mkdir -p /usr/share/ca-certificates/certs
COPY rat_server/http.key rat_server/http.crt /usr/share/ca-certificates/certs/
ENV XDG_DATA_DIRS=$XDG_DATA_DIRS:/usr/local/share:/usr/share:/usr/share/fbrat:/mnt/nfs
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY rat_server/devel.sh /wd/
RUN chmod +x /wd/devel.sh
RUN /wd/devel.sh
# TODO to rename following variable after source split
ENV AFC_MSGHND_RATAFC_TOUT=${AFC_MSGHND_RATAFC_TOUT:-600}
COPY rat_server/entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider localhost/fbrat/ratapi/v1/healthy || exit 1
#
