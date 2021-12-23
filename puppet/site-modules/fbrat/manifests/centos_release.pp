# == Class: fbrat::centos_release
#
# Pin the YUM repositories of a CentOS host to a specific
# release version or the latest release.
#
# === Parameters
#
# @param releasever The exact version to pin to, which must be present as
#  a centos "vault" repository, or the string 'latest'.
#
class fbrat::centos_release(
  String $version='7.7.1908',
){
  # This variable is assumed by some repos to only be the major version
  file { '/etc/yum/vars/releasever':
    ensure => 'absent',
  }

  if $version == 'latest' {
    file { '/etc/yum/vars/releasepinver':
      ensure => 'absent',
    }

    yumrepo { 'CentOS-Base-base':
      name       => 'base',
      mirrorlist => 'http://mirrorlist.centos.org/?release=$releasever&arch=$basearch&repo=os&infra=$infra',
      baseurl    => absent,
    }
    yumrepo { 'CentOS-Base-updates':
      name       => 'updates',
      mirrorlist => 'http://mirrorlist.centos.org/?release=$releasever&arch=$basearch&repo=updates&infra=$infra',
      baseurl    => absent,
    }
    yumrepo { 'CentOS-Base-extras':
      name       => 'extras',
      mirrorlist => 'http://mirrorlist.centos.org/?release=$releasever&arch=$basearch&repo=extras&infra=$infra',
      baseurl    => absent,
    }
    yumrepo { 'CentOS-Base-centosplus':
      name       => 'centosplus',
      mirrorlist => 'http://mirrorlist.centos.org/?release=$releasever&arch=$basearch&repo=centosplus&infra=$infra',
      baseurl    => absent,
    }
    yumrepo { 'CentOS-Debuginfo':
      name    => 'base-debuginfo',
      baseurl => 'http://debuginfo.centos.org/7/$basearch/',
    }
  }
  else {
    file { '/etc/yum/vars/releasepinver':
      content => "${version}\n",
      owner   => 'root',
      group   => 'root',
      mode    => '0644',
    }
  
    yumrepo { 'CentOS-Base-base':
      name       => 'base',
      mirrorlist => absent,
      baseurl    => 'http://vault.centos.org/centos/$releasepinver/os/$basearch/',
    }
    yumrepo { 'CentOS-Base-updates':
      name       => 'updates',
      mirrorlist => absent,
      baseurl    => 'http://vault.centos.org/centos/$releasepinver/updates/$basearch/',
    }
    yumrepo { 'CentOS-Base-extras':
      name       => 'extras',
      mirrorlist => absent,
      baseurl    => 'http://vault.centos.org/centos/$releasepinver/extras/$basearch/',
    }
    yumrepo { 'CentOS-Base-centosplus':
      name       => 'centosplus',
      mirrorlist => absent,
      baseurl    => 'http://vault.centos.org/centos/$releasepinver/centosplus/$basearch/',
    }
    yumrepo { 'CentOS-Debuginfo':
      name    => 'base-debuginfo',
      baseurl => 'http://debuginfo.centos.org/7/$basearch/',
    }
  }
}
