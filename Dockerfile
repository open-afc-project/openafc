FROM centos:7.7.1908 as build_image
RUN groupadd -g 1003 afc
RUN useradd -u 1003 -g 1003 -m -b /var/lib -s /sbin/nologin afc
RUN curl -sL https://rpm.nodesource.com/setup_14.x | bash -
RUN curl --silent --location https://dl.yarnpkg.com/rpm/yarn.repo | tee /etc/yum.repos.d/yarn.repo
RUN rpm --import https://dl.yarnpkg.com/rpm/pubkey.gpg
RUN yum -y install epel-release
COPY prereqs /wd/prereqs/

RUN cd /wd/prereqs/repo/ && \
    yum -y install subversion rpm-build python2-devel redhat-rpm-config rpmlint cmake3 \
	ninja-build gcc-c++ minizip-devel boost169-devel qt5-qtbase-devel python-nose checkpolicy \
	selinux-policy-devel selinux-policy-targeted yarn \
	iturp452-0.0.0-17626.el7.x86_64.rpm iturp452-devel-0.0.0-17626.el7.x86_64.rpm \
	iturp452-debuginfo-0.0.0-17626.el7.x86_64.rpm
RUN cd /wd/prereqs/repo/ && rpmbuild --rebuild python-wtforms-2.2.1-6.el7.src.rpm
RUN cd /wd/prereqs/repo/ && \
    yum -y install /root/rpmbuild/RPMS/noarch/python2-wtforms-2.2.1-6.el7.noarch.rpm \
	python-flask-wtf-0.14.2-7.el7.noarch.rpm
# stage BUILD
COPY build-rpm.sh /wd/


COPY CMakeLists.txt LICENSE.txt svnrevision.txt version.txt fbrat.rpmlintrc /wd/
COPY cmake /wd/cmake/
COPY pkg /wd/pkg/
COPY selinux /wd/selinux/
COPY src /wd/src/
RUN sh -x /wd/build-rpm.sh

# Stage Install
FROM centos:7.7.1908 as install_image
RUN groupadd -g 1003 afc
RUN useradd -u 1003 -g 1003 -m -b /var/lib -s /sbin/nologin afc
COPY --from=build_image /wd/build/dist/RPMS/x86_64/fbrat* /root/rpmbuild/RPMS/noarch/python2-wtforms-2.2.1-6.el7.noarch.rpm /wd/pkgs/
COPY prereqs /wd/prereqs/

RUN yum install -y https://yum.puppetlabs.com/puppet5-release-el-7.noarch.rpm epel-release && \
    cd /wd/prereqs/repo/ && \
    yum install -y \
    rabbitmq-server \
	postfix \
	python2-flask-mail-0.9.1-2.el7.noarch.rpm \
	python2-flask-user-1.0.1.5-1.el7.noarch.rpm \
    python2-flask_jsonrpc-0.3.1-1.el7.noarch.rpm \
	python-flask-wtf-0.14.2-7.el7.noarch.rpm \
	/wd/pkgs/python2-wtforms-2.2.1-6.el7.noarch.rpm \
	python2-jsmin-2.2.2-2.el7.noarch.rpm \
	python-wsgidav-2.4.1-1.el7.noarch.rpm \
	rpmconf \
	boost169-thread \
	createrepo \
	puppet-agent \
	httpd-2.4.6-97.el7.centos.1.x86_64 \
	httpd-tools-2.4.6-97.el7.centos.1.x86_64 \
	mod_ssl-2.4.6-97.el7.centos.1.x86_64 \
	/wd/prereqs/repo/iturp452* \
	/wd/pkgs/fbrat* \
	mailx \
	python2-celery \
	python-psycopg2 \
	awstats

RUN cd /etc/pki/tls/certs && cp localhost.crt http.crt && cd /etc/pki/tls/private && cp localhost.key http.key
RUN mkdir -p /repos/CentOS/7/7/Packages && cp /wd/pkgs/fbrat* /repos/CentOS/7/7/Packages/ && createrepo /repos/CentOS/7/7/Packages/
COPY puppet_for_docker /wd/puppet/

RUN rabbitmq-server -detached ; \
	/opt/puppetlabs/bin/puppet apply --modulepath /wd/puppet/site-modules:/wd/puppet/environments/rh/.modules /wd/puppet/example.pp ; \
	rabbitmqctl stop

RUN mkdir -p /var/lib/fbrat/daily_uls_parse
RUN chown afc:afc /var/log/fbrat/history /var/lib/fbrat/AntennaPatterns /var/spool/fbrat /var/lib/fbrat /var/lib/fbrat/daily_uls_parse
RUN chown -R afc:afc /var/celery 
RUN echo "DEFAULT_ULS_DIR = '/var/lib/fbrat/ULS_Database'" >> /etc/xdg/fbrat/ratapi.conf

RUN yum -y autoremove rpmconf createrepo puppet5-release puppet-agent && yum -y autoremove
RUN yum -y clean all && rm -rf /wd/* /repos /etc/yum.repos.d/fbrat-repo.repo && yum -y clean all

COPY docker-entrypoint.sh /
RUN chmod +x /docker-entrypoint.sh

FROM centos:7.7.1908
COPY --from=install_image / /

LABEL version="afc-runtime"
WORKDIR /wd
EXPOSE 80 443
ENV PGPORT=5432
CMD ["/docker-entrypoint.sh"]