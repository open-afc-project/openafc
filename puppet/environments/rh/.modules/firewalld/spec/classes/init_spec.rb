require 'spec_helper'
require 'puppet/provider/firewalld'

describe 'firewalld' do


  before do
    Puppet::Provider::Firewalld.any_instance.stubs(:running).returns(:true)
  end

  context 'with defaults for all parameters' do
    it { should contain_class('firewalld') }
  end

  context 'when defining a default zone' do
    let(:params) do
      {
        :default_zone => 'restricted',
      }
    end

    it do
      should contain_exec('firewalld::set_default_zone').with(
        :command => 'firewall-cmd --set-default-zone restricted',
        :unless  => '[ $(firewall-cmd --get-default-zone) = restricted ]',
      ).that_requires('Exec[firewalld::reload]')
    end
  end

  context 'with purge options' do
    let(:params) do
      {
        :purge_direct_rules => true,
        :purge_direct_chains => true,
        :purge_direct_passthroughs => true,
      }
    end

    it do
      should contain_firewalld_direct_purge('rule')
    end

    it do
      should contain_firewalld_direct_purge('passthrough')
    end

    it do
      should contain_firewalld_direct_purge('chain')
    end
  end

  context 'with parameter ports' do
    let(:params) do
      {
        ports:
          {
            'my_port' =>
              {
                'ensure'   => 'present',
                'port'     => '9999',
                'zone'     => 'public',
                'protocol' => 'tcp'
              }

          }
      }
    end

    it do
      should contain_firewalld_port('my_port')
        .with_ensure('present')
        .with_port('9999')
        .with_protocol('tcp')
        .with_zone('public')
        .that_notifies('Exec[firewalld::reload]')
        .that_requires('Service[firewalld]')
    end
  end

  context 'with parameter zones' do
    let(:params) do
      {
        zones:
        {
          'restricted' =>
                      {
                        'ensure' => 'present',
                        'target' => '%%REJECT%%'
                      }
        }
      }
    end

    it do
      should contain_firewalld_zone('restricted')
        .with_ensure('present')
        .with_target('%%REJECT%%')
        .that_notifies('Exec[firewalld::reload]')
        .that_requires('Service[firewalld]')
    end
  end

  context 'with parameter services' do
    let(:params) do
      {
        services:
        {
          'mysql' =>
            {
              'ensure' => 'present',
              'zone'   => 'public'
            }
        }
      }
    end

    it do
      should contain_firewalld_service('mysql')
        .with_ensure('present')
        .with_zone('public')
        .that_notifies('Exec[firewalld::reload]')
        .that_requires('Service[firewalld]')
    end
  end

  context 'with parameter rich_rules' do
    let(:params) do
      {
        rich_rules:
        {
          'Accept SSH from Gondor' =>
            {
              'ensure' => 'present',
              'zone'   => 'restricted',
              'source'  => '192.162.1.0/22',
              'service' => 'ssh',
              'action'  => 'accept'
            }
        }
      }
    end

    it do
      should contain_firewalld_rich_rule('Accept SSH from Gondor')
        .with_ensure('present')
        .with_zone('restricted')
        .that_notifies('Exec[firewalld::reload]')
        .that_requires('Service[firewalld]')
    end
  end

  context 'with parameter custom_service' do
    let(:params) do
      {
        'custom_services' =>
        {
          'MyService' =>
            {
              'ensure' => 'present',
              'short' => 'MyService',
              'description' => 'My Custom service',
              'port' => [
                { 'port' => '1234', 'protocol' => 'tcp' },
                { 'port' => '1234', 'protocol' => 'udp' }
              ]
            }
        }
      }
    end

    it do
      should contain_firewalld__custom_service('MyService')
        .with_ensure('present')
        .with_short('MyService')
        .with_port([{ 'port' => '1234', 'protocol' => 'tcp' }, { 'port' => '1234', 'protocol' => 'udp' }])
    end
  end

  context 'with default_zone' do
    let(:params) do
      {
        :default_zone => 'public'
      }
    end

    it do
      should contain_exec('firewalld::set_default_zone').with(
        :command => 'firewall-cmd --set-default-zone public',
        :unless  => '[ $(firewall-cmd --get-default-zone) = public ]',
      ).that_requires('Exec[firewalld::reload]')
    end
  end

  [ 'unicast', 'broadcast', 'multicast', 'all', 'off' ].each do |cond|
    context "with log_denied set to #{cond}" do
      let(:params) do
        {
          :log_denied => cond
        }
      end

      it do
        should contain_exec('firewalld::set_log_denied').with(
          :command => "firewall-cmd --set-log-denied #{cond} && firewall-cmd --reload",
          :unless => "[ \$\(firewall-cmd --get-log-denied) = #{cond} ]"
        )
      end
    end
  end

end
