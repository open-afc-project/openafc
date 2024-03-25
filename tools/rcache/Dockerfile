#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns 
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM alpine:3.18

# Default Rcache service port
ENV RCACHE_CLIENT_PORT=8000

# Default Rcache service root URL
ENV RCACHE_SERVICE_URL=http://rcache:${RCACHE_CLIENT_PORT}

# Default Rcache database connection string
ENV RCACHE_POSTGRES_DSN=postgresql://postgres:postgres@bulk_postgres/rcache

WORKDIR /wd

RUN apk add --update --no-cache python3=~3.11 py3-sqlalchemy=~1.4 py3-pip \
    py3-requests py3-pydantic=~1.10 py3-psycopg2 py3-greenlet py3-aiohttp curl

COPY tools/rcache/requirements.txt /wd
RUN pip3 install  --root-user-action=ignore -r /wd/requirements.txt

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.rcache \
    && rm -rf /wd/afc-packages

COPY tools/rcache/rcache_tool.py /wd
COPY tools/load_tool/afc_load_tool.* /wd/
RUN chmod a+x /wd/rcache_tool.py /wd/afc_load_tool.py

CMD sleep infinity