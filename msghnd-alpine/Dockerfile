FROM alpine:3.17 as preinstall_image
#
RUN mkdir -m 750 -p /var/lib/fbrat/AntennaPatterns
RUN mkdir -m 755 -p /var/spool/fbrat /var/lib/fbrat /var/celery /var/run/celery /var/log/celery
RUN addgroup -g 1003 fbrat
RUN adduser -g '' -D -u 1003 -G fbrat -h /var/lib/fbrat -s /sbin/nologin fbrat
RUN chown fbrat:fbrat /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/celery
#
# Requiered for building python packages
#
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 && ln -sf python3 /usr/bin/python
RUN apk add --update --no-cache py3-six py3-numpy py3-cryptography py3-sqlalchemy py3-requests py3-flask py3-psycopg2
RUN python3 -m ensurepip
RUN pip3 install --no-cache --upgrade pip setuptools
RUN apk add --update --no-cache cmake ninja

FROM alpine:3.17
COPY --from=preinstall_image / /
COPY msghnd-alpine/requirements.txt /wd/
RUN pip install -r /wd/requirements.txt
COPY gunicorn/wsgi.py gunicorn/gunicorn.conf.py gunicorn/gunicorn.logs.conf /wd/
RUN mkdir -p /run/gunicorn /etc/xdg/fbrat
COPY prereqs/ratapi.conf /etc/xdg/fbrat/
RUN echo "AFC_APP_TYPE = 'msghnd'" >> /etc/xdg/fbrat/ratapi.conf
#
# add development env
#
#COPY msghnd-alpine/devel.sh /wd/
#COPY msghnd-alpine/.bashrc /root/
#ARG AFC_DEVEL_ENV=production
#ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
#RUN /wd/devel.sh
#
COPY . /wd/
RUN mkdir -p -m 777 /wd/build
RUN cd /wd/build && \
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH=usr -DCMAKE_BUILD_TYPE=RatapiRelease -G Ninja /wd && \
ninja -j$(nproc) install
#
WORKDIR /wd
EXPOSE 8000
ENV PGPORT=5432
COPY msghnd-alpine/entrypoint.sh /
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
