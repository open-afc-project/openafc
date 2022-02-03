node 'devel.example.com' {
  include fbrat::centos_release
  include fbrat::general_centos
  include fbrat::puppet_agent

  class { 'fbrat::devel':
  }
}

node 'default' {
  include fbrat::centos_release
  include fbrat::general_centos
  include fbrat::puppet_agent

  class { 'fbrat::server':
    pgsql_password       => 'N3SF0LVKJx1RAhFGx4fcw',
    flask_secret_key     => '9XXc5Lw+DZwXINyOmKcY5c41AMhLabqn4jFLXJntsVutrZCauB5W/AOv7tDbp63ge2SS2Ujz/OnfeQboJOrbsQ',
    http_allow_plaintext => true,
    ratapi_debug_mode    => true,
    ratapi_google_apikey => 'AIzaSyAjcMamfS5LhIRzQ6Qapi0uKX151himkmQ',
    ratapi_history_dir   => '/var/log/fbrat/history',
  }
  class { 'fbrat::awstats':
  }
  class { 'fbrat::access_control':
    access_ratapi_addrs  => [
    ],
    access_paws_addrs    => [
    ],
    access_awstats_addrs => [
    ],
  }
}
