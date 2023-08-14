FROM postgres:14.7-alpine
ENV POSTGRES_PASSWORD=N3SF0LVKJx1RAhFGx4fcw
ENV PGDATA=/mnt/nfs/psql/data
ENV POSTGRES_DB=fbrat
#
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY ratdb/devel.sh /
RUN chmod +x /devel.sh
EXPOSE 5432
HEALTHCHECK --start-period=20s --interval=10s --timeout=5s \
    CMD pg_isready -U postgres || exit 1
