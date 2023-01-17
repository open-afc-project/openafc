# == Class: fbrat::server
#
# Class configuring all RAT Server operational interfaces (Apache) and daemons.
#
# === Parameters
#
# @param http_host The host name to create a virtual host for.
# @param http_log_level The HTTPd minimum logging severity.
# @param http_allow_plaintext Iff true, TCP port 80 will be opened.
# @param http_tls_keyfile If provided, this is used as the TLS private keypair file for HTTPS and CGW use.
# @param http_tls_certchain If provided, this is used as the TLS X.509 certificate HTTPS.
# @param pgsql_dbname The PostgreSQL database to create and use.
# @param pgsql_username The PostgreSQL role to define and access as.
# @param pgsql_password The role access password.
# @param flask_secret_key The internal private key used for user session keeping.
#   A key can be generated with command similar to `openssl rand -base64 32`
#
class fbrat::server(
  String $http_host = $fqdn,
  Boolean $http_allow_plaintext = false,
  Optional[String] $http_tls_keyfile = undef,
  Optional[String] $http_tls_certchain = undef,
  Integer $http_timeout = 120,
  String $http_log_level = 'warn',
  String $pgsql_host = 'ratdb',
  String $pgsql_dbname = 'fbrat',
  String $pgsql_username = 'postgres',
  String $pgsql_password,
  Integer $ratapi_worker_count = 10,
  Boolean $ratapi_debug_mode = false,
  String $ratapi_google_apikey = '',
  Optional[String] $ratapi_history_dir = undef,
  String $flask_secret_key = '',
){
/*   # Relational database
  class { 'postgresql::server': }
  postgresql::server::db { $pgsql_dbname:
    user     => $pgsql_username,
    password => postgresql_password($pgsql_username, $pgsql_password),
    require  => Class['postgresql::server'],
  } */

  # email backend with local-only access
  class { 'postfix':
    inet_interfaces => 'localhost',
    smtp_listen     => 'localhost',
  }
  postfix::config { 'relay_domains':
    ensure => 'present',
    value  => "localhost ${fqdn}",
  }

  # Message queuing
  class { 'rabbitmq':
    service_manage    => false,
    port              => 5672,
    delete_guest_user => true,
    admin_enable      => false,
    config_ranch      => false,
  }
  rabbitmq_vhost { 'fbrat':
    ensure => present,
  }
  rabbitmq_user { 'celery':
    admin    => true,
    password => 'celery',
    tags     => ['celery'],
  }
  rabbitmq_user_permissions { 'celery@fbrat':
    configure_permission => '.*',
    read_permission      => '.*',
    write_permission     => '.*',
  }

  package { 'python2-celery':
    ensure => 'installed',
  }
  service { 'celery-fbrat':
    ensure    => 'running',
    enable    => false,
    hasstatus => false,
    require   => [Package['python2-celery'], Package['fbrat'], File['/etc/systemd/system/celery-fbrat.service'], File['/etc/default/celery-fbrat'], File['/var/spool/fbrat']],
    start     => "/bin/sh -c \'/bin/celery multi start rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10 -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=INFO\'",
    stop      => "/bin/sh -c \'/bin/celery multi stopwait rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10 --pidfile=/var/run/celery/%n.pid\'",
    restart   => "/bin/sh -c \'/bin/celery multi restart rat_1 rat_2 rat_3 rat_4 rat_5 rat_6 rat_7 rat_8 rat_9 rat_10 -A ratapi.tasks.afc_worker --pidfile=/var/run/celery/%n.pid --logfile=/proc/self/fd/2 --loglevel=INFO\'",
  }
  file { '/etc/systemd/system/celery-fbrat.service':
    content => template('fbrat/etc/celery-fbrat.service.erb'),
    owner   => 'fbrat',
    group   => 'fbrat',
    mode    => '0644',
  }
  file {'/var/lib/fbrat':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/etc/default/celery-fbrat':
    content => template('fbrat/etc/celery-fbrat.erb'),
    owner   => 'fbrat',
    group   => 'fbrat',
    mode    => '0644',
    require => [File['/var/log/celery'], File['/var/run/celery'], File['/var/celery/results'], File['/mnt/nfs/responses']],
  }
  file { '/var/log/celery':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/var/run/celery':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/var/celery/results':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/var/celery':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/var/spool/fbrat':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }

  file { '/mnt/nfs/responses':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/mnt/nfs/afc_config':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0750',
  }
  file { '/var/lib/fbrat/AntennaPatterns':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0750',
  }
  file { '/usr/share/fbrat':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0755',
  }
  file { '/usr/share/fbrat/afc-engine':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0750',
  }
  file { '/usr/share/fbrat/afc-engine/ULS_Database':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0750',
  }
  if $ratapi_history_dir {
    file { $ratapi_history_dir:
      ensure => 'directory',
      owner  => 'fbrat',
      group  => 'fbrat',
      mode   => '0750',
    }
  }


  # HTTP frontend
  class { 'apache':
    service_enable => true,
    mpm_module     => 'event',
    default_vhost  => false,
    keepalive      => 'On',
    timeout        => $http_timeout,
    log_level      => $http_log_level,
    logroot        => '/',
    error_log      => '/proc/self/fd/2',
  }
  # Non-TLS access for local troubleshooting
  apache::vhost { "${http_host}-notls":
    servername      => $http_host,
    docroot         => '/var/www/html',
    port            => 80,
    ssl             => false,
    logroot         => '/',
    access_log_file => '/proc/self/fd/2',
    error_log_file  => '/proc/self/fd/2',
    error_log       => true,
  }
  # TLS access for normal use
  apache::vhost { "${http_host}-tls":
    servername      => $http_host,
    docroot         => '/var/www/html',
    port            => 443,
    ssl             => true,
    # Local host cert also contains its own CA chain
    ssl_chain       => '/etc/pki/tls/certs/http.crt',
    ssl_cert        => '/etc/pki/tls/certs/http.crt',
    ssl_key         => '/etc/pki/tls/private/http.key',
    ssl_cipher      => 'DHE-RSA-AES256-SHA256:DHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES256-GCM-SHA384',

    logroot         => '/',
    access_log_file => '/proc/self/fd/2',
    error_log_file  => '/proc/self/fd/2',
    error_log       => true,
  }

  # basic user authentication
  file { '/var/www/fbrat.htaccess':
    source => 'puppet:///modules/fbrat/fbrat.htaccess',
    owner  => 'fbrat',
    group  => 'apache',
    mode   => '0440',
  }

  # For python WSGI 
  class { 'apache::mod::wsgi': }
#  selboolean { 'httpd_execmem':
#    value      => 'on',
#    persistent => true,
#  }
#  selboolean { 'httpd_tmp_exec':
#    value      => 'on',
#    persistent => true,
#  }

  file { '/etc/pki/tls/private/http.key':
    ensure => 'present',
    owner  => 'fbrat',
    group  => 'apache',
    mode   => '0440',
    notify => Service['httpd'],
  }
  file { '/etc/pki/tls/certs/http.crt':
    ensure => 'present',
    owner  => 'fbrat',
    group  => 'apache',
    mode   => '0444',
    notify => Service['httpd'],
  }

  # RAT runtime dependencies
  ensure_packages(
    [
      'python-psycopg2',
    ],
    {
      ensure => 'installed',
    }
  )
  # RAT itself
  $ratapi_script_path = '/fbrat'
  package { 'fbrat':
    ensure => 'installed',
  }
  file { '/etc/httpd/conf.d/fbrat.conf':
    content => template('fbrat/httpd/fbrat.conf.erb'),
    owner   => 'fbrat',
    group   => 'fbrat',
    mode    => '0644',
    require => [Package['fbrat']],
    notify  => Service['httpd'],
  }
  file { '/var/www/fbrat.wsgi':
    content => template('fbrat/httpd/fbrat.wsgi.erb'),
    owner   => 'fbrat',
    group   => 'apache',
    mode    => '0644',
    require => [Package['fbrat']],
    notify  => [Service['httpd'], Service['celery-fbrat']],
  }
  file { '/etc/xdg/fbrat/ratapi.conf':
    content => template('fbrat/etc/ratapi.conf.erb'),
    owner   => 'fbrat',
    group   => 'fbrat',
    mode    => '0644',
    require => [Package['fbrat']],
    notify  => [Service['httpd'], Service['celery-fbrat']],
  }
  file { '/etc/tmpfiles.d/celery.conf':
    content => template('fbrat/etc/celery-fbrat.tmp.erb'),
    owner   => 'fbrat',
    group   => 'fbrat',
    mode    => '0644',
    require => [Package['fbrat']],
    notify  => [Service['celery-fbrat']],
  }
  file { '/var/log/fbrat':
    ensure => 'directory',
    owner  => 'fbrat',
    group  => 'fbrat',
    mode   => '0666',
  }
  cron { 'clean-rat-history': # clean history twice a month
    ensure   => present,
    command  => '/usr/bin/rat-manage-api data clean-history',
    hour     => 0,
    minute   => 0,
    month    => absent,
    monthday => [1, 15],
    weekday  => absent,
    user     => 'root',
  }
  cron { 'clean-tmp-files': # clean temp files twice a month
    ensure   => present,
    command  => '/usr/bin/rat-manage-api data clean-tmp-files',
    hour     => 0,
    minute   => 0,
    month    => absent,
    monthday => [1, 15],
    weekday  => absent,
    user     => 'root',
  }
/*   exec { 'rat-db-upgrade':
    path    => ['/usr/bin'],
    unless  => 'rat-manage-api db current 2>/dev/null | grep -E \' \(head\)$\'',
    command => 'rat-manage-api db upgrade head',
    require => [
      #Postgresql::Server::Db[$pgsql_dbname],
      Package['fbrat'],
      File['/etc/xdg/fbrat/ratapi.conf'],
    ],
  }
 */
#  file { '/usr/share/fbrat/afc-engine.sha256':
#  ensure => file,
#  source => "puppet:///modules/fbrat/afc-engine.sha256",
#  }
#
#  exec  { 'check-data-hash':
#    path    =>['/usr/bin'],
#    command => 'sha256sum --check /usr/share/fbrat/afc-engine.sha256',
#    require =>[File['/usr/share/fbrat/afc-engine.sha256'],]
#  }

}
