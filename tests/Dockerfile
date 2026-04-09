#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-slim

# Configure pip to use package registry proxy (optional)
ARG PIP_INDEX_URL

WORKDIR /usr/app
COPY tests/requirements.txt /usr/app/

RUN apt-get update \
    && apt-get install -y --no-install-recommends net-tools \
    && pip3 install --no-cache-dir -r requirements.txt \
    && rm -rf /var/lib/apt/lists/*

COPY \
tests/afc_tests.py \
tests/_version.py \
tests/_afc_errors.py \
tests/_afc_types.py \
tests/_wfa_types.py \
tests/afc_input.sqlite3 \
tools/certs.sh \
dispatcher/certs/servers/server.bundle.pem \
/usr/app/

RUN cd /usr/app && \
    openssl genrsa -out test_ca_key.pem 4096 && \
    openssl req -x509 -new -nodes -key test_ca_key.pem -sha256 -days 3650 \
        -out test_ca_crt.pem \
        -subj "/C=IL/ST=Israel/L=Tel Aviv/O=Broadcom/CN=AFC Testing" && \
    chown -R nobody:nogroup /usr/app

ENV AFC_CA_CERT_PATH=${AFC_CA_CERT_PATH:-/usr/app}
ENV PYTHONPATH=/usr/app

USER nobody

ENTRYPOINT ["/usr/app/afc_tests.py"]
CMD ["-h"]
