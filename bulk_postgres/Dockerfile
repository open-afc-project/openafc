#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# Dockerfile for PostgreSQL+PostGIS server used for ALS log storage

FROM postgis/postgis:14-3.5@sha256:0bcd52e7ce93747a21f5cdd34e07f33f272052ed0f80ec4e70afba07886ac660

COPY postgis-upgrade.sh /usr/local/bin/postgis-upgrade.sh
RUN chmod +x /usr/local/bin/postgis-upgrade.sh

# Init script creates bulk_rw (non-superuser rw) and bulk_ro
# (SELECT-only) roles at first-boot so application services no longer share
# the postgres superuser credential.
COPY initdb.d/01_create_bulk_roles.sh \
     /docker-entrypoint-initdb.d/01_create_bulk_roles.sh
RUN chmod +x /docker-entrypoint-initdb.d/01_create_bulk_roles.sh

# POSTGRES_PASSWORD is NOT set here; it must be injected at runtime via the
# POSTGRES_PASSWORD_FILE environment variable pointing to the
# BULK_POSTGRES_PASSWORD Docker secret (set in docker-compose.yaml).
# Per-service roles (bulk_rw / bulk_ro) are created by the init script above.
ENV AFC_BULKDB_CONNS=${AFC_BULKDB_CONNS:-1000}
ENTRYPOINT docker-entrypoint.sh postgres -c max_connections=$AFC_BULKDB_CONNS
HEALTHCHECK --start-period=20s --interval=10s --timeout=5s \
    CMD pg_isready -U postgres || exit 1
