#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns 
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10

WORKDIR /usr/app
RUN mkdir -p /dl && chmod 777 /dl

COPY requirements.txt /usr/app/
RUN /usr/local/bin/python -m pip install --upgrade pip
RUN pip install --no-cache-dir -r requirements.txt

COPY filestorage_config.py filestorage.py /usr/app/
RUN chmod +x /usr/app/filestorage.py

ENV PYTHONPATH=/usr/app
ENV FILESTORAGE_HOST="0.0.0.0"
ENV FILESTORAGE_PORT=5000

CMD ["/usr/app/filestorage.py"]
