# PRIVATE CLASS: do not call directly
class postgresql::server::service {
  $service_ensure   = $postgresql::server::service_ensure
  $service_enable   = false
  $service_manage   = $postgresql::server::service_manage
  $service_name     = $postgresql::server::service_name
  $service_provider = $postgresql::server::service_provider
  $service_status   = $postgresql::server::service_status
  $user             = $postgresql::server::user
  $port             = $postgresql::server::port
  $default_database = $postgresql::server::default_database
  $psql_path        = $postgresql::server::psql_path
  $connect_settings = $postgresql::server::default_connect_settings

  anchor { 'postgresql::server::service::begin': }

  if $service_manage {

    service { 'postgresqld':
      ensure    => $service_ensure,
      enable    => $service_enable,
      name      => $service_name,
      provider  => $service_provider,
      hasstatus => false,
      status    => $service_status,
      start     => "/usr/local/bin/gosu postgres:postgres /usr/bin/pg_ctl start -D \${PGDATA} -s -o \"-p \${PGPORT}\" -w -t 300",
      stop      => "/usr/local/bin/gosu postgres:postgres /usr/bin/pg_ctl stop -D \${PGDATA} -s -m fast",
      restart   => "/usr/local/bin/gosu postgres:postgres /usr/bin/pg_ctl reload -D \${PGDATA} -s",

    }

    if $service_ensure == 'running' {
      # This blocks the class before continuing if chained correctly, making
      # sure the service really is 'up' before continuing.
      #
      # Without it, we may continue doing more work before the database is
      # prepared leading to a nasty race condition.
      postgresql_conn_validator{ 'validate_service_is_running':
        run_as           => $user,
        db_name          => $default_database,
        port             => $port,
        connect_settings => $connect_settings,
        sleep            => 1,
        tries            => 60,
        psql_path        => $psql_path,
        require          => Service['postgresqld'],
        before           => Anchor['postgresql::server::service::end']
      }
      Postgresql::Server::Database <| title == $default_database |> -> Postgresql_conn_validator['validate_service_is_running']
    }
  }

  anchor { 'postgresql::server::service::end': }
}
