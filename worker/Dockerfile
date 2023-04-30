#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# default value of args
ARG BLD_TAG=3.7.7.0
ARG PRINST_TAG=3.7.7.0
ARG BLD_NAME=public.ecr.aws/w9v6y1o0/openafc/worker-al-build-image
ARG PRINST_NAME=public.ecr.aws/w9v6y1o0/openafc/worker-al-preinstall

# Stage Build
FROM ${BLD_NAME}:${BLD_TAG} as build_image

ARG BUILDREV=localbuild
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /root/afc/
COPY cmake /root/afc/cmake/
COPY pkg /root/afc/pkg/
COPY selinux /root/afc/selinux/
COPY src /root/afc/src/

RUN mkdir -p -m 777 /root/afc/build
ARG BUILDREV=localbuild
RUN cd /root/afc/build && \
cmake -DCMAKE_INSTALL_PREFIX=/root/afc/__install -DCMAKE_PREFIX_PATH=/usr -DBUILD_WITH_COVERAGE=off -DCMAKE_BUILD_TYPE=EngineRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /root/afc && \
ninja -j$(nproc) install
RUN cd /root/afc/src/afc-engine-preload && make && cp aep.so /root/afc/__install/lib/ && cp parse_fs.py /root/afc/__install/bin/
RUN strip --strip-unneeded /root/afc/__install/bin/afc-engine
RUN strip --strip-unneeded /root/afc/__install/lib/*.so
#
# Stage Install
#
FROM ${PRINST_NAME}:${PRINST_TAG} as install_image

COPY --from=build_image /root/afc/__install /usr/

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.worker \
    && rm -rf /wd/afc-packages

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
COPY worker/devel.sh /wd/
ARG AFC_DEVEL_ENV=production
ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
RUN /wd/devel.sh
RUN rm -f /wd/devel.sh

FROM alpine:3.17
COPY --from=install_image / /

# set alternative user
ARG AFC_WORKER_USER=${AFC_WORKER_USER:-root}
USER ${AFC_WORKER_USER}

LABEL revision="afc-worker-al"
WORKDIR /wd
ENV PGPORT=5432
ENV XDG_DATA_DIRS=$XDG_DATA_DIRS:/usr/local/share:/usr/share:/usr/share/fbrat:/mnt/nfs
CMD ["/docker-entrypoint.sh"]
