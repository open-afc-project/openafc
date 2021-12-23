require 'beaker-rspec'
require 'beaker-puppet'
require 'beaker/puppet_install_helper'
require 'beaker/module_install_helper'

run_puppet_install_helper unless ENV['BEAKER_provision'] == 'no'
install_ca_certs unless ENV['PUPPET_INSTALL_TYPE'] =~ %r{pe}i
install_module_on(hosts)
install_module_dependencies_on(hosts)

# Additional modules for soft deps
install_module_from_forge('puppetlabs-apt', '>= 4.4.0 < 8.0.0')
install_module_from_forge('stahnma-epel', '>= 1.2.0 < 2.0.0')
# We do not run Beaker acceptance tests on Windows or Gentoo at the moment.
# install_module_from_forge('puppet-chocolatey', '>= 2.0.0 < 4.0.0')
# install_module_from_forge('gentoo-portage', '>= 2.0.1 < 3.0.0')

UNSUPPORTED_PLATFORMS = %w[AIX Solaris].freeze

RSpec.configure do |c|
  # Readable test descriptions
  c.formatter = :documentation
end

shared_examples 'an idempotent resource' do
  it 'applies with no errors' do
    apply_manifest(pp, catch_failures: true)
  end

  it 'applies a second time without changes' do
    apply_manifest(pp, catch_changes: true)
  end
end
