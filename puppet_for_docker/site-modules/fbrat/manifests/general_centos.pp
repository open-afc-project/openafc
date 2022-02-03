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
}
