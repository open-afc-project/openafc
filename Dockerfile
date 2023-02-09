#
# Copyright © 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#
# default value of args
ARG BLD_TAG=3.7.1.0
ARG PRINST_TAG=3.7.1.0
ARG BLD_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-build-image
ARG PRINST_NAME=public.ecr.aws/w9v6y1o0/openafc/centos-preinstall-image

# Stage Build
FROM ${BLD_NAME}:${BLD_TAG} as build_image
COPY CMakeLists.txt LICENSE.txt version.txt fbrat.rpmlintrc Doxyfile.in /wd/

COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY selinux /wd/selinux/
COPY src /wd/src/
ARG BUILDREV=localbuild
RUN sh -x /wd/build-rpm.sh

# Stage Install
FROM ${PRINST_NAME}:${PRINST_TAG} as install_image
COPY prereqs/devel.sh /wd/
COPY prereqs/.bashrc /root/
ARG AFC_DEVEL_ENV=production
ENV AFC_DEVEL_ENV ${AFC_DEVEL_ENV}
RUN /wd/devel.sh
RUN mkdir -p /repos/CentOS/7/7/Packages /mnt/nfs
COPY --from=build_image \
    /wd/build/dist/RPMS/x86_64/fbrat* \
    /root/rpmbuild/RPMS/noarch/python2-wtforms-2.2.1-6.el7.noarch.rpm \
        /repos/CentOS/7/7/Packages/
COPY prereqs/repo/python-flask-wtf-0.14.2-7.el7.noarch.rpm /repos/CentOS/7/7/Packages/
COPY prereqs/repo/requests-2.7.0-py2.py3-none-any.whl /repos/CentOS/7/7/Packages/
COPY prereqs/repo/python2_secrets-1.0.5-py2.py3-none-any.whl /repos/CentOS/7/7/Packages/
RUN createrepo /repos/CentOS/7/7/Packages/
RUN echo -e "[fbrat-repo]\nname=FBRAT-Repo\nbaseurl=file:///repos/CentOS/7/7/Packages/\ngpgcheck=0\nenabled=1\n" >> /etc/yum.repos.d/fbrat-repo.repo
RUN yum -y install python-pip
# python2-secrets require requests 2.7.0
RUN pip install /repos/CentOS/7/7/Packages/requests-2.7.0-py2.py3-none-any.whl
RUN pip install /repos/CentOS/7/7/Packages/python2_secrets-1.0.5-py2.py3-none-any.whl

RUN yum -y clean all && \
    rm -rf /var/cache/yum && \
    yum install -y /repos/CentOS/7/7/Packages/fbrat* \
    /repos/CentOS/7/7/Packages/python2-wtforms-2.2.1-6.el7.noarch.rpm \
    /repos/CentOS/7/7/Packages/python-flask-wtf-0.14.2-7.el7.noarch.rpm
COPY puppet_for_docker /wd/puppet/

RUN rabbitmq-server -detached ; \
    yum -y makecache && \
    /opt/puppetlabs/bin/puppet apply --modulepath /wd/puppet/site-modules:/wd/puppet/environments/rh/.modules /wd/puppet/example.pp ; \
    rabbitmqctl stop

RUN mkdir -p /var/lib/fbrat/daily_uls_parse
RUN chown fbrat:fbrat /var/spool/fbrat /var/lib/fbrat /var/lib/fbrat/daily_uls_parse
RUN chown -R fbrat:fbrat /var/celery
RUN echo "DEFAULT_ULS_DIR = '/mnt/nfs/rat_transfer/ULS_Database'" >> /etc/xdg/fbrat/ratapi.conf
RUN echo "AFC_APP_TYPE = 'server'" >> /etc/xdg/fbrat/ratapi.conf

RUN yum -y autoremove rpmconf createrepo puppet7-release puppet-agent && yum -y autoremove
RUN yum -y clean all && rm -rf /wd/* /repos /etc/yum.repos.d/fbrat-repo.repo && yum -y clean all && rm -rf /var/cache/yum

COPY prereqs/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh
COPY prereqs/proxy.conf /etc/httpd/conf.modules.d/

FROM centos:7
COPY --from=install_image / /

LABEL revision="afc-monolith"
WORKDIR /wd
EXPOSE 80 443
ENV PGPORT=5432
ARG CELERY_OPTIONS
ENV CELERY_OPTIONS=$CELERY_OPTIONS
ARG CELERY_LOG
ENV CELERY_LOG=$CELERY_LOG
ARG HTTPD_OPTIONS
ENV HTTPD_OPTIONS=$HTTPD_OPTIONS
ARG AFC_PROD_ENV
ENV AFC_PROD_ENV ${AFC_PROD_ENV}
RUN pip install werkzeug==0.16.1
RUN pip install futures
RUN pip install gunicorn==19.10.0
COPY gunicorn/wsgi.py /wd/
COPY gunicorn/gunicorn.conf.py /wd/
COPY gunicorn/gunicorn.logs.conf /wd/
RUN mkdir -p /run/gunicorn
ENV XDG_DATA_DIRS=$XDG_DATA_DIRS:/usr/local/share:/usr/share:/usr/share/fbrat:/mnt/nfs
CMD ["/docker-entrypoint.sh"]
