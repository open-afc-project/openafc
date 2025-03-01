#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM alpine:3.18.11
# This variable should be set to default Python version for chosen Alpine
# version. Unfortunately there is no way to get it automagically
ENV PYTHONVERSION=3.11

ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv
# This explicitly set variable and its forceful injection in some places helps
# to alleviate various venv shortcomings
ENV PYTHONPATH=/venv/lib/python${PYTHONVERSION}/site-packages:/usr/lib/python${PYTHONVERSION}/site-packages:${PYTHONPATH}

# Installing Python libraries
RUN apk add --update --no-cache librdkafka-dev=~2.1.1
COPY msghnd/requirements.txt /wd/
RUN apk add --update --no-cache python3-dev gcc musl-dev && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
    apk del --purge python3-dev gcc musl-dev && rm -f /wd/requirements.txt

RUN mkdir -p /run/gunicorn /etc/xdg/fbrat
COPY gunicorn/wsgi.py /wd/
COPY config/ratapi.conf /etc/xdg/fbrat/
RUN echo "AFC_APP_TYPE = 'msghnd'" >> /etc/xdg/fbrat/ratapi.conf

COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild
RUN cd /wd/build && \
apk add --update --no-cache cmake ninja && \
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
env PYTHONPATH=${PYTHONPATH} \
ninja -j$(nproc) install && \
apk del --purge cmake ninja && rm -rf /wd/cmake/ /wd/pkg/ /wd/src/ /wd/build/

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.msghnd \
    && rm -rf /wd/afc-packages
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

# Prometheus stuff
COPY gunicorn/config.py /wd/gunicorn_config.py
# Directory for Prometheus's multiprocess housekeeping
ENV PROMETHEUS_MULTIPROC_DIR=/wd/prometheus_multiproc_dir
RUN mkdir -p $PROMETHEUS_MULTIPROC_DIR

# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY msghnd/devel.sh /wd/
RUN chmod +x /wd/devel.sh
RUN /wd/devel.sh
# msghnd environment variables default values
ENV AFC_MSGHND_PORT=${AFC_MSGHND_PORT:-"8000"}
ENV AFC_MSGHND_BIND=${AFC_MSGHND_BIND:-"0.0.0.0"}
ENV AFC_MSGHND_PID=${AFC_MSGHND_PID:-"/run/gunicorn/openafc_app.pid"}
ENV AFC_MSGHND_ACCESS_LOG=
ENV AFC_MSGHND_ERROR_LOG=${AFC_MSGHND_ERROR_LOG:-"/proc/self/fd/2"}
ENV AFC_MSGHND_TIMEOUT=${AFC_MSGHND_TIMEOUT:-180}
ENV AFC_MSGHND_WORKERS=${AFC_MSGHND_WORKERS:-20}
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
ENV FLASK_AFC_MSGHND_RATAFC_TOUT=${FLASK_AFC_MSGHND_RATAFC_TOUT:-600}
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider \
    http://127.0.0.1:${AFC_MSGHND_PORT}/fbrat/ap-afc/healthy || exit 1
