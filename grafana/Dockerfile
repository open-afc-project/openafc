#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

# Grafana dockerfile (for future expansion)

FROM grafana/grafana:11.3.1-ubuntu

EXPOSE 3000/tcp

# Work directory - a plsce where to all customized files go
ARG WORKDIR=/wd

# NOTE ON PREFIXES
# Following environment variables somehow overide Grafana configuration.
# Variables prefixed with GF_ override correspondent config values verbatim.
# Variables prefixed with GRAFANA_ override them through some additional step
# (like use of __file or __env provider)
# Variables prefixed with GRAFANA_DS_ parameterize Grafana datosources (via
# Jinja templating, as Grafana datasources are not directly templatable)

# Overrides admin username. Default is 'admin'
ENV GF_SECURITY_ADMIN_USER=admin

# Filename, containing grafana admin user password (presumably filled from
# secret). Admin username as seen by Grafana can't be changed just by changing
# contents of this file,
# 'grafana cli admin reset-admin-password <NEW PASSWORD>'
# (or `grafana_tool.py change_admin_password`) should vbe executed.
# If unspecified, utilities that need it use Grafana-default 'admin' password
# ENV GRAFANA_ADMIN_PASSWORD_FILE

# DSN of database where Grafana stores datasources, dashboards, user accounts,
# etc. Default is to use internal SQLite database
# ENV GRAFANA_DATABASE_URL
# File with password to use in $GRAFANA_DATABASE_URL
# ENV GRAFANA_DATABASE_PASSWORD_FILE


# DSN to database server to create Grafana database, provided it is not yet
# there. Default is to not create Grafana database even if it down ot exist
# ENV GRAFANA_DATABASE_INIT_URL

# File with password to use with $GRAFANA_DATABASE_INIT_URL. Default is to use
# password provided in $GRAFANA_DATABASE_INIT_URL
# ENV GRAFANA_DATABASE_INIT_PASSWORD_FILE


# DSN for read-only access to ALS database
# ENV GRAFANA_DS_ALS_RO_DSN

# File with password to use with $GRAFANA_DS_ALS_RO_DSN
# ENV GRAFANA_DS_ALS_RO_PASSWORD_FILE


# DSN for read-only access to AFC JSON logs database
# ENV GRAFANA_DS_JSON_LOG_RO_DSN

# File with password to use with $GRAFANA_DS_JSON_LOG_RO_DSN
# ENV GRAFANA_DS_JSON_LOG_RO_PASSWORD_FILE


# DSN for read-only access to FS downloader state database
# ENV GRAFANA_DS_FS_STATE_RO_DSN

# File with password to use with $GRAFANA_DS_FS_STATE_RO_DSN
# ENV GRAFANA_DS_FS_STATE_RO_PASSWORD_FILE


# Host name of Prometheus server
# ENV GRAFANA_DS_PROMETHEUS_HOST

# Host name of rcache server
ENV GRAFANA_DS_RCACHE_HOST=rcache


# Disabling SqlAlchemy 2.0 transition warnings
ENV SQLALCHEMY_SILENCE_UBER_WARNING=1

USER root

# Establishing work directory
ENV WORKDIR=$WORKDIR
RUN mkdir -p $WORKDIR && chown -R grafana $WORKDIR
WORKDIR $WORKDIR
ENV PYTHONPATH=$WORKDIR
ENV PATH=$PATH:$WORKDIR

RUN apt-get update
RUN apt-get -y install python3-minimal python3-pip

USER grafana

COPY grafana/requirements.txt $WORKDIR/
RUN pip3 install -r $WORKDIR/requirements.txt && \
    rm -f $WORKDIR/requirements.txt

COPY --chown=grafana src/afc-packages $WORKDIR/afc-packages/
RUN pip3 install --use-pep517  -r $WORKDIR/afc-packages/pkgs.grafana \
    && rm -rf $WORKDIR/afc-packages

# Customization of default Grafana config (instantiated from Jinja template)
ENV GF_PATHS_CONFIG=$WORKDIR/custom.ini

COPY --chown=grafana grafana/templates $WORKDIR/templates/

COPY --chown=grafana grafana/entrypoint.sh grafana/grafana_tool.py $WORKDIR/
RUN chmod a+x $WORKDIR/entrypoint.sh $WORKDIR/grafana_tool.py

ENTRYPOINT $WORKDIR/entrypoint.sh
