#!/bin/bash
set -e
set -x

if [ $# -ne 3 ] || ! [ -d $1 ]; then
	echo "Usage: $0 <pgdata_dir> <postgres_password> <postgres_db_name>"
	exit
fi

pgdata=$(readlink -f $1)
pass=$2
db_name=$3
wd=$(dirname "$pgdata")

mkdir -p "$wd"/dbtmp
chmod 777 "$wd"/dbtmp

#docker pull postgres:9.6
container=$(docker run --rm -d -e POSTGRES_PASSWORD="$pass" -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data --name postgres_old postgres:9.6)
sleep 3
docker exec -it $container pg_dumpall -U postgres > "$wd"/dbtmp/dump.sql
docker stop postgres_old

mv "$pgdata" "$pgdata".back
mkdir "$wd"/pgdata
chmod 777 "$wd"/pgdata

#docker pull postgres:14.7
container=$(docker run --rm -d -e POSTGRES_PASSWORD="$pass" -e PGDATA=/var/lib/pgsql/data -e POSTGRES_DB="$db_name" -v "$pgdata":/var/lib/pgsql/data -v "$wd"/dbtmp:/dbtmp --name postgres_new postgres:14.7)
sleep 3
docker exec -it $container psql -U postgres -f /dbtmp/dump.sql
docker stop postgres_new

rm -rf "$wd"/dbtmp






