#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
FROM nginx:1.22-alpine
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 \
    && ln -sf python3 /usr/bin/python \
    && python3 -m ensurepip
RUN pip3 install --no-cache --upgrade \
        --root-user-action=ignore pip setuptools
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
    && pip3 install --no-cache --root-user-action=ignore \
     -r /wd/requirements.txt \
    && pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.dispatcher \
    && rm -rf /wd/afc-packages \
    && pip3 uninstall -y setuptools pip \
# create certificate directories
    && mkdir -p \
    /certificates/servers \
    /certificates/clients \
    /etc/nginx/templates

# Server side certificates
COPY dispatcher/certs/servers/server.cert.pem /certificates/servers/
COPY dispatcher/certs/servers/server.key.pem /certificates/servers/
COPY dispatcher/certs/clients/client.bundle.pem /certificates/clients/
# Client side certificates
COPY dispatcher/certs/servers/server.bundle.pem /certificates/servers/
COPY dispatcher/certs/clients/client.key.pem /certificates/clients/
COPY dispatcher/certs/clients/client.bdl.pem /certificates/clients/

# Copy nginx configuration files
COPY dispatcher/nginx.conf /etc/nginx/
COPY dispatcher/nginx.conf.template /etc/nginx/templates/

COPY dispatcher/acceptor.py /wd/

ENV AFC_MSGHND_NAME=${AFC_MSGHND_NAME:-msghnd}
ENV AFC_MSGHND_PORT=${AFC_MSGHND_PORT:-8000}
ENV AFC_PROXY_CONN_TOUT=${AFC_PROXY_CONN_TOUT:-720}
#
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY dispatcher/entrypoint.sh /
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
