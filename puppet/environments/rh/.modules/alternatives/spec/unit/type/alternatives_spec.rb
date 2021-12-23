require 'spec_helper'

describe Puppet::Type.type(:alternatives) do
  describe 'property `path`' do
    it 'passes validation with an absolute path' do
      expect { described_class.new(name: 'ruby', path: '/usr/bin/ruby1.9') }.not_to raise_error
    end
    it 'fails validation without an absolute path' do
      expect { described_class.new(name: 'ruby', path: "The bees they're in my eyes") }.to raise_error Puppet::Error, %r{must be a fully qualified path}
    end
  end

  describe 'validating the mode', pending: 'Type level validation' do
    it 'raises an error if the mode is auto and a path is set' do
      expect do
        described_class.new(name: 'thing', mode: 'auto', path: '/usr/bin/explode')
      end.to raise_error Puppet::Error, %r{Mode cannot be 'auto'}
    end

    it 'raises an error if the mode is manual and a path is not set' do
      expect do
        described_class.new(name: 'thing', mode: 'manual').validate
      end.to raise_error Puppet::Error, %r{Mode cannot be 'manual'}
    end
  end

  describe 'when autorequiring resources' do
    alternative_entry = Puppet::Type.type(:alternative_entry).new(
      name: '/usr/pgsql-9.1/bin/pg_config',
      ensure: :present,
      altlink: '/usr/bin/pg_config',
      altname: 'pgsql-pg_config',
      priority: '910'
    )
    alternatives = described_class.new(
      name: 'pgsql-pg_config',
      path: '/usr/pgsql-9.1/bin/pg_config'
    )

    catalog = Puppet::Resource::Catalog.new
    catalog.add_resource alternative_entry
    catalog.add_resource alternatives
    req = alternatives.autorequire

    it 'is equal' do
      expect(req.size).to eq(1)
    end
    it 'has matching source' do
      expect(req[0].source).to eq alternative_entry
    end
    it 'has matching target' do
      expect(req[0].target).to eq alternatives
    end
  end
end
