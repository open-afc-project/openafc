#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10-slim

WORKDIR /usr/app

COPY requirements.txt /usr/app/
RUN /usr/local/bin/python -m pip install --upgrade pip
RUN pip install --no-cache-dir -r requirements.txt

EXPOSE 5000

COPY filestorage_config.py filestorage.py history.py /usr/app/
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /usr/app/filestorage.py /usr/app/history.py /docker-entrypoint.sh

copy google.cert.json /usr/app/google.cert.json

ENV PYTHONPATH=/usr/app

CMD ["/docker-entrypoint.sh"]
