#
# Copyright (C) 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
FROM nginx:1.30.1-alpine@sha256:c819f83c54b0361f5557601bf5eb4943d09360e7a7fdf426afc466570f45874d
ENV PYTHONUNBUFFERED=1

RUN apk add --update --no-cache python3 openssl

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
# Private PyPI mirror credential is passed via BuildKit secret
# (--secret id=pip_config,src=<pip.conf>) so it is never baked into image
# layers or build-arg history. Omit --secret to use public PyPI.

# copy internal packages
COPY src/afc-packages ./afc-packages
# install
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    set -x \
    &&  apk add --update --no-cache python3-dev build-base linux-headers expat-dev \
    && pip3 install --no-cache --root-user-action=ignore --require-hashes \
     -r /wd/requirements.txt \
    && apk del --purge python3-dev build-base linux-headers \
    && pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.dispatcher \
    && rm -rf /wd/afc-packages /wd/requirements.txt \
# create certificate directories
    && mkdir -p \
    /certificates/servers \
    /etc/nginx/certs \
    /etc/nginx/templates

# No static dummy CA is baked into the image: a repo-committed placeholder
# would be shared, byte-identical, across every deployment, so its private
# key custody can never be verified from source (CWE-798 class). Instead
# dispatcher/entrypoint.sh generates a fresh, unique, throwaway placeholder
# CA at container start (mirroring the placeholder-CRL generation already
# done there) so nginx can start before acceptor.py installs the real
# bundle, without trusting a shared static cert.

# Copy nginx configuration files
COPY dispatcher/nginx.conf /etc/nginx/
COPY dispatcher/nginx.conf.template /etc/nginx/templates/

COPY dispatcher/acceptor.py /wd/
COPY dispatcher/25-strip-monitoring.sh /docker-entrypoint.d/
RUN chmod +x /docker-entrypoint.d/25-strip-monitoring.sh

ENV AFC_MSGHND_NAME=${AFC_MSGHND_NAME:-msghnd}
ENV AFC_MSGHND_PORT=${AFC_MSGHND_PORT:-8000}
ENV AFC_WEBUI_NAME=${AFC_WEBUI_NAME:-rat_server}
ENV AFC_WEBUI_PORT=${AFC_WEBUI_PORT:-80}
ENV AFC_ENFORCE_HTTPS=${AFC_ENFORCE_HTTPS:-TRUE}
ENV AFC_SERVER_NAME=${AFC_SERVER_NAME:-"_"}
# Secure by default: enforce mTLS unless an operator explicitly opts out
# (e.g. AFC_ENFORCE_MTLS=false for a lab/test deployment).
ENV AFC_ENFORCE_MTLS=${AFC_ENFORCE_MTLS:-true}
ENV AFC_PROXY_CONN_TOUT=${AFC_PROXY_CONN_TOUT:-720}
#
ENV AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}

# 'true' to enable monitoring UIs (Grafana, Prometheus, cAdvisor, RabbitMQ, Kafka UI)
ENV AFC_GRAFANA_ENABLED=false
ENV AFC_GRAFANA_TARGET=grafana:3000
ENV AFC_PROMETHEUS_TARGET=prometheus:9090
ENV AFC_KAFKA_UI_TARGET=kafka_ui:8080
ENV AFC_CADVISOR_TARGET=cadvisor:8080
ENV AFC_RABBITMQ_TARGET=rmq:15672
ENV AFC_ALLOY_TARGET=alloy:12345

COPY dispatcher/entrypoint.sh /
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
HEALTHCHECK --start-period=60s --interval=30s --retries=3 \
    CMD curl --fail http://localhost/fbrat/ap-afc/healthy && \
    /wd/acceptor.py --cmd check || exit 1
