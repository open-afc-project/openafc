#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM alpine:3.21.3
# This variable should be set to default Python version for chosen Alpine
# version. Unfortunately there is no way to get it automagically
ENV PYTHONVERSION=3.12

RUN apk add --update --no-cache python3=~${PYTHONVERSION}

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv
# This explicitly set variable and its forceful injection in some places helps
# to alleviate various venv shortcomings
ENV PYTHONPATH=/venv/lib/python${PYTHONVERSION}/site-packages:/usr/lib/python${PYTHONVERSION}/site-packages:${PYTHONPATH}

RUN apk add --update --no-cache gcc python3-dev=~${PYTHONVERSION} libc-dev

COPY objstorage/requirements.txt /wd/
RUN pip3 install --no-cache-dir -r /wd/requirements.txt && \
    rm -f /wd/requirements.txt

COPY objstorage/docker-entrypoint.sh /wd/
RUN chmod +x /wd/docker-entrypoint.sh

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.objstorage \
    && rm -rf /wd/afc-packages

ENV AFC_OBJST_PORT=${AFC_OBJST_PORT:-"5000"}
ENV AFC_OBJST_WORKERS=${AFC_OBJST_WORKERS:-10}
ENV AFC_OBJST_HIST_PORT=${AFC_OBJST_HIST_PORT:-"4999"}
ENV AFC_OBJST_HIST_WORKERS=${AFC_OBJST_HIST_WORKERS:-2}
CMD ["/wd/docker-entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider \
    http://127.0.0.1:${AFC_OBJST_PORT}/objst_healthy || exit 1
