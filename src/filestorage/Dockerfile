#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10-slim

WORKDIR /usr/app

COPY src/filestorage/requirements.txt /usr/app/
RUN /usr/local/bin/python -m pip install --upgrade pip
RUN pip install --no-cache-dir --root-user-action=ignore -r requirements.txt

EXPOSE 4999 5000

COPY src/filestorage/*.py /usr/app/
COPY src/filestorage/docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /usr/app/filestorage.py /usr/app/history.py /docker-entrypoint.sh

COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.filestorage \
    && rm -rf /wd/afc-packages

ENV PYTHONPATH=/usr/app

CMD ["/docker-entrypoint.sh"]
