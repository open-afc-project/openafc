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
  if [ "${PGSSLMODE:-}" != "verify-full" ] || [ -z "${PGSSLROOTCERT:-}" ]; then
    echo "bulk_postgis_upgrade: refusing password migration: set PGSSLMODE=verify-full and PGSSLROOTCERT=<pinned CA file> so the server receiving the new password is authenticated (the factory-password probe cannot verify peer identity)" >&2
    exit 1
  fi
  export PGSSLMODE PGSSLROOTCERT
  # Probe the factory-default password.  stderr is no longer discarded, so
  # transport/auth errors are visible in the job log.  The probe outcome is
  # made definitive by the authenticated preflight check below: if neither
  # the factory password nor the configured password can authenticate, the
  # script aborts instead of assuming "already migrated".
  if PGPASSWORD=postgres psql -h "$PGHOST" -U "$PGUSER" -d postgres \
        -Atc "SELECT 1" >/dev/null; then
    echo "bulk_postgis_upgrade: migrating postgres superuser password"
    # shellcheck disable=SC2154  # PGPASSWORD is set by the caller entrypoint
    # Pass the ALTER USER statement on stdin (not -c) so the secret never
    # appears in process argument lists.  Double single quotes so the
    # password cannot break out of the SQL string literal.
    pw_sql=$(printf "%s" "${PGPASSWORD}" | sed "s/'/''/g")
    printf "ALTER USER postgres PASSWORD '%s';\n" "${pw_sql}" \
      | PGPASSWORD=postgres psql -h "$PGHOST" -U "$PGUSER" -d postgres
  else
    echo "bulk_postgis_upgrade: factory-default password not accepted;" \
         "verifying configured credentials before skipping migration"
  fi
fi

# Fail closed: prove this script can authenticate under the current
# transport settings before treating any later failure as benign.  Without
# this check an unsatisfiable requirement (e.g. PGCHANNELBINDING=require
# against a TLS-less server, which never offers SCRAM-SHA-256-PLUS) made
# every authenticated psql below fail, all hardening silently no-op, and
# the job still exit 0 logging "done".
if ! psql -h "$PGHOST" -U "$PGUSER" -d postgres -Atc "SELECT 1" >/dev/null; then
  echo "bulk_postgis_upgrade: FATAL: cannot authenticate to ${PGHOST} as ${PGUSER}." >&2
  echo "bulk_postgis_upgrade: if the server has no TLS, channel_binding=require can never" >&2
  echo "bulk_postgis_upgrade: be satisfied: provision server TLS (preferred) or explicitly" >&2
  echo "bulk_postgis_upgrade: set PGCHANNELBINDING=disable for a TLS-less network." >&2
  exit 1
fi

# Harden template1 on pre-existing deployments (the initdb hook only runs on
# first boot): PG14 grants CREATE on schema public to PUBLIC by default,
# which would let bulk_ro create objects there.  Databases created later
# inherit the revocation from template1.  bulk_rw deliberately does NOT
# keep CREATE in template1: a template grant would let any holder of the
# fleet-shared bulk_rw credential seed decoy/search-path objects that
# CREATE DATABASE clones into every new database (CVE-2018-1058 staging
# against later superuser sessions).  The explicit REVOKE below also
# removes the grant from pre-existing deployments that received it from
# earlier versions of this script.  Application databases get their
# bulk_rw CREATE grant in the per-database loop below and from db_creator
# at creation time.  Failures abort the job (set -e) so compose
# surfaces them instead of reporting success.
psql -h "$PGHOST" -U "$PGUSER" -d template1 -c \
  "REVOKE CREATE ON SCHEMA public FROM PUBLIC; REVOKE CREATE ON SCHEMA public FROM bulk_rw;"

echo "bulk_postgis_upgrade: running postgis_extensions_upgrade() per database (best-effort)..."

# Capture the list in an assignment so a psql failure aborts the job
# (a failing command substitution inside 'for' does not trip set -e).
db_list=$(psql -h "$PGHOST" -U "$PGUSER" -d postgres -Atc \
  "SELECT datname FROM pg_database WHERE datistemplate = false AND datallowconn")
# shellcheck disable=SC2013
for db in $db_list; do
  echo "bulk_postgis_upgrade: database=${db}"
  # Same PG14 public-schema hardening for pre-existing databases - applied
  # to EVERY database regardless of PostGIS presence (the has_pg gate below
  # covers only the extension upgrade; it used to sit above this REVOKE,
  # leaving pre-existing non-PostGIS databases with PUBLIC CREATE on
  # schema public).
  # Not suppressed: a failed REVOKE must fail the job, not fake success.
  psql -h "$PGHOST" -U "$PGUSER" -d "$db" -c \
    "REVOKE CREATE ON SCHEMA public FROM PUBLIC;"
  if [ "$db" != "postgres" ]; then
    psql -h "$PGHOST" -U "$PGUSER" -d "$db" -c \
      "GRANT CREATE ON SCHEMA public TO bulk_rw;"
  fi
  # Fail closed: a probe failure aborts the job via set -e (the assignment
  # carries the command substitution's exit status), consistent with the
  # authenticated preflight above - instead of '|| echo f' silently
  # skipping this database while the job still exits 0.
  has_pg=$(psql -h "$PGHOST" -U "$PGUSER" -d "$db" -Atc \
    "SELECT EXISTS (SELECT 1 FROM pg_extension WHERE extname = 'postgis');")
  if [ "$has_pg" != "t" ]; then
    echo "bulk_postgis_upgrade: database=${db} (skip extension upgrade: PostGIS not installed)"
    continue
  fi
  # Pin search_path so unqualified names in this superuser session resolve
  # in pg_catalog only (CVE-2018-1058); the upgrade call is schema-qualified
  # (the postgis/postgis image installs PostGIS into schema public).
  psql -h "$PGHOST" -U "$PGUSER" -d "$db" -v ON_ERROR_STOP=0 -c \
    "SET search_path = pg_catalog; SELECT public.postgis_extensions_upgrade();" 2>/dev/null || true
done

echo "bulk_postgis_upgrade: done"
