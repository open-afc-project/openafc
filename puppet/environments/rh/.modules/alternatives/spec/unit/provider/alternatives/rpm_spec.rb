require 'spec_helper'

describe Puppet::Type.type(:alternatives).provider(:rpm) do
  def my_fixture_alternatives
    Dir.glob(File.join(PROJECT_ROOT, 'spec', 'fixtures', 'unit', 'provider', 'alternatives', 'rpm', 'alternatives', '*'))
  end

  def my_fixture_display(type, path)
    File.join(PROJECT_ROOT, 'spec', 'fixtures', 'unit', 'provider', 'alternatives', 'rpm', type, path)
  end

  def my_fixture_read(type, path)
    File.read(my_fixture_display(type, path))
  end

  let(:stub_selections) do
    {
      'sample'  => { path: '/opt/sample1' },
      'testcmd' => { path: '/opt/testcmd1' }
    }
  end

  describe '.all' do
    it 'List all alternatives in folder /var/lib/alternatives' do
      described_class.expects(:list_alternatives).returns my_fixture_alternatives
      described_class.expects(:update).with('--display', 'sample').returns my_fixture_read('display', 'sample')
      described_class.expects(:update).with('--display', 'testcmd').returns my_fixture_read('display', 'testcmd')
      described_class.all
    end

    describe 'returning data' do
      before do
        described_class.stubs(:list_alternatives).returns my_fixture_alternatives
        described_class.stubs(:update).with('--display', 'sample').returns my_fixture_read('display', 'sample')
        described_class.stubs(:update).with('--display', 'testcmd').returns my_fixture_read('display', 'testcmd')
      end

      subject { described_class.all }

      it { is_expected.to be_a Hash }
      it { expect(subject['sample']).to eq(mode: 'manual', path: '/opt/sample2') }
      it { expect(subject['testcmd']).to eq(mode: 'manual', path: '/opt/testcmd1') }
    end
  end

  describe '.instances' do
    it 'delegates to .all' do
      described_class.expects(:all).returns(stub_selections)
      described_class.expects(:new).twice.returns(stub)
      described_class.instances
    end
  end

  describe 'instances' do
    subject { described_class.new(name: 'sample') }

    let(:resource) { Puppet::Type.type(:alternatives).new(name: 'sample') }

    before do
      Puppet::Type.type(:alternatives).stubs(:defaultprovider).returns described_class
      described_class.stubs(:update).with('--display', 'sample').returns my_fixture_read('display', 'sample')
      described_class.stubs(:update).with('--display', 'testcmd').returns my_fixture_read('display', 'testcmd')
      resource.provider = subject
      described_class.stubs(:all).returns(stub_selections)
    end

    it '#path retrieves the path from class.all' do
      expect(subject.path).to eq('/opt/sample1')
    end

    it '#path= updates the path with alternatives --set' do
      subject.expects(:update).with('--set', 'sample', '/opt/sample1')
      subject.path = '/opt/sample1'
    end

    it '#mode=(:auto) calls alternatives --auto' do
      subject.expects(:update).with('--auto', 'sample')
      subject.mode = :auto
    end

    it '#mode=(:manual) calls alternatives --set with current value' do
      subject.expects(:path).returns('/opt/sample2')
      subject.expects(:update).with('--set', 'sample', '/opt/sample2')
      subject.mode = :manual
    end
  end
end
