FROM centos:7.7.1908
# Builder-independent state
COPY ca-bundle.crt /etc/pki/tls/certs/ca-bundle.crt
RUN yum-config-manager --add-repo https://repouser:pHp8k9Vg@repo.rkf-engineering.com/artifactory/list/rat-build/rat-build.repo
RUN yum -y install puppet-agent sudo
RUN update-alternatives --install /usr/bin/puppet puppet /opt/puppetlabs/puppet/bin/puppet 10
RUN yum -y clean all
RUN echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel-nopasswd
COPY puppet/ /root/puppet
RUN sh /root/puppet/docker-apply.sh --color=off
COPY runcmd.py /usr/local/bin/runcmd.py
RUN chmod 755 /usr/local/bin/*

# Mirror user account from builder host
# --no-log-init flag is required due to a bug in go https://github.com/golang/go/issues/13548
RUN groupadd -g 1364400010 'jenkins' &&     useradd --no-log-init -u 1364400010 -g 1364400010 -G wheel 'jenkins'
ENV HOME=/home/jenkins
RUN chown -R 1364400010:1364400010 $HOME
USER jenkins
WORKDIR /app
LABEL version="fbrat-282"
