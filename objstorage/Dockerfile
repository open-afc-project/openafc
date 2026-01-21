#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-alpine3.21
# This variable should be set to used Python version (major.minor only)
ENV AFC_PYTHONVERSION=3.12


# Configure pip to use package registry proxy (optional)
ARG PIP_INDEX_URL
ENV PIP_INDEX_URL=${PIP_INDEX_URL}

COPY objstorage/requirements.txt /wd/
RUN apk add --update --no-cache gcc python3-dev libc-dev && \
    pip3 install --no-cache-dir -r /wd/requirements.txt && \
    apk del --purge gcc python3-dev libc-dev && \
    rm -f /wd/requirements.txt

COPY objstorage/docker-entrypoint.sh /wd/
RUN chmod +x /wd/docker-entrypoint.sh

COPY src/afc-packages /wd/afc-packages
RUN apk add --update --no-cache gcc python3-dev libc-dev && \
    pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.objstorage && \
    apk del --purge gcc python3-dev libc-dev && \
    rm -rf /wd/afc-packages

ENV AFC_OBJST_PORT=${AFC_OBJST_PORT:-"5000"}
ENV AFC_OBJST_WORKERS=${AFC_OBJST_WORKERS:-10}
ENV AFC_OBJST_HIST_PORT=${AFC_OBJST_HIST_PORT:-"4999"}
ENV AFC_OBJST_HIST_WORKERS=${AFC_OBJST_HIST_WORKERS:-2}
CMD ["/wd/docker-entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider \
    http://127.0.0.1:${AFC_OBJST_PORT}/objst_healthy || exit 1
