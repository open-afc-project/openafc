require 'spec_helper'

describe 'systemd' do
  context 'supported operating systems' do
    on_supported_os.each do |os, facts|
      context "on #{os}" do
        let(:facts) { facts }

        it { is_expected.to compile.with_all_deps }
        it { is_expected.to create_class('systemd') }
        it { is_expected.to create_class('systemd::systemctl::daemon_reload') }
        it { is_expected.to contain_class('systemd::journald') }
        it { is_expected.to create_service('systemd-journald') }
        it { is_expected.to have_ini_setting_resource_count(0) }
        it { is_expected.not_to create_service('systemd-resolved') }
        it { is_expected.not_to create_service('systemd-networkd') }
        it { is_expected.not_to create_service('systemd-timesyncd') }

        context 'when enabling resolved and networkd' do
          let(:params) do
            {
              manage_resolved: true,
              manage_networkd: true,
            }
          end

          it { is_expected.to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.to create_service('systemd-resolved').with_enable(true) }
          it { is_expected.to create_service('systemd-networkd').with_ensure('running') }
          it { is_expected.to create_service('systemd-networkd').with_enable(true) }
        end

        context 'when enabling resolved with DNS values (string)' do
          let(:params) do
            {
              manage_resolved: true,
              dns: '8.8.8.8 8.8.4.4',
              fallback_dns: '2001:4860:4860::8888 2001:4860:4860::8844',
            }
          end

          it { is_expected.to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.to create_service('systemd-resolved').with_enable(true) }
          it { is_expected.to contain_ini_setting('dns') }
          it { is_expected.to contain_ini_setting('fallback_dns') }
          it { is_expected.not_to contain_ini_setting('domains') }
          it { is_expected.not_to contain_ini_setting('multicast_dns') }
          it { is_expected.not_to contain_ini_setting('llmnr') }
          it { is_expected.not_to contain_ini_setting('dnssec') }
          it { is_expected.not_to contain_ini_setting('dnsovertls') }
          it { is_expected.not_to contain_ini_setting('cache') }
          it { is_expected.not_to contain_ini_setting('dns_stub_listener') }
        end

        context 'when enabling resolved with DNS values (array)' do
          let(:params) do
            {
              manage_resolved: true,
              dns: ['8.8.8.8', '8.8.4.4'],
              fallback_dns: ['2001:4860:4860::8888', '2001:4860:4860::8844'],
            }
          end

          it { is_expected.to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.to create_service('systemd-resolved').with_enable(true) }
          it { is_expected.to contain_ini_setting('dns') }
          it { is_expected.to contain_ini_setting('fallback_dns') }
          it { is_expected.not_to contain_ini_setting('domains') }
          it { is_expected.not_to contain_ini_setting('multicast_dns') }
          it { is_expected.not_to contain_ini_setting('llmnr') }
          it { is_expected.not_to contain_ini_setting('dnssec') }
          it { is_expected.not_to contain_ini_setting('dnsovertls') }
          it { is_expected.not_to contain_ini_setting('cache') }
          it { is_expected.not_to contain_ini_setting('dns_stub_listener') }
        end

        context 'when enabling resolved with DNS values (full)' do
          let(:params) do
            {
              manage_resolved: true,
              dns: ['8.8.8.8', '8.8.4.4'],
              fallback_dns: ['2001:4860:4860::8888', '2001:4860:4860::8844'],
              domains: ['2001:4860:4860::8888', '2001:4860:4860::8844'],
              llmnr: true,
              multicast_dns: false,
              dnssec: false,
              dnsovertls: 'no',
              cache: true,
              dns_stub_listener: 'udp',
            }
          end

          it { is_expected.to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.to create_service('systemd-resolved').with_enable(true) }
          it { is_expected.to contain_ini_setting('dns') }
          it { is_expected.to contain_ini_setting('fallback_dns') }
          it { is_expected.to contain_ini_setting('domains') }
          it { is_expected.to contain_ini_setting('multicast_dns') }
          it { is_expected.to contain_ini_setting('llmnr') }
          it { is_expected.to contain_ini_setting('dnssec') }
          it { is_expected.to contain_ini_setting('dnsovertls') }
          it {
            is_expected.to contain_ini_setting('cache').with(
              path: '/etc/systemd/resolved.conf',
              value: 'yes',
            )
          }
          it { is_expected.to contain_ini_setting('dns_stub_listener') }
        end

        context 'when enabling resolved with no-negative cache variant' do
          let(:params) do
            {
              manage_resolved: true,
              cache: 'no-negative',
            }
          end

          it { is_expected.to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.to create_service('systemd-resolved').with_enable(true) }
          it {
            is_expected.to contain_ini_setting('cache').with(
              path: '/etc/systemd/resolved.conf',
              value: 'no-negative',
            )
          }
        end

        context 'when enabling timesyncd' do
          let(:params) do
            {
              manage_timesyncd: true,
            }
          end

          it { is_expected.to create_service('systemd-timesyncd').with_ensure('running') }
          it { is_expected.to create_service('systemd-timesyncd').with_enable(true) }
          it { is_expected.not_to create_service('systemd-resolved').with_ensure('running') }
          it { is_expected.not_to create_service('systemd-resolved').with_enable(true) }
          it { is_expected.not_to create_service('systemd-networkd').with_ensure('running') }
          it { is_expected.not_to create_service('systemd-networkd').with_enable(true) }
        end

        context 'when enabling timesyncd with NTP values (string)' do
          let(:params) do
            {
              manage_timesyncd: true,
              ntp_server: '0.pool.ntp.org 1.pool.ntp.org',
              fallback_ntp_server: '2.pool.ntp.org 3.pool.ntp.org',
            }
          end

          it { is_expected.to compile.with_all_deps }
          it { is_expected.to contain_ini_setting('ntp_server') }
          it { is_expected.to contain_ini_setting('fallback_ntp_server') }
        end

        context 'when enabling timesyncd with NTP values (array)' do
          let(:params) do
            {
              manage_timesyncd: true,
              ntp_server: ['0.pool.ntp.org', '1.pool.ntp.org'],
              fallback_ntp_server: ['2.pool.ntp.org', '3.pool.ntp.org'],
            }
          end

          it { is_expected.to compile.with_all_deps }
          it { is_expected.to contain_ini_setting('ntp_server') }
          it { is_expected.to contain_ini_setting('fallback_ntp_server') }
        end

        context 'when passing service limits' do
          let(:params) do
            {
              service_limits: { 'openstack-nova-compute.service' => { 'limits' => { 'LimitNOFILE' => 32_768 } } },
            }
          end

          it { is_expected.to compile.with_all_deps }
          it { is_expected.to contain_systemd__service_limits('openstack-nova-compute.service').with_limits('LimitNOFILE' => 32_768) }
        end

        context 'when managing Accounting options' do
          let :params do
            {
              manage_accounting: true,
            }
          end

          it { is_expected.to contain_class('systemd::system') }

          case facts[:os]['family']
          when 'Archlinux'
            accounting = ['DefaultCPUAccounting', 'DefaultIOAccounting', 'DefaultIPAccounting', 'DefaultBlockIOAccounting', 'DefaultMemoryAccounting', 'DefaultTasksAccounting']
          when 'Debian'
            accounting = ['DefaultCPUAccounting', 'DefaultBlockIOAccounting', 'DefaultMemoryAccounting']
          when 'RedHat'
            accounting = ['DefaultCPUAccounting', 'DefaultBlockIOAccounting', 'DefaultMemoryAccounting', 'DefaultTasksAccounting']
          when 'Suse'
            accounting = ['DefaultCPUAccounting', 'DefaultBlockIOAccounting', 'DefaultMemoryAccounting', 'DefaultTasksAccounting']
          end
          accounting.each do |account|
            it { is_expected.to contain_ini_setting(account) }
          end
          it { is_expected.to compile.with_all_deps }
        end
        context 'when enabling journald with options' do
          let(:params) do
            {
              manage_journald: true,
              journald_settings: {
                'Storage'         => 'auto',
                'MaxRetentionSec' => '5day',
                'MaxLevelStore'   => {
                  'ensure' => 'absent',
                },
              },
            }
          end

          it { is_expected.to compile.with_all_deps }
          it {
            is_expected.to contain_service('systemd-journald').with(
              ensure: 'running',
            )
          }
          it { is_expected.to have_ini_setting_resource_count(3) }
          it {
            is_expected.to contain_ini_setting('Storage').with(
              path: '/etc/systemd/journald.conf',
              section: 'Journal',
              notify: 'Service[systemd-journald]',
              value: 'auto',
            )
          }
          it {
            is_expected.to contain_ini_setting('MaxRetentionSec').with(
              path: '/etc/systemd/journald.conf',
              section: 'Journal',
              notify: 'Service[systemd-journald]',
              value: '5day',
            )
          }
          it {
            is_expected.to contain_ini_setting('MaxLevelStore').with(
              path: '/etc/systemd/journald.conf',
              section: 'Journal',
              notify: 'Service[systemd-journald]',
              ensure: 'absent',
            )
          }
        end

        context 'when disabling journald' do
          let(:params) do
            {
              manage_journald: false,
            }
          end

          it { is_expected.to compile.with_all_deps }
          it { is_expected.not_to contain_service('systemd-journald') }
        end

        context 'when disabling udevd management' do
          let(:params) do
            {
              manage_udevd: false,
            }
          end

          it { is_expected.to compile.with_all_deps }
          it { is_expected.not_to contain_service('systemd-udevd') }
          it { is_expected.not_to contain_file('/etc/udev/udev.conf') }
        end

        context 'when working with udevd and no custom rules' do
          let(:params) do
            {
              manage_udevd: true,
              udev_log: 'daemon',
              udev_children_max: 1,
              udev_exec_delay: 2,
              udev_event_timeout: 3,
              udev_resolve_names: 'early',
              udev_timeout_signal: 'SIGKILL',
            }
          end

          it { is_expected.to compile.with_all_deps }
          it {
            is_expected.to contain_service('systemd-udevd')
              .with(enable: true,
                    ensure: 'running')
          }
          it {
            is_expected.to contain_file('/etc/udev/udev.conf')
              .with(ensure: 'file',
                    owner: 'root',
                    group: 'root',
                    mode: '0444')
              .with_content(%r{^udev_log=daemon$})
              .with_content(%r{^children_max=1$})
              .with_content(%r{^exec_delay=2$})
              .with_content(%r{^event_timeout=3$})
              .with_content(%r{^resolve_names=early$})
              .with_content(%r{^timeout_signal=SIGKILL$})
          }
        end

        context 'when working with udevd and a rule set' do
          let(:params) do
            {
              manage_udevd: true,
              udev_log: 'daemon',
              udev_children_max: 1,
              udev_exec_delay: 2,
              udev_event_timeout: 3,
              udev_resolve_names: 'early',
              udev_timeout_signal: 'SIGKILL',
              udev_rules: { 'example_raw.rules' => {
                'rules' => [
                  '# I am a comment',
                  'ACTION=="add", KERNEL=="sda", RUN+="/bin/raw /dev/raw/raw1 %N"',
                  'ACTION=="add", KERNEL=="sdb", RUN+="/bin/raw /dev/raw/raw2 %N"',
                ],
              } },

            }
          end

          it { is_expected.to compile.with_all_deps }
          it {
            is_expected.to contain_service('systemd-udevd')
              .with(enable: true,
                    ensure: 'running')
          }
          it {
            is_expected.to contain_file('/etc/udev/udev.conf')
              .with(ensure: 'file',
                    owner: 'root',
                    group: 'root',
                    mode: '0444')
              .with_content(%r{^udev_log=daemon$})
              .with_content(%r{^children_max=1$})
              .with_content(%r{^exec_delay=2$})
              .with_content(%r{^event_timeout=3$})
              .with_content(%r{^resolve_names=early$})
              .with_content(%r{^timeout_signal=SIGKILL$})
          }
          it {
            is_expected.to contain_systemd__udev__rule('example_raw.rules')
              .with(rules: [
                      '# I am a comment',
                      'ACTION=="add", KERNEL=="sda", RUN+="/bin/raw /dev/raw/raw1 %N"',
                      'ACTION=="add", KERNEL=="sdb", RUN+="/bin/raw /dev/raw/raw2 %N"',
                    ])
          }
        end

        context 'when enabling logind with options' do
          let(:params) do
            {
              manage_logind: true,
              logind_settings: {
                'HandleSuspendKey'  => 'ignore',
                'KillUserProcesses' => 'no',
                'KillExcludeUsers'  => ['a', 'b'],
                'RemoveIPC'         => {
                  'ensure' => 'absent',
                },
                'UserTasksMax' => '10000',
              },
              loginctl_users: {
                'foo' => { 'linger' => 'enabled' },
              },
            }
          end

          it { is_expected.to compile.with_all_deps }
          it {
            is_expected.to contain_service('systemd-logind').with(
              ensure: 'running',
            )
          }
          it { is_expected.to have_ini_setting_resource_count(5) }
          it {
            is_expected.to contain_ini_setting('HandleSuspendKey').with(
              path: '/etc/systemd/logind.conf',
              section: 'Login',
              notify: 'Service[systemd-logind]',
              value: 'ignore',
            )
          }
          it {
            is_expected.to contain_ini_setting('KillUserProcesses').with(
              path: '/etc/systemd/logind.conf',
              section: 'Login',
              notify: 'Service[systemd-logind]',
              value: 'no',
            )
          }
          it {
            is_expected.to contain_ini_setting('KillExcludeUsers').with(
              path: '/etc/systemd/logind.conf',
              section: 'Login',
              notify: 'Service[systemd-logind]',
              value: 'a b',
            )
          }
          it {
            is_expected.to contain_ini_setting('RemoveIPC').with(
              path: '/etc/systemd/logind.conf',
              section: 'Login',
              notify: 'Service[systemd-logind]',
              ensure: 'absent',
            )
          }
          it {
            is_expected.to contain_ini_setting('UserTasksMax').with(
              path: '/etc/systemd/logind.conf',
              section: 'Login',
              notify: 'Service[systemd-logind]',
              value: '10000',
            )
          }
          it { is_expected.to contain_loginctl_user('foo').with(linger: 'enabled') }
        end
        context 'when passing dropin_files' do
          let(:params) do
            {
              dropin_files: {
                'my-foo.conf' => {
                  'unit'    => 'foo.service',
                  'content' => '[Service]\nReadWritePaths=/',
                },
              },
            }
          end

          it { is_expected.to contain_systemd__dropin_file('my-foo.conf').with_content('[Service]\nReadWritePaths=/') }
        end
      end
    end
  end
end
