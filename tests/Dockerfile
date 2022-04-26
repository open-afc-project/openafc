#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns 
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10

WORKDIR /usr/app

COPY requirements.txt /usr/app/
RUN /usr/local/bin/python3 -m pip install --upgrade pip
RUN pip3 install --no-cache-dir -r requirements.txt

COPY afc_tests.py /usr/app/afc_tests.py
COPY _version.py /usr/app/_version.py
COPY _afc_errors.py /usr/app/_afc_errors.py
COPY _wfa_tests.py /usr/app/_wfa_tests.py
COPY afc_input.sqlite3 /usr/app/afc_input.sqlite3

ENV PYTHONPATH=/usr/app

ENTRYPOINT ["/usr/app/afc_tests.py"]
CMD ["-h"]
