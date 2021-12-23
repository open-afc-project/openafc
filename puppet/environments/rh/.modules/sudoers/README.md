# sudoers::allowed_command

Puppet module for creating sudoers user specifications

[![Build Status](https://travis-ci.org/phinze/puppet-sudoers.svg)](https://travis-ci.org/phinze/puppet-sudoers)

### Install

Using the Puppet Module Tool:

    $ puppet module install phinze/sudoers

As a git submodule:

    $ git submodule add git@github.com:phinze/puppet-sudoers.git modules/sudoers


### Usage

The following puppet declaration:

```puppet
sudoers::allowed_command{ "acme":
  command          => "/usr/sbin/service",
  user             => "acme",
  require_password => false,
  comment          => "Allows access to the service command for the acme user"
}
```

Creates the file:

```
# /etc/sudoers.d/acme
acme ALL=(root) NOPASSWD: /usr/sbin/service
```

As user 'acme' you can now run the service command without a password, eg:

    $ sudo service rsyslog restart


### Parameters

The `allowed_command` type takes the following options (with defaults in brackets):

```
[*command*]               - the command you want to give access to, eg. '/usr/sbin/service'
[*filename*]              - the name of the file to be placed in /etc/sudoers.d/ ($title)
[*host*]                  - hosts which can run command (ALL)
[*run_as*]                - user to run the command as (root)
[*user*]                  - user to give access to
[*group*]                 - group to give access to
[*require_password*]      - require user to give password, setting to false sets 'NOPASSWD:' (true)
[*comment*]               - comment to add to the file
[*allowed_env_variables*] - allowed list of env variables ([])
[*require_exist*]         - Require the Group or User to exist. Setting this to false for example is needed if the user groups come from Active Directory. (true)
[*no_tty*]                - remove default tty requirement (false)
```
