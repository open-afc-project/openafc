# == Class: fbrat::awstats
#
# Class to install and configure AWStats web log monitoring tool.
# The tool allows non-authenticated access, so it leaks use information.
#
# === Parameters
#
# @param awstats_host The host name to collect statistics for.
#
class fbrat::awstats(
  String $awstats_host = $fqdn,
) {
  package { 'awstats':
    ensure          => 'installed',
    install_options => ['--enablerepo', 'epel'],
  }
  class { 'apache::mod::cgid': }
  file { '/etc/httpd/conf.d/awstats.conf':
    content => template('fbrat/awstats/httpd.conf.erb'),
    owner   => 'root',
    group   => 'root',
    mode    => '0644',
    require => [
      Package['awstats'],
      Class['apache::mod::alias'],
    ],
    notify  => Service['httpd'],
  }
  file { "/etc/awstats/awstats.${awstats_host}.conf":
    content => template('fbrat/awstats/awstats.conf.erb'),
    owner   => 'root',
    group   => 'root',
    mode    => '0644',
    require => [Package['awstats']],
  }
}
