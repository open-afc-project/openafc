class postfix::service {
  service { 'postfix':
    ensure     => running,
    enable     => true,
    hasstatus  => false,
    hasrestart => false,
    start      => '/usr/sbin/postfix start',
    stop       => '/usr/sbin/postfix stop',
  }
}
