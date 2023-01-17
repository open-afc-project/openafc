#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM alpine:3.17 as msghnd.preinstall
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 && ln -sf python3 /usr/bin/python && \
apk add --update --no-cache py3-six py3-numpy py3-cryptography py3-sqlalchemy \
py3-requests py3-flask py3-psycopg2 && python3 -m ensurepip && \
pip3 install --no-cache --upgrade pip setuptools
COPY msghnd/requirements.txt /wd/
RUN pip3 install -r /wd/requirements.txt && mkdir -p /run/gunicorn /etc/xdg/fbrat
COPY gunicorn/wsgi.py gunicorn/gunicorn.conf.py gunicorn/gunicorn.logs.conf /wd/
COPY prereqs/ratapi.conf /etc/xdg/fbrat/
RUN echo "AFC_APP_TYPE = 'msghnd'" >> /etc/xdg/fbrat/ratapi.conf
#
# Build Message Handler application
#
FROM alpine:3.17 as msghnd.build
COPY --from=msghnd.preinstall / /
# Development env
RUN apk add --update --no-cache cmake ninja
#
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild
RUN cd /wd/build && \
cmake -DCMAKE_INSTALL_PREFIX=/wd/__install -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
ninja -j$(nproc) install
#
# Install Message Handler application
#
FROM alpine:3.17 as msghnd.install
COPY --from=msghnd.preinstall / /
COPY --from=msghnd.build /wd/__install /usr/
#
RUN mkdir -m 750 -p /var/lib/fbrat/AntennaPatterns && \
mkdir -m 755 -p /var/spool/fbrat /var/lib/fbrat /var/celery /var/run/celery /var/log/celery
# Add user and group
RUN addgroup -g 1003 fbrat && \
adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat && \
chown fbrat:fbrat /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/celery
#
LABEL revision="afc-msghnd"
WORKDIR /wd
EXPOSE 8000
COPY msghnd/entrypoint.sh /
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY msghnd/devel.sh /wd/
RUN chmod +x /wd/devel.sh
RUN /wd/devel.sh
#
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
