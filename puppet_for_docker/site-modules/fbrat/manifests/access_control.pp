# == Class: fbrat::access_control
#
# Class to configure Apache HTTPD access control for services.
#
# === Parameters
#
# @param awstats_host The host name to collect statistics for.
#
class fbrat::access_control(
  Array[String] $access_ratapi_addrs = [],
  Array[String] $access_paws_addrs = [],
  Array[String] $access_awstats_addrs = [],
) {
  file { '/etc/httpd/conf.d/access.conf':
    content => template('fbrat/httpd/access.conf.erb'),
    owner   => 'root',
    group   => 'root',
    mode    => '0644',
    notify  => Service['httpd'],
  }
}
