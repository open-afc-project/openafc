#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# default value of args
ARG BLD_TAG=3.4.6.1
ARG PRINST_TAG=3.4.6.1
ARG BLD_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-build-image
ARG PRINST_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-msghnd-preinstall

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
cmake3 -DCMAKE_INSTALL_PREFIX=/wd/build/__install -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiRelease -G Ninja /wd && \
ninja-build -j$(nproc) install

# Stage Install
FROM ${PRINST_NAME}:${PRINST_TAG} as install_image
COPY --from=build_image /wd/build/__install /usr

RUN mkdir -m 750 -p /var/lib/fbrat/AntennaPatterns
RUN mkdir -m 755 -p /var/spool/fbrat
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/celery
RUN mkdir -m 755 /var/run/celery
RUN mkdir -m 755 /var/log/celery

RUN chown fbrat:fbrat /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/celery

COPY msghnd/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh

FROM centos:7
COPY --from=install_image / /

LABEL revision="afc-msghnd"
WORKDIR /wd
EXPOSE 8000
ENV PGPORT=5432
COPY msghnd/requirements.txt /wd/
RUN pip install -r /wd/requirements.txt
COPY gunicorn/wsgi.py /wd/
COPY gunicorn/gunicorn.conf.py /wd/
COPY gunicorn/gunicorn.logs.conf /wd/
RUN mkdir -p /run/gunicorn /etc/xdg/fbrat
COPY prereqs/ratapi.conf /etc/xdg/fbrat/
# optional development environment
# included if AFC_DEVEL_ENV is devel
COPY msghnd/devel.sh /wd/
COPY prereqs/.bashrc /root/
ARG AFC_DEVEL_ENV=production
ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
RUN /wd/devel.sh
CMD ["/docker-entrypoint.sh"]
