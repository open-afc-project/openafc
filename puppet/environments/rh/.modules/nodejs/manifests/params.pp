class nodejs::params {
  $npmrc_auth                  = undef
  $npmrc_config                = undef
  $nodejs_debug_package_ensure = 'absent'
  $nodejs_package_ensure       = 'present'
  $repo_enable_src             = false
  $repo_ensure                 = 'present'
  $repo_pin                    = undef
  $repo_priority               = 'absent'
  $repo_proxy                  = 'absent'
  $repo_proxy_password         = 'absent'
  $repo_proxy_username         = 'absent'
  $repo_release                = undef
  $repo_url_suffix             = '8.x'
  $use_flags                   = ['npm', 'snapshot']

  $cmd_exe_path = $facts['os']['family'] ? {
    'Windows' => "${facts['os']['windows']['system32']}\\cmd.exe",
    default   => undef,
  }

  case $facts['os']['family'] {
    'Debian': {
      if $facts['os']['release']['major'] =~ /^[89]$/ {
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-dbg'
        $nodejs_dev_package_name   = undef
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = false
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }
      elsif $facts['os']['release']['full'] =~ /^1[468]\.04$/ {
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-dbg'
        $nodejs_dev_package_name   = 'nodejs-dev'
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = 'npm'
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }
      else {
        warning("The ${module_name} module might not work on ${facts['os']['name']} ${facts['os']['release']['full']}. Sensible defaults will be attempted.")
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-dbg'
        $nodejs_dev_package_name   = 'nodejs-dev'
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = 'npm'
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }

      $package_provider          = undef
    }
    'RedHat': {
      if $facts['os']['release']['major'] =~ /^[67]$/ {
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-debuginfo'
        $nodejs_dev_package_name   = 'nodejs-devel'
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = 'npm'
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }
      elsif $facts['os']['name'] == 'Fedora' {
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-debuginfo'
        $nodejs_dev_package_name   = 'nodejs-devel'
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = 'npm'
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }
      elsif ($facts['os']['name'] == 'Amazon') {
        $manage_package_repo       = true
        $nodejs_debug_package_name = 'nodejs-debuginfo'
        $nodejs_dev_package_name   = 'nodejs-devel'
        $nodejs_dev_package_ensure = 'absent'
        $nodejs_package_name       = 'nodejs'
        $npm_package_ensure        = 'absent'
        $npm_package_name          = 'npm'
        $npm_path                  = '/usr/bin/npm'
        $repo_class                = '::nodejs::repo::nodesource'
      }
      else {
        fail("The ${module_name} module is not supported on ${::operatingsystem} ${::operatingsystemrelease}.")
      }
      $package_provider          = undef
    }
    'Suse': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = 'nodejs-debuginfo'
      $nodejs_dev_package_name   = 'nodejs-devel'
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'nodejs'
      $npm_package_ensure        = 'present'
      $npm_package_name          = 'npm'
      $npm_path                  = '/usr/bin/npm'
      $repo_class                = undef
      $package_provider          = undef
    }
    'Archlinux': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = undef
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'nodejs'
      $npm_package_ensure        = 'present'
      $npm_package_name          = 'npm'
      $npm_path                  = '/usr/bin/npm'
      $repo_class                = undef
      $package_provider          = undef
    }
    'FreeBSD': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = 'www/node-devel'
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'www/node'
      $npm_package_ensure        = 'present'
      $npm_package_name          = 'www/npm'
      $npm_path                  = '/usr/bin/npm'
      $repo_class                = undef
      $package_provider          = undef
    }
    'OpenBSD': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = undef
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'node'
      $npm_package_ensure        = 'absent'
      $npm_package_name          = false
      $npm_path                  = '/usr/local/bin/npm'
      $repo_class                = undef
      $package_provider          = undef
    }
    'Darwin': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = 'nodejs-devel'
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'nodejs'
      $npm_package_ensure        = 'present'
      $npm_package_name          = 'npm'
      $npm_path                  = '/opt/local/bin/npm'
      $repo_class                = undef
      $package_provider          = 'macports'
    }
    'Windows': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = undef
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'nodejs'
      $npm_package_ensure        = 'absent'
      $npm_package_name          = 'npm'
      $npm_path                  = '"C:\Program Files\nodejs\npm.cmd"'
      $repo_class                = undef
      $package_provider          = 'chocolatey'
    }
    'Gentoo': {
      $manage_package_repo       = false
      $nodejs_debug_package_name = undef
      $nodejs_dev_package_name   = undef
      $nodejs_dev_package_ensure = 'absent'
      $nodejs_package_name       = 'net-libs/nodejs'
      $npm_package_ensure        = 'absent'
      $npm_package_name          = false
      $npm_path                  = '/usr/bin/npm'
      $repo_class                = undef
      $package_provider          = undef
    }
    default: {
      fail("The ${module_name} module is not supported on a ${facts['os']['name']} distribution.")
    }
  }
}
