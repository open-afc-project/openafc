FROM rabbitmq:3.12.2-alpine
COPY rabbitmq/rabbitmq.conf /etc/rabbitmq/
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
