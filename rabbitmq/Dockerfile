FROM rabbitmq:3.13.7-alpine

# LOG level for console output (debug, info, warning, error, critical, none)
ENV RMQ_LOG_CONSOLE_LEVEL=warning

COPY rabbitmq/rabbitmq.conf /etc/rabbitmq/
RUN echo log.console.level=$RMQ_LOG_CONSOLE_LEVEL >> /etc/rabbitmq/rabbitmq.conf

COPY rabbitmq/definitions.json /etc/rabbitmq/
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY rabbitmq/devel.sh /
RUN chmod +x /devel.sh
RUN /devel.sh
#
ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["rabbitmq-server"]
HEALTHCHECK --start-period=60s --interval=20s --timeout=5s --retries=3 \
    CMD rabbitmq-diagnostics -q check_port_connectivity || exit 1
