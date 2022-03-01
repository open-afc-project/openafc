FROM openafc/centos-build-image:3.3.11.4 as build_image
ARG BUILDREV=localbuild
COPY CMakeLists.txt LICENSE.txt version.txt fbrat.rpmlintrc Doxyfile.in /wd/
RUN echo $BUILDREV > /wd/svnrevision.txt
RUN DEBUGFAIL
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY selinux /wd/selinux/
COPY src /wd/src/
RUN sh -x /wd/build-rpm.sh

# Stage Install
FROM openafc/centos-preinstall-image:3.3.11.4 as install_image
RUN mkdir -p /repos/CentOS/7/7/Packages
COPY --from=build_image \
    /wd/build/dist/RPMS/x86_64/fbrat* \
    /root/rpmbuild/RPMS/noarch/python2-wtforms-2.2.1-6.el7.noarch.rpm \
        /repos/CentOS/7/7/Packages/
COPY prereqs/repo/python-flask-wtf-0.14.2-7.el7.noarch.rpm /repos/CentOS/7/7/Packages/
RUN createrepo /repos/CentOS/7/7/Packages/
RUN echo -e "[fbrat-repo]\nname=FBRAT-Repo\nbaseurl=file:///repos/CentOS/7/7/Packages/\ngpgcheck=0\nenabled=1\n" >> /etc/yum.repos.d/fbrat-repo.repo
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
RUN chown fbrat:fbrat /var/log/fbrat/history /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/lib/fbrat/daily_uls_parse
RUN chown -R fbrat:fbrat /var/celery
RUN echo "DEFAULT_ULS_DIR = '/var/lib/fbrat/ULS_Database'" >> /etc/xdg/fbrat/ratapi.conf

RUN yum -y autoremove rpmconf createrepo puppet7-release puppet-agent && yum -y autoremove
RUN yum -y clean all && rm -rf /wd/* /repos /etc/yum.repos.d/fbrat-repo.repo && yum -y clean all && rm -rf /var/cache/yum

COPY prereqs/docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh

FROM centos:7
COPY --from=install_image / /

LABEL revision="afc-monolith"
WORKDIR /wd
EXPOSE 80 443
ENV PGPORT=5432
CMD ["/docker-entrypoint.sh"]