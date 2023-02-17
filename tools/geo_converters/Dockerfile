# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# This Dockerfile contains various scripts that use GDAL library and utilities.
# It might be useful in environments where DDAL can't be properly installed,
# otherwise there are no reasoins to run those scripts outside the container.



FROM osgeo/gdal:alpine-normal-latest

RUN apk add py3-pip
RUN pip install pyaml
WORKDIR /usr/app
COPY dir_md5.py nlcd_wgs84.py nlcd_wgs84.yaml /usr/app/
# If container is created on Windows 'chmod' is necessary. No harm on *nix
RUN chmod a+x /usr/app/dir_md5.py /usr/app/nlcd_wgs84.py
ENV PATH=$PATH:/usr/app

CMD sh
