#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# default value of args
ARG BLD_TAG=3.4.7.1
ARG PRINST_TAG=3.4.7.1
ARG BLD_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-build-image
ARG PRINST_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-worker-preinstall

# Stage Build
FROM ${BLD_NAME}:${BLD_TAG} as build_image
ARG BUILDREV=localbuild
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
RUN echo $BUILDREV > /wd/svnrevision.txt

COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY selinux /wd/selinux/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build/__install
RUN cd /wd/build && \
source /opt/rh/$(scl -l)/enable && \
cmake3 -DCMAKE_INSTALL_PREFIX=/wd/build/__install -DCMAKE_PREFIX_PATH=/usr -DBOOST_INCLUDEDIR=/usr/include/boost169 -DBOOST_LIBRARYDIR=/usr/lib64/boost169 -DBUILD_WITH_COVERAGE=off -DCMAKE_BUILD_TYPE=EngineRatapiRelease -G Ninja /wd && \
ninja-build -j$(nproc) install


# Stage Install
FROM ${PRINST_NAME}:${PRINST_TAG} as install_image

COPY worker/devel.sh /wd/
COPY prereqs/.bashrc /wd/
ARG AFC_DEVEL_ENV=production
ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
RUN /wd/devel.sh
RUN rm -f /wd/devel.sh /wd/.bashrc

COPY --from=build_image /wd/build/__install /usr

RUN mkdir -m 750 -p /var/lib/fbrat/AntennaPatterns
RUN mkdir -m 755 -p /var/spool/fbrat
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/celery
RUN mkdir -m 755 -p /var/run/celery
RUN mkdir -m 755 -p /var/log/celery

RUN chown -R fbrat:fbrat /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/celery

COPY worker/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh

FROM centos:7
COPY --from=install_image / /

LABEL revision="afc-worker"
WORKDIR /wd
EXPOSE 80 443
ENV PGPORT=5432
ARG CELERY_OPTIONS
ENV CELERY_OPTIONS=$CELERY_OPTIONS
ARG CELERY_LOG
ENV CELERY_LOG=$CELERY_LOG
ARG HTTPD_OPTIONS
ENV HTTPD_OPTIONS=$HTTPD_OPTIONS
CMD ["/docker-entrypoint.sh"]
