# Configure a puppet agent from PuppetLabs repository
class fbrat::puppet_agent {
  package { 'puppetlabs-release-pc1':
    ensure => 'purged',
  }
  package { 'puppet-release':
    ensure   => 'installed',
    name     => 'puppet7-release',
    source   => 'https://yum.puppetlabs.com/puppet7-release-el-7.noarch.rpm',
    provider => 'rpm',
  }
  package { 'puppet-agent':
    ensure  => 'installed',
    require => Package['puppet-release'],
  }
  alternative_entry {'/opt/puppetlabs/bin/puppet':
    ensure   => present,
    altlink  => '/usr/bin/puppet',
    altname  => 'puppet',
    priority => 10,
    require  => Package['puppet-agent'],
  }

  package { 'puppet-bolt':
    ensure  => 'installed',
    require => Package['puppet-release'],
  }
  alternative_entry {'/opt/puppetlabs/bin/bolt':
    ensure   => present,
    altlink  => '/usr/bin/bolt',
    altname  => 'bolt',
    priority => 10,
    require  => Package['puppet-bolt'],
  }
}
