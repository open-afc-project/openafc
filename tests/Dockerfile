#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10-slim

WORKDIR /usr/app

COPY tests/requirements.txt /usr/app/
RUN /usr/local/bin/python3 -m pip install --upgrade pip
RUN pip3 install --no-cache-dir -r requirements.txt

COPY \
tests/afc_tests.py \
tests/_version.py \
tests/_afc_errors.py \
tests/_afc_types.py \
tests/_wfa_types.py \
tests/afc_input.sqlite3 \
tools/certs.sh \
dispatcher/certs/servers/server.bundle.pem \
dispatcher/certs/clients/test_ca_crt.pem \
dispatcher/certs/clients/test_ca_key.pem \
/usr/app/

ENV AFC_CA_CERT_PATH ${AFC_CA_CERT_PATH:-/usr/app}
ENV PYTHONPATH=/usr/app

ENTRYPOINT ["/usr/app/afc_tests.py"]
CMD ["-h"]
