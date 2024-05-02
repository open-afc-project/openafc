#
# Copyright (C) 2023 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns 
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM alpine:3.18

# True if enabled (default is True)
# ENV RCACHE_ENABLED=True

# Port REST API service listen on
# ENV RCACHE_CLIENT_PORT

# Connection string to Postgres DB
# ENV RCACHE_POSTGRES_DSN=postgresql://[user[:password]@]host[:port]/database[?options]

# File with password for Rcache database DSN
# ENV RCACHE_POSTGRES_PASSWORD_FILE

# What to do if database already exists:
#  - 'leave' (leave as is - default)
#  - 'recreate' (completely recreate - e.g. removing alembic, if any)
#  - 'clean' (only clean the cache table)
ENV RCACHE_IF_DB_EXISTS=leave

# Maximum number of simultaneous precomputation requests. Default is 10
ENV RCACHE_PRECOMPUTE_QUOTA=10

# REST API URL to send precompute requests to. No precomputation if absent
# ENV RCACHE_AFC_REQ_URL

# REST API URL for requesting list of active AFC Configs. Default max link
# distance (130km) is used for spatial invalidation if unspecified or not
# responding
# ENV RCACHE_RULESETS_URL

# REST API URL to use for requesting AFC Config by Ruleset ID. Default max link
# distance (130km) is used for spatial invalidation if unspecified or not
# responding
# ENV RCACHE_CONFIG_RETRIEVAL_URL

# Additional command line parameters for uvicorn
ENV RCACHE_UVICORN_PARAMS="--no-access-log --log-level info"

WORKDIR /wd

RUN apk add --update --no-cache python3=~3.11 py3-sqlalchemy=~1.4 py3-pip \
    py3-requests py3-pydantic=~1.10 py3-alembic py3-psycopg2 py3-greenlet \
    py3-aiohttp curl

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.rcache \
    && rm -rf /wd/afc-packages

COPY rcache/requirements.txt /wd
RUN pip3 install  --root-user-action=ignore -r /wd/requirements.txt
COPY rcache/rcache_app.py rcache/rcache_app.py rcache/rcache_db_async.py \
    rcache/rcache_service.py /wd/
COPY tools/rcache/rcache_tool.py /wd
RUN chmod a+x /wd/rcache_tool.py

ENTRYPOINT uvicorn rcache_app:app --host 0.0.0.0 --port ${RCACHE_CLIENT_PORT} \
    ${RCACHE_UVICORN_PARAMS}

HEALTHCHECK --start-period=20s \
    CMD curl -f http://localhost:${RCACHE_CLIENT_PORT}/healthcheck || exit 1