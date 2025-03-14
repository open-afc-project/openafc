#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-alpine3.21

# Default Rcache service port
ENV RCACHE_CLIENT_PORT=8000

# Default Rcache service root URL
ENV RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}

# Default Rcache database connection string
ENV RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache

# Rcache database password file name
# ENV RCACHE_POSTGRES_PASSWORD_FILE

WORKDIR /wd

RUN apk add --update --no-cache curl

COPY tools/rcache/requirements.txt /wd
RUN pip3 install --root-user-action=ignore -r /wd/requirements.txt

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
    --no-cache-dir -r /wd/afc-packages/pkgs.rcache \
    && rm -rf /wd/afc-packages

COPY tools/rcache/rcache_tool.py /wd
COPY tools/load_tool/afc_load_tool.* /wd/
RUN chmod a+x /wd/rcache_tool.py /wd/afc_load_tool.py

CMD sleep infinity