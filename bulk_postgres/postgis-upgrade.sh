#!/bin/sh
# Run after bulk_postgres is healthy: upgrade PostGIS SQL objects in every database
# that has the extension (no-op if already current). Safe to run on every compose up.
set -eu

export PGHOST="${PGHOST:-bulk_postgres}"
export PGUSER="${PGUSER:-postgres}"
export PGPASSWORD="${PGPASSWORD:-postgres}"

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
  psql -h "$PGHOST" -U "$PGUSER" -d "$db" -v ON_ERROR_STOP=0 -c \
    "SELECT postgis_extensions_upgrade();" 2>/dev/null || true
done

echo "bulk_postgis_upgrade: done"
