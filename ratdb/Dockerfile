FROM postgres:14-alpine@sha256:f1341c01408dc7278e9d365ed4f860cd3f87dd16b4464ac326fc0f422083a579
# POSTGRES_PASSWORD is no longer baked in — it must be supplied at runtime
# via POSTGRES_PASSWORD_FILE (Docker secret) or POSTGRES_PASSWORD env var.
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
