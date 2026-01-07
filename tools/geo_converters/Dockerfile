# Copyright (C) 2022 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

# This Dockerfile contains various scripts that use GDAL library and utilities.
# It might be useful in environments where DDAL can't be properly installed,
# otherwise there are no reasons to run those scripts outside the container.



FROM ghcr.io/osgeo/gdal:alpine-normal-3.10.2

RUN apk add py3-pip py3-yaml py3-jsonschema
WORKDIR /usr/app
COPY geoutils.py /usr/app/
COPY dir_md5.py g8l_info_schema.json /usr/app/
COPY nlcd_wgs84.py nlcd_wgs84.yaml /usr/app/
COPY tiler.py to_wgs84.py lidar_merge.py to_png.py /usr/app/
COPY make_population_db.py /usr/app/
RUN chmod a+x /usr/app/*.py

# Installing proc_gdal
COPY proc_gdal/*.h proc_gdal/*.cpp proc_gdal/CMakeLists.txt /usr/app/proc_gdal/src/
COPY proc_gdal/create_lidar_database_threaded.pl /usr/app/proc_gdal/
RUN chmod a+x /usr/app/proc_gdal/*.pl
USER root
# Installed permanently
RUN apk add perl musl
# Build stuff only installed for duration of build, then removed, thus saving
# on image size
RUN apk add cmake g++ make musl-dev && cd /usr/app/proc_gdal/src && \
  cmake . && make && mv proc_gdal /usr/app/proc_gdal && \
  cd .. && rm -rf src && apk del --purge cmake g++ make musl-dev
# USER solr

ENV PATH=$PATH:/usr/app:/usr/app/proc_gdal

CMD ["sh"]
