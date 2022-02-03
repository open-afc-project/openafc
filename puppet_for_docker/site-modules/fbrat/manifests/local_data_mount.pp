# Mount bulk data so the afc-engine can read it.
class fbrat::local_data_mount(String $ratapi_s3_api_key_secret = '', Optional[String] $cache_disk_uuid = undef) {
  ensure_packages(
    [
      'cifs-utils',
    ],
    {
      ensure => 'installed',
    }
  )

  mount { '/usr/share/fbrat':
    ensure => 'mounted',
    device => '/dev/nvme1n1p1',
    fstype => 'xfs',
  }
}
