#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-alpine3.21@sha256:d4f9227f21409479c7fe92288f2e40b1a56ae591c8ad3b5446dfeaef90f67857
# This variable should be set to used Python version (major.minor only)
ENV AFC_PYTHONVERSION=3.12


# Private PyPI mirror credential is passed via BuildKit secret
# (--secret id=pip_config,src=<pip.conf>) so it is never baked into image
# layers or build-arg history. Omit --secret to use public PyPI.

COPY objstorage/requirements.txt /wd/
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    apk add --update --no-cache gcc python3-dev libc-dev && \
    pip3 install --root-user-action=ignore --no-cache-dir --require-hashes \
        -r /wd/requirements.txt && \
    apk del --purge gcc python3-dev libc-dev && \
    rm -f /wd/requirements.txt

COPY objstorage/docker-entrypoint.sh /wd/
RUN chmod +x /wd/docker-entrypoint.sh

COPY src/afc-packages /wd/afc-packages
RUN --mount=type=secret,id=pip_config,target=/root/.config/pip/pip.conf \
    apk add --update --no-cache gcc python3-dev libc-dev && \
    pip3 install --use-pep517 --root-user-action=ignore --no-cache-dir \
        -r /wd/afc-packages/pkgs.objstorage && \
    apk del --purge gcc python3-dev libc-dev && \
    rm -rf /wd/afc-packages

ENV AFC_OBJST_PORT=${AFC_OBJST_PORT:-"5000"}
ENV AFC_OBJST_WORKERS=${AFC_OBJST_WORKERS:-10}
ENV AFC_OBJST_HIST_PORT=${AFC_OBJST_HIST_PORT:-"4999"}
ENV AFC_OBJST_HIST_WORKERS=${AFC_OBJST_HIST_WORKERS:-2}

# Run as non-root fbrat (uid/gid 1003) to limit blast radius.
# No host bind mounts for this service so the non-root switch is safe.
# Create /storage (the AFC_OBJST_LOCAL_DIR default) with correct ownership
# before the USER switch so the process can write task files.
RUN addgroup -g 1003 fbrat && \
    adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat && \
    chown -R fbrat:fbrat /wd && \
    mkdir -p /storage && \
    chown fbrat:fbrat /storage
WORKDIR /wd
USER fbrat

CMD ["/wd/docker-entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider \
    http://127.0.0.1:${AFC_OBJST_PORT}/objst_healthy || exit 1
