# @summary Locks package from updates.
#
# @example Sample usage
#   yum::versionlock { '0:bash-4.1.2-9.el6_2.*':
#     ensure => present,
#   }
#
# @param ensure
#   Specifies if versionlock should be `present`, `absent` or `exclude`.
#
# @note The resource title must use the format
#   "%{EPOCH}:%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}".  This can be retrieved via
#   the command `rpm -q --qf '%{EPOCH}:%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}'.
#   If "%{EPOCH}" returns as '(none)', it should be set to '0'.  Wildcards may
#   be used within token slots, but must not cover seperators, e.g.,
#   '0:b*sh-4.1.2-9.*' covers Bash version 4.1.2, revision 9 on all
#   architectures.
#
# @see http://man7.org/linux/man-pages/man1/yum-versionlock.1.html
define yum::versionlock (
  Enum['present', 'absent', 'exclude'] $ensure = 'present',
) {
  require yum::plugin::versionlock

  assert_type(Yum::VersionlockString, $name) |$_expected, $actual | {
    fail("Package name must be formatted as %{EPOCH}:%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}, not \'${actual}\'. See Yum::Versionlock documentation for details.")
  }

  $line_prefix = $ensure ? {
    'exclude' => '!',
    default   => '',
  }

  unless $ensure == 'absent' {
    concat::fragment { "yum-versionlock-${name}":
      content => "${line_prefix}${name}\n",
      target  => $yum::plugin::versionlock::path,
    }
  }
}
