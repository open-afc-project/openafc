#
# Copyright � 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.12-slim@sha256:57cd7c3a7a273101a6485ba99423ee568157882804b1124b4dd04266317710de

WORKDIR /usr/app

COPY requirements.txt /usr/app/
RUN /usr/local/bin/python3 -m pip install --upgrade pip
RUN pip3 install --no-cache-dir --root-user-action=ignore --require-hashes -r requirements.txt

COPY keyhole_gen.py /usr/app/
RUN chmod a+x /usr/app/keyhole_gen.py

ENV PYTHONPATH=/usr/app

USER nobody

ENTRYPOINT ["/usr/app/keyhole_gen.py"]
