#
# Copyright © 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
FROM nginx:1.21.6
RUN apt update \
    && apt install -y openssl \
    && apt install -y bash

RUN mkdir -p \
    /wd \
    /certificates/servers \
    /certificates/clients \
    /etc/nginx/templates

# Server side certificates
COPY certs/servers/server.cert.pem /certificates/servers/
COPY certs/servers/server.key.pem /certificates/servers/
COPY certs/clients/client.bundle.pem /certificates/clients/
# Client side certificates
COPY certs/servers/server.bundle.pem /certificates/servers/
COPY certs/clients/client.key.pem /certificates/clients/
COPY certs/clients/client.bdl.pem /certificates/clients/

# Copy nginx configuration files
COPY nginx.conf /etc/nginx/
COPY nginx.conf.template /etc/nginx/templates/

ENV AFC_MSGHND_NAME=${AFC_MSGHND_NAME:-msghnd}
ENV AFC_MSGHND_PORT=${AFC_MSGHND_PORT:-8000}
ENV AFC_PROXY_CONN_TOUT=${AFC_PROXY_CONN_TOUT:-120}
