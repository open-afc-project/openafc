## [v1.1.0](https://github.com/voxpupuli/puppet-alternatives/tree/v1.1.0) (2017-02-10)

This is the last release with Puppet3 support!
* Modulesync with latest Vox Pupuli defaults

## 2016-12-25 Release 1.0.2

* Modulesync with latest Vox Pupuli defaults
* Fix ALT_RPM_QUERY_REGEX to catch all entries and not only the default ones
* Add Spectest for rpm provider

## 2016-08-12 Release 1.0.1

* Modulesync with latest Vox Pupuli defaults
* Drop of ruby183 support
* new aternative_entry with rpm provider
* Enable explicitly setting mode property to manual
* First release under the Voxpupuli namespace! Migrated from Adrien Thebo
* Rerelease of 1.0.0, which didn't make it to the forge


## 2014-12-19 Release 0.3.0

* New RPM based providers for alternatives and alternative_entry
* Debian based distributions use `update-alternatives` based on $PATH
* The alternatives type now autorequires matching alternate_entry types
* dpkg provider explicitly sets the mode property to manual
* alternative_entry can check for alernative entries for a non-existent entry


## 2014-08-21 Release 0.2.0

* New type: alternative_entry
* The alternatives type now has the `mode` property to use the automatic
    option for a given alternative.
* The update-alternatives binary is no longer hardcoded to use
    /usr/sbin/update-alternatives

### Thanks

Thanks to Michael Moll and jakov Sosic for their work on this release.


## 2013-07-16 Release 0.1.1

This is a backwards compatible bugfix release.

* Module rebuilt with correct permissions.
