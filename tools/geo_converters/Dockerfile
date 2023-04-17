# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# This Dockerfile contains various scripts that use GDAL library and utilities.
# It might be useful in environments where DDAL can't be properly installed,
# otherwise there are no reasons to run those scripts outside the container.



FROM osgeo/gdal:alpine-normal-latest

RUN apk add py3-pip
RUN pip install pyaml jsonschema
WORKDIR /usr/app
COPY geoutils.py /usr/app/
COPY dir_md5.py g8l_info_schema.json /usr/app/
COPY nlcd_wgs84.py nlcd_wgs84.yaml /usr/app/
COPY tiler.py to_wgs84.py lidar_merge.py to_png.py /usr/app/

# If container is created on Windows 'chmod' is necessary. No harm on *nix
RUN chmod a+x /usr/app/*.py
ENV PATH=$PATH:/usr/app

CMD sh
