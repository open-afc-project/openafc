#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program

# default value of args
ARG BLD_TAG=3.4.7.1
ARG PRINST_TAG=3.4.7.1
ARG BLD_NAME=public.ecr.aws/w9v6y1o0/openafc/worker-al-build-image
ARG PRINST_NAME=public.ecr.aws/w9v6y1o0/openafc/worker-al-preinstall

# Stage Build
FROM ${BLD_NAME}:${BLD_TAG} as build_image

ARG BUILDREV=localbuild
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /root/afc/
RUN echo $BUILDREV > /root/afc/svnrevision.txt
COPY cmake /root/afc/cmake/
COPY pkg /root/afc/pkg/
COPY selinux /root/afc/selinux/
COPY src /root/afc/src/

RUN mkdir -p -m 777 /root/afc/build
RUN cd /root/afc/build && \
cmake -DCMAKE_INSTALL_PREFIX=/root/afc/__install -DCMAKE_PREFIX_PATH=/usr -DBUILD_WITH_COVERAGE=off -DCMAKE_BUILD_TYPE=EngineRatapiRelease -G Ninja /root/afc && \
ninja -j$(nproc) install


# Stage Install
FROM ${PRINST_NAME}:${PRINST_TAG} as install_image

# Development env if ordered
#COPY worker/devel.sh /root/
#ARG AFC_DEVEL_ENV=production
#ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
#RUN /root/devel.sh
#RUN rm -f /root/devel.sh

COPY --from=build_image /root/afc/__install /usr/
COPY --from=build_image /root/gdal/__install /usr/

RUN mkdir -m 750 -p /var/lib/fbrat/AntennaPatterns
RUN mkdir -m 755 -p /var/spool/fbrat
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/celery
RUN mkdir -m 755 -p /var/run/celery
RUN mkdir -m 755 -p /var/log/celery
RUN chown -R fbrat:fbrat /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/celery

COPY worker/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh

# Size optimization

# Schweineoptimierung
RUN rm -rf /usr/include /usr/bin/ogr* /usr/bin/gdal*


FROM alpine:3.17
COPY --from=install_image / /

LABEL revision="afc-worker-al"
WORKDIR /wd
EXPOSE 80 443
ENV PGPORT=5432
CMD ["/docker-entrypoint.sh"]
