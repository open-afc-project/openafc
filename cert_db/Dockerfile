#
# Copyright 2022 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# Install required packages
#
FROM alpine:3.17 as cert_db.preinstall
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 && ln -sf python3 /usr/bin/python && \
apk add --update --no-cache py3-sqlalchemy py3-cryptography py3-numpy \
py3-requests py3-flask py3-psycopg2 && python3 -m ensurepip && \
pip3 install --no-cache --upgrade pip setuptools

COPY cert_db/requirements.txt /wd/
RUN pip3 install -r /wd/requirements.txt && mkdir -p /etc/xdg/fbrat
COPY config/ratapi.conf /etc/xdg/fbrat/
RUN echo "AFC_APP_TYPE = 'cert_db'" >> /etc/xdg/fbrat/ratapi.conf
#
# Build Message Handler application
#
FROM alpine:3.17 as cert_db.build
COPY --from=cert_db.preinstall / /
# Development env
RUN apk add --update --no-cache cmake ninja
#
COPY CMakeLists.txt LICENSE.txt version.txt Doxyfile.in /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY src /wd/src/
RUN mkdir -p -m 777 /wd/build
ARG BUILDREV=localbuild
RUN cd /wd/build && \
cmake -DCMAKE_INSTALL_PREFIX=/wd/__install -DCMAKE_PREFIX_PATH=/usr -DCMAKE_BUILD_TYPE=RatapiRelease -DSVN_LAST_REVISION=$BUILDREV -G Ninja /wd && \
ninja -j$(nproc) install
#
# Install FCCID_DB application
#
FROM alpine:3.17 as cert_db.install
COPY --from=cert_db.preinstall / /
COPY --from=cert_db.build /wd/__install /usr/
COPY src/afc-packages /wd/afc-packages
RUN pip3 install --use-pep517 --root-user-action=ignore \
        -r /wd/afc-packages/pkgs.cert_db \
    && rm -rf /wd/afc-packages
RUN mkdir -m 755 -p /var/lib/fbrat
RUN mkdir -m 755 -p /var/spool/fbrat 

# Add user and group
RUN addgroup -g 1003 fbrat && \
adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat && \
chown fbrat:fbrat /var/lib/fbrat
#
LABEL revision="afc-cert_db"
WORKDIR /wd
COPY cert_db/entrypoint.sh /
# Add debugging env if configured
ARG AFC_DEVEL_ENV=${AFC_DEVEL_ENV:-production}
COPY cert_db/devel.sh /wd/
RUN chmod +x /wd/devel.sh
RUN /wd/devel.sh
#
ADD cert_db/sweep.sh /etc/periodic/daily/
RUN chmod 744 /etc/periodic/daily/sweep.sh
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
