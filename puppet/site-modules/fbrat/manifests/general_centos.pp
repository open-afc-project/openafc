# == Class: fbrat::general_centos
#
# Behavior common to all CentOS hosts in the RAT Server Segment.
#
# === Parameters
#
# none
#
class fbrat::general_centos {
  ensure_packages(
    [
      'rpmconf',
    ],
    {
      ensure => 'installed',
    }
  )

  # Log and allow violations for now
  class { 'selinux':
    mode => 'permissive',
  }

  # Base firewall configuration
  class { 'firewalld':
    default_zone => 'public',
  }
  
  # remote admin
  service { 'sshd':
    ensure => 'running',
    enable => true,
  }
  firewalld_service { 'Allow inbound SSH':
    ensure  => 'present',
    zone    => 'public',
    service => 'ssh',
  }
}
