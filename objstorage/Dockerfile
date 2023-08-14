#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM alpine:3.17

RUN apk add py3-pip py3-gunicorn py3-gevent \
    gcc python3-dev libc-dev

COPY objstorage/docker-entrypoint.sh /wd/
RUN chmod +x /wd/docker-entrypoint.sh

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.objstorage \
    && rm -rf /wd/afc-packages

ENV AFC_OBJST_PORT=${AFC_OBJST_PORT:-"5000"}
ENV AFC_OBJST_WORKERS=${AFC_OBJST_WORKERS:-10}
ENV AFC_OBJST_HIST_PORT=${AFC_OBJST_HIST_PORT:-"4999"}
ENV AFC_OBJST_HIST_WORKERS=${AFC_OBJST_HIST_WORKERS:-2}
CMD ["/wd/docker-entrypoint.sh"]
HEALTHCHECK CMD wget --no-verbose --tries=1 --spider \
    http://localhost:${AFC_OBJST_PORT}/healthy || exit 1
