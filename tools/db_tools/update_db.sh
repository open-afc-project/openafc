#!/bin/bash
set -e

if [ $# -ne 3 ] || ! [ -d $1 ]; then
	echo "Usage: $0 <pgdata_dir> <old_postgres_version> <new_postgres_version>"
	echo "       POSTGRES_PASSWORD is read from stdin (never pass it on argv)."
	exit
fi

pgdata=$(readlink -f $1)
read -rsp "POSTGRES_PASSWORD: " POSTGRES_PASSWORD; echo
export POSTGRES_PASSWORD
db_name=fbrat
ver_old=$2
ver_new=$3
wd=$(dirname "$pgdata")

mkdir -p "$wd"/dbtmp
chmod 700 "$wd"/dbtmp

# Require Docker Content Trust so image tags are verified against registry
# signatures (mirrors the S0096-12 hardening in scripts/upgrade_pg_production.sh).
export DOCKER_CONTENT_TRUST=1

docker pull postgres:$ver_old
container=$(docker run --rm -d -e POSTGRES_PASSWORD -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data --name postgres_old postgres:$ver_old)
docker logs -f postgres_old &
sleep 3
docker exec -it $container chown postgres:postgres /var/lib/pgsql/data -R
docker exec -it $container pg_dumpall -U postgres > "$wd"/dbtmp/dump.sql
# The dump is a full plaintext cluster export (all DBs plus pg_authid
# scram-sha-256 role verifiers): restrict to owner-only immediately, it
# must never be left group/world-readable under the invoking umask.
chmod 600 "$wd"/dbtmp/dump.sql
docker stop postgres_old

mv "$pgdata" "$pgdata".back
mkdir "$wd"/pgdata

docker pull postgres:$ver_new
container=$(docker run --rm -d -e POSTGRES_PASSWORD -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data -v "$wd"/dbtmp:/dbtmp --name postgres_new postgres:$ver_new)
docker logs -f postgres_new &
sleep 3
docker exec -it $container chown postgres:postgres /var/lib/pgsql/data -R
docker exec -it $container psql -U postgres -f /dbtmp/dump.sql
docker stop postgres_new

# Restore succeeded: the plaintext dump (all DBs + pg_authid password
# verifiers) has served its purpose and must not be retained at rest.
rm -f "$wd"/dbtmp/dump.sql

# Do NOT replace scram-sha-256 with trust — that disables authentication.
# The pg_hba.conf written by postgres:$ver_new already uses scram-sha-256; leave it as-is.
# If you need password auth during the restore step, scope it to the local/unix socket
# only and revert immediately after.
