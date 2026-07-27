#!/usr/bin/env bash
# Create least-privilege application roles for bulk_postgres so
# application services no longer run as the PostgreSQL superuser.
#
# bulk_rw  -- Non-superuser LOGIN role used by als_siphon, rcache,
#             uls_downloader, and Grafana's own database.  Password is read
#             from the BULK_POSTGRES_RW_PASSWORD Docker secret.
# bulk_ro  -- Non-superuser LOGIN role with SELECT-only access, used by
#             Grafana read-only datasources (ALS, AFC_LOGS, fs_state).
#             Password is read from the BULK_POSTGRES_RO_PASSWORD Docker secret.
#
# The postgres superuser credential is reserved exclusively for the db_creator
# REST endpoint (rat_server).  Compromising any other container therefore no
# longer yields a PostgreSQL superuser session on bulk_postgres.
#
# IMPORTANT: This script runs inside the PostgreSQL Docker entrypoint
# initdb hook (docker-entrypoint-initdb.d/), which fires once at first-boot
# when the data directory is empty.  The secret files MUST be mounted in the
# bulk_postgres container (see docker-compose.yaml) before first start.

set -euo pipefail

BULK_RW_PWD_FILE="${BULK_POSTGRES_RW_PASSWORD_FILE:-/run/secrets/BULK_POSTGRES_RW_PASSWORD}"
BULK_RO_PWD_FILE="${BULK_POSTGRES_RO_PASSWORD_FILE:-/run/secrets/BULK_POSTGRES_RO_PASSWORD}"

if [ ! -f "$BULK_RW_PWD_FILE" ]; then
    echo "ERROR: BULK_POSTGRES_RW_PASSWORD secret not found at $BULK_RW_PWD_FILE" >&2
    exit 1
fi
if [ ! -f "$BULK_RO_PWD_FILE" ]; then
    echo "ERROR: BULK_POSTGRES_RO_PASSWORD secret not found at $BULK_RO_PWD_FILE" >&2
    exit 1
fi

BULK_RW_PWD=$(cat "$BULK_RW_PWD_FILE")
BULK_RO_PWD=$(cat "$BULK_RO_PWD_FILE")

# Escape any single quotes in the passwords for safe inclusion in the
# heredoc below.  PostgreSQL SQL string literals escape single quotes by
# doubling them: ' → ''
BULK_RW_PWD_ESC="${BULK_RW_PWD//\'/\'\'}"
BULK_RO_PWD_ESC="${BULK_RO_PWD//\'/\'\'}"

psql --username "$POSTGRES_USER" --no-password <<-EOSQL
    -- Non-superuser read/write role for application services
    CREATE ROLE bulk_rw WITH
        LOGIN
        NOSUPERUSER
        NOCREATEDB
        NOCREATEROLE
        NOINHERIT
        PASSWORD '$BULK_RW_PWD_ESC';

    -- Non-superuser read-only role for Grafana datasources
    CREATE ROLE bulk_ro WITH
        LOGIN
        NOSUPERUSER
        NOCREATEDB
        NOCREATEROLE
        NOINHERIT
        PASSWORD '$BULK_RO_PWD_ESC';
EOSQL

# PostgreSQL 14 grants CREATE on schema public to PUBLIC by default (removed
# only in PG15), so without this revoke bulk_ro would NOT be SELECT-only: any
# role (including bulk_ro) could create objects in public and plant a
# maliciously-named decoy table that partition-maintenance DDL would later
# interpolate (second-order SQL injection), or stage CVE-2018-1058-style
# search-path attacks against superuser maintenance sessions
# (postgis-upgrade.sh). Revoke in template1 so every database created later
# (db_creator REST endpoint) inherits the revocation, and in every database
# that already exists in this cluster. bulk_rw keeps CREATE explicitly: it
# is the documented read/write role and application services create their
# tables/partitions with it.
for _db in template1 $(psql --username "$POSTGRES_USER" --no-password -Atc \
        "SELECT datname FROM pg_database WHERE datistemplate = false AND datallowconn"); do
    psql --username "$POSTGRES_USER" --no-password --dbname "$_db" \
        --command "REVOKE CREATE ON SCHEMA public FROM PUBLIC;" \
        --command "GRANT CREATE ON SCHEMA public TO bulk_rw;"
done
