
# Production repositories
class local_repos {
  yumrepo { 'rat-release':
    descr           => 'RAT packages',
    baseurl         => 'file:///var/lib/rpm_repos/rat-release/',
    gpgkey          => 'file:///var/lib/rpm_repos/rat-release/repodata/repomd.xml.key',
    enabled         => 1,
    gpgcheck        => 1,
    metadata_expire => '3600',
  }
}

node 'devel.example.com' {
  include fbrat::centos_release
  include fbrat::general_centos
  include fbrat::puppet_agent

  include local_repos
  class { 'fbrat::devel':
  }
}

node 'prod.example.com' {
  include fbrat::centos_release
  include fbrat::general_centos
  include fbrat::puppet_agent

  include local_repos
  class { 'fbrat::server':
    pgsql_password       => 'N3SF0LVKJx1RAhFGx4fcw',
    flask_secret_key     => '9XXc5Lw+DZwXINyOmKcY5c41AMhLabqn4jFLXJntsVutrZCauB5W/AOv7tDbp63ge2SS2Ujz/OnfeQboJOrbsQ',
    http_allow_plaintext => true,
    ratapi_debug_mode    => false,
    ratapi_google_apikey => 'VfVksTziiVsXL1rKFvUYP9eFt2bQlGj3s3/52o3R99w=',
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
