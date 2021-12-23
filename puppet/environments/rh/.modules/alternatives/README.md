# puppet-alternatives

[![Build Status](https://travis-ci.org/voxpupuli/puppet-alternatives.png?branch=master)](https://travis-ci.org/voxpupuli/puppet-alternatives)
[![Code Coverage](https://coveralls.io/repos/github/voxpupuli/puppet-alternatives/badge.svg?branch=master)](https://coveralls.io/github/voxpupuli/puppet-alternatives)
[![Puppet Forge](https://img.shields.io/puppetforge/v/puppet/alternatives.svg)](https://forge.puppet.com/puppet/alternatives)
[![Puppet Forge - downloads](https://img.shields.io/puppetforge/dt/puppet/alternatives.svg)](https://forge.puppet.com/puppet/alternatives)
[![Puppet Forge - endorsement](https://img.shields.io/puppetforge/e/puppet/alternatives.svg)](https://forge.puppet.com/puppet/alternatives)
[![Puppet Forge - scores](https://img.shields.io/puppetforge/f/puppet/alternatives.svg)](https://forge.puppet.com/puppet/alternatives)

Manage alternatives symlinks.

## Synopsis

Using `puppet resource` to inspect alternatives

    root@master:~# puppet resource alternatives
    alternatives { 'aptitude':
      path => '/usr/bin/aptitude-curses',
    }
    alternatives { 'awk':
      path => '/usr/bin/mawk',
    }
    alternatives { 'builtins.7.gz':
      path => '/usr/share/man/man7/bash-builtins.7.gz',
    }
    alternatives { 'c++':
      path => '/usr/bin/g++',
    }
    alternatives { 'c89':
      path => '/usr/bin/c89-gcc',
    }
    alternatives { 'c99':
      path => '/usr/bin/c99-gcc',
    }
    alternatives { 'cc':
      path => '/usr/bin/gcc',
    }

- - -

Using `puppet resource` to update an alternative

    root@master:~# puppet resource alternatives editor
    alternatives { 'editor':
      path => '/bin/nano',
    }
    root@master:~# puppet resource alternatives editor path=/usr/bin/vim.tiny
    notice: /Alternatives[editor]/path: path changed '/bin/nano' to '/usr/bin/vim.tiny'
    alternatives { 'editor':
      path => '/usr/bin/vim.tiny',
    }

- - -

Using the alternatives resource in a manifest:

    class ruby::193 {

      package { 'ruby1.9.3':
        ensure => present,
      }

      # Will also update gem, irb, rdoc, rake, etc.
      alternatives { 'ruby':
        path    => '/usr/bin/ruby1.9.3',
        require => Package['ruby1.9.3'],
      }
    }

    # magic!
    include ruby::193

- - -

Creating a new alternative entry:

    alternative_entry {'/usr/bin/gcc-4.4':
        ensure   => present,
        altlink  => '/usr/bin/gcc',
        altname  => 'gcc',
        priority => 10,
        require  => Package['gcc-4.4-multilib'],
    }

- - -

This module should work on any Debian and RHEL based distribution.

## Contact

* [Source code](https://github.com/voxpupuli/puppet-alternatives)
* [Issue tracker](https://github.com/voxpupuli/puppet-alternatives/issues)
