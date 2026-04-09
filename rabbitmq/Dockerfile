FROM rabbitmq:4.3.0-alpine@sha256:4f4a74fe951bb8bebae41187c8592ac0737e63d929467502f17689b22e9e9204

# LOG level for console output (debug, info, warning, error, critical, none)
ENV RMQ_LOG_CONSOLE_LEVEL=warning

COPY rabbitmq/rabbitmq.conf /etc/rabbitmq/
RUN echo log.console.level=$RMQ_LOG_CONSOLE_LEVEL >> /etc/rabbitmq/rabbitmq.conf

# copy template (with ${BROKER_PWD} / ${RCACHE_RMQ_PWD} placeholders);
# the shim substitutes env vars at startup so no plaintext password is baked
# into the image.
COPY rabbitmq/definitions.json.template /etc/rabbitmq/
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY rabbitmq/devel.sh /
RUN chmod +x /devel.sh
RUN /devel.sh
# Install gettext for envsubst used by the entrypoint shim
RUN apk add --no-cache gettext
COPY rabbitmq/docker-entrypoint-shim.sh /docker-entrypoint-shim.sh
RUN chmod +x /docker-entrypoint-shim.sh
ENTRYPOINT ["/docker-entrypoint-shim.sh"]
CMD ["rabbitmq-server"]
HEALTHCHECK --start-period=60s --interval=20s --timeout=5s --retries=3 \
    CMD rabbitmq-diagnostics -q check_port_connectivity || exit 1
