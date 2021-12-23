require 'spec_helper'

describe Puppet::Type.type(:alternatives).provider(:dpkg) do
  def my_fixture(path)
    File.join(PROJECT_ROOT, 'spec', 'fixtures', 'unit', 'provider', 'alternatives', 'dpkg', path)
  end

  def my_fixture_read(path)
    File.read(my_fixture(path))
  end

  let(:stub_selections) do
    {
      'editor'   => { mode: 'manual', path: '/usr/bin/vim.tiny' },
      'aptitude' => { mode: 'auto', path: '/usr/bin/aptitude-curses' }
    }
  end

  describe '.all' do
    it 'calls update-alternatives --get-selections' do
      described_class.expects(:update).with('--get-selections').returns my_fixture_read('get-selections')
      described_class.all
    end

    describe 'returning data' do
      before do
        described_class.stubs(:update).with('--get-selections').returns my_fixture_read('get-selections')
      end

      subject { described_class.all }

      it { is_expected.to be_a Hash }
      it { expect(subject['editor']).to eq(mode: 'manual', path: '/usr/bin/vim.tiny') }
      it { expect(subject['aptitude']).to eq(mode: 'auto', path: '/usr/bin/aptitude-curses') }
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
    subject { described_class.new(name: 'editor') }

    let(:resource) { Puppet::Type.type(:alternatives).new(name: 'editor') }

    before do
      Puppet::Type.type(:alternatives).stubs(:defaultprovider).returns described_class
      resource.provider = subject
      described_class.stubs(:all).returns(stub_selections)
    end

    it '#path retrieves the path from class.all' do
      expect(subject.path).to eq('/usr/bin/vim.tiny')
    end

    it '#path= updates the path with update-alternatives --set' do
      subject.expects(:update).with('--set', 'editor', '/bin/nano')
      subject.path = '/bin/nano'
    end

    it '#mode=(:auto) calls update-alternatives --auto' do
      subject.expects(:update).with('--auto', 'editor')
      subject.mode = :auto
    end

    it '#mode=(:manual) calls update-alternatives --set with current value' do
      subject.expects(:path).returns('/usr/bin/vim.tiny')
      subject.expects(:update).with('--set', 'editor', '/usr/bin/vim.tiny')
      subject.mode = :manual
    end
  end
end
