#!/bin/sh
# Run after bulk_postgres is healthy: upgrade PostGIS SQL objects in every database
# that has the extension (no-op if already current). Safe to run on every compose up.
set -eu

export PGHOST="${PGHOST:-bulk_postgres}"
export PGUSER="${PGUSER:-postgres}"
# PGPASSWORD is injected by the bulk_postgis_upgrade entrypoint via
# $(cat /run/secrets/BULK_POSTGRES_PASSWORD).  This fallback allows running
# the script directly during development (e.g. docker exec) if the env is set.
: "${PGPASSWORD:?PGPASSWORD must be set — pass it from /run/secrets/BULK_POSTGRES_PASSWORD}"

# Require SCRAM-over-TLS channel binding on every libpq connection this
# script makes, so no credential material is ever sent over a channel whose
# peer has not proven possession of the password via SCRAM on TLS (libpq's
# default sslmode=prefer silently falls back to plaintext and honors
# cleartext-password auth requests). Operators of TLS-less dev setups may
# override with PGCHANNELBINDING=disable.
export PGCHANNELBINDING="${PGCHANNELBINDING:-require}"

echo "bulk_postgis_upgrade: waiting for PostgreSQL at ${PGHOST}..."
i=0
while ! pg_isready -h "$PGHOST" -U "$PGUSER" 2>/dev/null; do
  i=$((i + 1))
  if [ "$i" -ge 60 ]; then
    echo "bulk_postgis_upgrade: timeout waiting for postgres" >&2
    exit 1
  fi
  sleep 2
done

# Migration helper: if the postgres role's password is still the factory default
# (written during the initial container startup before password-auth was enforced),
# promote it to the current BULK_POSTGRES_PASSWORD so all service connections succeed.
# This is a one-shot migration for deployments with pre-existing data directories.
# Security: this branch transmits the production secret, so it must be
# explicitly requested by the operator (BULK_PG_MIGRATE_DEFAULT_PWD=1) instead
# of auto-probing on every start; the channel-binding requirement exported
# above additionally refuses unauthenticated/plaintext peers.
if [ "${BULK_PG_MIGRATE_DEFAULT_PWD:-0}" = "1" ]; then
  if PGPASSWORD=postgres psql -h "$PGHOST" -U "$PGUSER" -d postgres \
        -Atc "SELECT 1" >/dev/null 2>&1; then
    echo "bulk_postgis_upgrade: migrating postgres superuser password"
    # shellcheck disable=SC2154  # PGPASSWORD is set by the caller entrypoint
    # Pass the ALTER USER statement on stdin (not -c) so the secret never
    # appears in process argument lists.  Double single quotes so the
    # password cannot break out of the SQL string literal.
    pw_sql=$(printf "%s" "${PGPASSWORD}" | sed "s/'/''/g")
    printf "ALTER USER postgres PASSWORD '%s';\n" "${pw_sql}" \
      | PGPASSWORD=postgres psql -h "$PGHOST" -U "$PGUSER" -d postgres
  fi
fi

# Harden template1 on pre-existing deployments (the initdb hook only runs on
# first boot): PG14 grants CREATE on schema public to PUBLIC by default,
# which would let bulk_ro create objects there.  Databases created later
# inherit the revocation from template1.  bulk_rw keeps CREATE explicitly
# (documented read/write role).
psql -h "$PGHOST" -U "$PGUSER" -d template1 -c \
  "REVOKE CREATE ON SCHEMA public FROM PUBLIC; GRANT CREATE ON SCHEMA public TO bulk_rw;" \
  2>/dev/null || true

echo "bulk_postgis_upgrade: running postgis_extensions_upgrade() per database (best-effort)..."

# shellcheck disable=SC2013
for db in $(psql -h "$PGHOST" -U "$PGUSER" -d postgres -Atc \
  "SELECT datname FROM pg_database WHERE datistemplate = false AND datallowconn"); do
  has_pg=$(psql -h "$PGHOST" -U "$PGUSER" -d "$db" -Atc \
    "SELECT EXISTS (SELECT 1 FROM pg_extension WHERE extname = 'postgis');" 2>/dev/null || echo "f")
  if [ "$has_pg" != "t" ]; then
    echo "bulk_postgis_upgrade: database=${db} (skip: PostGIS extension not installed)"
    continue
  fi
  echo "bulk_postgis_upgrade: database=${db}"
  # Same PG14 public-schema hardening for pre-existing databases.
  psql -h "$PGHOST" -U "$PGUSER" -d "$db" -c \
    "REVOKE CREATE ON SCHEMA public FROM PUBLIC; GRANT CREATE ON SCHEMA public TO bulk_rw;" \
    2>/dev/null || true
  # Pin search_path so unqualified names in this superuser session resolve
  # in pg_catalog only (CVE-2018-1058); the upgrade call is schema-qualified
  # (the postgis/postgis image installs PostGIS into schema public).
  psql -h "$PGHOST" -U "$PGUSER" -d "$db" -v ON_ERROR_STOP=0 -c \
    "SET search_path = pg_catalog; SELECT public.postgis_extensions_upgrade();" 2>/dev/null || true
done

echo "bulk_postgis_upgrade: done"
