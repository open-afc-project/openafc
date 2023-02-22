#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

FROM python:3.10-slim

WORKDIR /usr/app

COPY tests/requirements.txt /usr/app/
RUN /usr/local/bin/python3 -m pip install --upgrade pip
RUN pip3 install --no-cache-dir -r requirements.txt

COPY tests/afc_tests.py /usr/app/afc_tests.py
COPY tests/_version.py /usr/app/_version.py
COPY tests/_afc_errors.py /usr/app/_afc_errors.py
COPY tests/_afc_types.py /usr/app/_afc_types.py
COPY tests/_wfa_types.py /usr/app/_wfa_types.py
COPY tests/afc_input.sqlite3 /usr/app/afc_input.sqlite3

ENV PYTHONPATH=/usr/app

ENTRYPOINT ["/usr/app/afc_tests.py"]
CMD ["-h"]
