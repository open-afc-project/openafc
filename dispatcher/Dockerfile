#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
FROM nginx:1.27.4-alpine
ENV PYTHONUNBUFFERED=1

RUN apk add --update --no-cache python3

# Setting up venv in /venv and 'activating' it
RUN python3 -m venv /venv
ENV PATH=/venv/bin:$PATH
ENV VIRTUAL_ENV=/venv

#
# Install reqiored python external and internal packages
#
WORKDIR /wd
# copy list of external packages
COPY dispatcher/requirements.txt /wd
# copy internal packages
COPY src/afc-packages ./afc-packages
# install
RUN set -x \
    &&  apk add --update --no-cache python3-dev build-base linux-headers \
    && pip3 install --no-cache --root-user-action=ignore \
     -r /wd/requirements.txt \
    && apk del --purge python3-dev build-base linux-headers \
    && pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.dispatcher \
    && rm -rf /wd/afc-packages /wd/requirements.txt \
# create certificate directories
    && mkdir -p \
    /certificates/servers \
    /etc/nginx/templates

# Server side certificates
COPY dispatcher/certs/servers/server.cert.pem \
dispatcher/certs/servers/server.key.pem /certificates/servers/
# Default client side CA certificates as a placeholder
COPY dispatcher/certs/clients/client.bundle.pem \
/etc/nginx/certs/

# Copy nginx configuration files
COPY dispatcher/nginx.conf /etc/nginx/
COPY dispatcher/nginx.conf.template /etc/nginx/templates/

COPY dispatcher/acceptor.py /wd/

ENV AFC_MSGHND_NAME=${AFC_MSGHND_NAME:-msghnd}
ENV AFC_MSGHND_PORT=${AFC_MSGHND_PORT:-8000}
ENV AFC_WEBUI_NAME=${AFC_WEBUI_NAME:-rat_server}
ENV AFC_WEBUI_PORT=${AFC_WEBUI_PORT:-80}
ENV AFC_ENFORCE_HTTPS=${AFC_ENFORCE_HTTPS:-TRUE}
ENV AFC_SERVER_NAME=${AFC_SERVER_NAME:-"_"}
ENV AFC_ENFORCE_MTLS=${AFC_ENFORCE_MTLS:-false}
ENV AFC_PROXY_CONN_TOUT=${AFC_PROXY_CONN_TOUT:-720}
#
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}

# 'true' to enable access to Prometheus via dispatcher
ENV AFC_PROMETHEUS_ENABLED=false
# Host and port of Prometheus server
ENV AFC_PROMETHEUS_TARGET=prometheus:9090

# 'true' to enable access to Grafana via dispatcher
ENV AFC_GRAFANA_ENABLED=false
# Host and port of Grafana server
ENV AFC_GRAFANA_TARGET=grafana:3000

COPY dispatcher/entrypoint.sh /
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
HEALTHCHECK --start-period=60s --interval=30s --retries=1 \
    CMD curl --fail http://localhost/fbrat/ap-afc/healthy && \
    /wd/acceptor.py --cmd check || exit 1
