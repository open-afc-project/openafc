#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# Dockerfile for PostgreSQL+PostGIS server used for ALS log storage

FROM postgis/postgis:14-3.3

ENV POSTGRES_PASSWORD=postgres
ENV POSTGRES_HOST_AUTH_METHOD=trust
ENV AFC_BULKDB_CONNS=${AFC_BULKDB_CONNS:-1000}
ENTRYPOINT docker-entrypoint.sh postgres -c max_connections=$AFC_BULKDB_CONNS
HEALTHCHECK --start-period=20s --interval=10s --timeout=5s \
    CMD pg_isready -U postgres || exit 1
