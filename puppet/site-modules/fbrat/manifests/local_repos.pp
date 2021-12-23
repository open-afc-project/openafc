# Development environment for RAT
class fbrat::local_repos {
  yumrepo { 'rat-build':
    descr           => 'RAT packages',
    baseurl         => 'https://repouser:pHp8k9Vg@repo.rkf-engineering.com/artifactory/list/rat-build/',
    gpgkey          => 'https://repouser:pHp8k9Vg@repo.rkf-engineering.com/artifactory/list/rat-build/repodata/repomd.xml.key',
    enabled         => 1,
    gpgcheck        => 0,
    repo_gpgcheck   => 1,
    metadata_expire => '600',
  }
}
