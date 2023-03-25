#!/bin/bash
set -e
set -x

if [ $# -ne 4 ] || ! [ -d $1 ]; then
	echo "Usage: $0 <pgdata_dir> <postgres_password> <old_postgres_version> <new_postgres_version>"
	exit
fi

pgdata=$(readlink -f $1)
pass=$2
db_name=fbrat
ver_old=$3
ver_new=$4
wd=$(dirname "$pgdata")

mkdir -p "$wd"/dbtmp

docker pull postgres:$ver_old
container=$(docker run --rm -d -e POSTGRES_PASSWORD="$pass" -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data --name postgres_old postgres:$ver_old)
docker logs -f postgres_old &
sleep 3
docker exec -it $container chown postgres:postgres /var/lib/pgsql/data -R
docker exec -it $container pg_dumpall -U postgres > "$wd"/dbtmp/dump.sql
docker stop postgres_old

mv "$pgdata" "$pgdata".back
mkdir "$wd"/pgdata

docker pull postgres:$ver_new
container=$(docker run --rm -d -e POSTGRES_PASSWORD="$pass" -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data -v "$wd"/dbtmp:/dbtmp --name postgres_new postgres:$ver_new)
docker logs -f postgres_new &
sleep 3
docker exec -it $container chown postgres:postgres /var/lib/pgsql/data -R
docker exec -it $container psql -U postgres -f /dbtmp/dump.sql
docker stop postgres_new

sed -i "s/scram-sha-256$/trust/" "$pgdata"/pg_hba.conf

rm -rf "$wd"/dbtmp






