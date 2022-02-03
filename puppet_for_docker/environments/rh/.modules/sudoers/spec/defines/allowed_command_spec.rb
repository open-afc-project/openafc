require 'spec_helper'

describe 'sudoers::allowed_command', :type => :define do
  let(:title) { 'run-riak-admin-as-riak' }
  let(:require_exist) { true }
  let(:user_group_spec) {{
    :user => 'ALL'
  }}
  let(:params) {
    {
      :command          => '/usr/sbin/riak-admin',
      :run_as           => 'riak',
      :require_password => false,
      :require_exist    => require_exist,
    }.merge(user_group_spec)
  }
  it {
    should contain_file('/etc/sudoers.d/run-riak-admin-as-riak').with(
      :content => "ALL ALL=(riak) NOPASSWD: /usr/sbin/riak-admin\n"
    )
  }
  context 'when user is specified' do
    let(:user_group_spec) {{
      :user => 'phinze'
    }}

    it 'requires the user' do
      should contain_file('/etc/sudoers.d/run-riak-admin-as-riak').with(
        :require => 'User[phinze]'
      )
    end

    context 'when require_exist = false' do
      let(:require_exist) { false }

      it 'does not require user' do
        should contain_file('/etc/sudoers.d/run-riak-admin-as-riak').with(
          :require => nil
        )
      end
    end
  end

  context 'when group is specified' do
    let(:user_group_spec) {{
      :group => 'admins'
    }}

    it 'requires the group' do
      should contain_file('/etc/sudoers.d/run-riak-admin-as-riak').with(
        :require => 'Group[admins]'
      )
    end

    context 'when require_exist = false' do
      let(:require_exist) { false }

      it 'does not require group' do
        should contain_file('/etc/sudoers.d/run-riak-admin-as-riak').with(
          :require => nil
        )
      end
    end
  end

end
