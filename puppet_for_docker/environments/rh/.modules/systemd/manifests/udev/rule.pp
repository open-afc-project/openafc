# Adds a custom udev rule
#
# @api public
#
# @see udev(7)
#
# @attr name [Pattern['^.+\.rules$']]
#   The name of the udev rules to create
#
# @param $ensure
#   Whether to drop a file or remove it
#
# @param path
#   The path to the main systemd settings directory
#
# @param selinux_ignore_defaults
#   If Puppet should ignore the default SELinux labels.
#
# @param notify_services
#   List of services to notify when this rule is updated
#
# @param rules
#   The literal udev rules you want to deploy
#
define systemd::udev::rule (
  Array                             $rules,
  Enum['present', 'absent', 'file'] $ensure                  = 'present',
  Stdlib::Absolutepath              $path                    = '/etc/udev/rules.d',
  Optional[Variant[Array, String]]  $notify_services         = [],
  Optional[Boolean]                 $selinux_ignore_defaults = false,
) {
  include systemd

  $filename = assert_type(Pattern['^.+\.rules$'], $name) |$expected, $actual| {
    fail("The \$name should match \'${expected}\', you passed \'${actual}\'")
  }

  file { $filename:
    ensure                  => $ensure,
    owner                   => 'root',
    group                   => 'root',
    mode                    => '0444',
    path                    => join([$path, $name], '/'),
    notify                  => $notify_services,
    selinux_ignore_defaults => $selinux_ignore_defaults,
    content                 => epp("${module_name}/udev_rule.epp", {'rules' => $rules}),
  }
}
