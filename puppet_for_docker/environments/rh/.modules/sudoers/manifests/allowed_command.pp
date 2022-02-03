# define: sudoers::allowed_command
#
#   Creates a new sudoers user specification in /etc/sudoers.d/
#
# Parameters:
#   [*command*]               - the command you want to give access to, eg. '/usr/sbin/service'
#   [*filename*]              - the name of the file to be placed in /etc/sudoers.d/ ($title)
#   [*host*]                  - hosts which can run command (ALL)
#   [*run_as*]                - user to run the command as (root)
#   [*user*]                  - user to give access to
#   [*group*]                 - group to give access to
#   [*require_password*]      - require user to give password, setting to false sets 'NOPASSWD:' (true)
#   [*comment*]               - comment to add to the file
#   [*allowed_env_variables*] - allowed list of env variables ([])
#   [*require_exist*]         - Require the Group or User to exist.
#   [*no_tty*]                - remove default tty requirement (false)
#
# Example usage:
#
#     sudoers::allowed_command{ "acme":
#       command          => "/usr/sbin/service",
#       user             => "acme",
#       require_password => false,
#       comment          => "Allows access to the service command for the acme user"
#     }
#
# Creates the file:
#
#   # /etc/sudoers.d/acme
#   acme ALL=(root) NOPASSWD: /usr/sbin/service
#
# As user 'acme' you can now run the service command without a password, eg:
#
#   sudo service rsyslog restart
#
define sudoers::allowed_command(
  $command,
  $filename         = $title,
  $host             = 'ALL',
  $run_as           = 'root',
  $user             = undef,
  $group            = undef,
  $require_password = true,
  $comment          = undef,
  $allowed_env_variables = [],
  $require_exist    = true,
  $no_tty           = false
) {

  if ($user == undef and $group == undef) {
    fail('must define user or group')
  }

  $nopasswd = $require_password ? {
    true  => '',
    false => ' NOPASSWD:'
  }

  $user_spec = $group ? {
    undef   => $user,
    default => "%${group}"
  }

  if $require_exist {
    $require_spec = $group ? {
      undef   => $user ? { 'ALL' => undef, default => User[$user] },
      default => Group[$group]
    }
  }

  file { "/etc/sudoers.d/${filename}":
    ensure  => file,
    content => validate(template('sudoers/allowed-command.erb'), '/usr/sbin/visudo -cq -f'),
    mode    => '0440',
    owner   => 'root',
    group   => 'root',
    require => $require_spec
  }

}
