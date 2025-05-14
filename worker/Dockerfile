#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

FROM python:3.12-alpine3.21
# This variable should be set to used Python version (major.minor only)
ENV AFC_PYTHONVERSION=3.12

LABEL revision="afc-worker-al"
ARG AFC_WORKER_USER=${AFC_WORKER_USER:-root}
ENV AFC_WORKER_USER=${AFC_WORKER_USER}
ENV AFC_WORKER_CELERY_WORKERS=${AFC_WORKER_CELERY_WORKERS:-"rat_1"}
ENV AFC_WORKER_CELERY_LOG=${AFC_WORKER_CELERY_LOG:-WARNING}
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
ARG AFC_WORKER_UID=${AFC_WORKER_UID}

# True to update Rcache by AFC Response sender (Worker), False to update by
# AFC REsponse receiver (MsgHandler, RatAPI)
ENV RCACHE_UPDATE_ON_SEND=True

RUN apk add --update --no-cache \
    boost1.84-program_options boost1.84-log boost1.84-filesystem \
    boost1.84-thread qt5-qtbase armadillo minizip libgeotiff \
    qt5-qtbase-sqlite libbsd gdal tzdata imagemagick zip

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv

# Installing Python libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev
COPY worker/requirements.txt /wd/
RUN apk add --update --no-cache python3-dev=~${AFC_PYTHONVERSION} build-base expat expat-dev && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
    apk del --purge python3-dev build-base expat-dev && \
    rm -f /wd/requirements.txt

# Building AFC Engine
ARG BUILDREV=localbuild
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /build/afc/
COPY cmake /build/afc/cmake/
COPY pkg /build/afc/pkg/
COPY selinux /build/afc/selinux/
COPY src /build/afc/src/

RUN mkdir -p -m 777 /build/afc/build
ARG BUILDREV=localbuild

RUN cd /build/afc/build && \
    apk add --update --no-cache build-base cmake samurai boost-dev \
        qt5-qtbase-dev armadillo-dev minizip-dev libbsd-dev gdal-dev && \
    cmake -DCMAKE_INSTALL_PREFIX=/build/afc/__install -DCMAKE_PREFIX_PATH=/usr -DBUILD_WITH_COVERAGE=off -DCMAKE_BUILD_TYPE=EngineRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /build/afc && \
    env PYTHONPATH=${PYTHONPATH} \
        ninja -j$(nproc) install && \
    cd /build/afc/src/afc-engine-preload && make && \
    cp libaep.so /build/afc/__install/lib/ && \
    cp parse_fs.py /build/afc/__install/bin/ && \
    strip --strip-unneeded /build/afc/__install/bin/afc-engine && \
    strip --strip-unneeded /build/afc/__install/lib/*.so && \
    cp -r /build/afc/__install/* /usr && \
    apk del --purge build-base cmake samurai boost-dev \
        qt5-qtbase-dev armadillo-dev minizip-dev libbsd-dev gdal-dev && \
    rm -rf /build


COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.worker \
    && rm -rf /wd/afc-packages

RUN addgroup -g 1003 fbrat
RUN adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat

RUN mkdir -m 755 -p /var/spool/fbrat
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/celery
RUN mkdir -m 755 -p /var/run/celery
RUN mkdir -m 755 -p /var/log/celery
RUN chown -R fbrat:fbrat /var/spool/fbrat /var/lib/fbrat /var/celery

COPY worker/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh
COPY worker/afc-engine.sh /usr/bin/
RUN chmod +x /usr/bin/afc-engine.sh

# Development env
COPY worker/devel.sh worker/ddd.tar.gz /wd/
ARG AFC_DEVEL_ENV=production
ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
# set alternative user
ARG AFC_WORKER_USER=${AFC_WORKER_USER:-root}
RUN /wd/devel.sh
RUN rm -f /wd/devel.sh /wd/ddd.tar.gz

WORKDIR /wd
ENV PGPORT=5432
ENV XDG_DATA_DIRS=$XDG_DATA_DIRS:/usr/local/share:/usr/share:/usr/share/fbrat:/mnt/nfs
CMD ["/docker-entrypoint.sh"]
HEALTHCHECK --interval=20s --timeout=3s --retries=3 \
    CMD for celery_worker in ${AFC_WORKER_CELERY_WORKERS} ; do \
    celery -A afc_worker status -d $celery_worker@$(hostname) || exit 1 ; done
