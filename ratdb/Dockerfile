FROM postgres:14.17-alpine3.21
ENV POSTGRES_PASSWORD=postgres
ENV PGDATA=/mnt/nfs/psql/data
#
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY ratdb/devel.sh /
RUN chmod +x /devel.sh
RUN /devel.sh
#
ENV AFC_RATDB_CONNS=${AFC_RATDB_CONNS:-1000}
ENTRYPOINT docker-entrypoint.sh postgres -c max_connections=$AFC_RATDB_CONNS
HEALTHCHECK --start-period=20s --interval=10s --timeout=5s \
    CMD pg_isready -U postgres || exit 1
