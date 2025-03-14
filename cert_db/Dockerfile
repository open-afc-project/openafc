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

# CMake installs packages to /usr/lib even though this image uses /usr/local/lib
ENV PYTHONPATH=/usr/lib/python${AFC_PYTHONVERSION}/site-packages:/usr/local/lib/python${AFC_PYTHONVERSION}/site-packages:${PYTHONPATH}

# Installing Python libraries
#   Version of librdkafka-dev depends on Alpine version. confluent-kafka
#   version in requirements.txt may need to be updated on Alpine version change
RUN apk add --update --no-cache librdkafka-dev
COPY cert_db/requirements.txt /wd/
RUN apk add --update --no-cache python3-dev=~${AFC_PYTHONVERSION} build-base && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
    apk del --purge python3-dev build-base && rm -f /wd/requirements.txt

RUN mkdir -p /etc/xdg/fbrat
COPY config/ratapi.conf /etc/xdg/fbrat/
RUN echo "AFC_APP_TYPE = 'cert_db'" >> /etc/xdg/fbrat/ratapi.conf
#
# Build Message Handler application
#

# Development env
#
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild

RUN apk add --update --no-cache cmake ninja && \
cd /wd/build && \
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
env PYTHONPATH=${PYTHONPATH} \
ninja -j$(nproc) install && \
apk del --purge cmake ninja && rm -rf /wd/cmake/ /wd/pkg/ /wd/src/ /wd/build

#
# Install FCCID_DB application
#
COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.cert_db \
    && rm -rf /wd/afc-packages
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/spool/fbrat 

# Add user and group
RUN addgroup -g 1003 fbrat && \
adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat && \
chown fbrat:fbrat /var/lib/fbrat
#
# Prevent Rcache client initialization (fails on parameter checking)
ENV RCACHE_ENABLED=False
# Space-separated list of regions to sweep
ENV SWEEP_REGIONS="US CA"
# Sweep periodicity in /etc/periodic units (15min,hourly,daily,weekly,monthly)
ENV SWEEP_PERIOD=daily
# REST request timeout in seconds
ENV REQUEST_TIMEOUT_SEC=500
# Make all environment variables available to cronjobs
RUN env >> /etc/environment

LABEL revision="afc-cert_db"
WORKDIR /wd
COPY cert_db/entrypoint.sh /
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY cert_db/devel.sh cert_db/sweep.sh /wd/
RUN chmod +x /wd/devel.sh /wd/sweep.sh
RUN /wd/devel.sh
#
RUN mkdir -p /etc/periodic/$SWEEP_PERIOD
RUN ln -s /wd/sweep.sh /etc/periodic/$SWEEP_PERIOD/sweep.sh
RUN chmod 744 /etc/periodic/$SWEEP_PERIOD/sweep.sh
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]

HEALTHCHECK --start-period=30m CMD test `find /wd/touchfile -mtime -3`
