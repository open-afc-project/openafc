# == Class: fbrat::devel
#
# This class installs development packages and tools necessary to build the RAT from source.
#
# === Parameters
#
class fbrat::devel {
  class { 'nodejs':
    repo_url_suffix => '10.x',
  }
  package { 'yarn':
    ensure   => '1.17.3',
    provider => 'npm',
  }
  ensure_packages(
    [
      # Software/package building:
      'subversion',
      'rpm-build',
      'redhat-rpm-config',
      'rpmlint',
      'cmake3',
      'ninja-build',
      'gcc-c++',
      'python-devel',
      'lcov',
      'doxygen',
      'graphviz',
      'minizip-devel',
      # Build dependencies
      'gtest-devel',
      'boost169-devel',
      'qt5-qtbase-devel',
      'armadillo-devel',
      'gdal-devel',
      # Packaging:
      'checkpolicy',
      'python2-gnupg',
      'pexpect',
      'rpm-sign',
      'createrepo',
      # Unit testing:
      'python-nose',
      'python-flask',
      'python-pyxdg',
      'python-prettytable',
      'selinux-policy-devel',
      'selinux-policy-targeted',
    ],
    {
      ensure => 'installed',
    }
  )
}
