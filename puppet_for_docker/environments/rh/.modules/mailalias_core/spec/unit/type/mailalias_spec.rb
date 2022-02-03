require 'spec_helper'

describe Puppet::Type.type(:mailalias) do
  include PuppetSpec::Files

  let :tmpfile_path do
    tmpfile('afile')
  end

  let :target do
    tmpfile('mailalias')
  end

  let :recipient_resource do
    described_class.new(name: 'luke', recipient: 'yay', target: target)
  end

  let :file_resource do
    described_class.new(name: 'lukefile', file: tmpfile_path, target: target)
  end

  it 'is initially absent as a recipient' do
    expect(recipient_resource.retrieve_resource[:recipient]).to eq(:absent)
  end

  it 'is initially absent as an included file' do
    expect(file_resource.retrieve_resource[:file]).to eq(:absent)
  end

  it 'tries and set the recipient when it does the sync' do
    expect(recipient_resource.retrieve_resource[:recipient]).to eq(:absent)
    expect(recipient_resource.property(:recipient)).to receive(:set).with(['yay'])
    recipient_resource.property(:recipient).sync
  end

  it 'tries and set the included file when it does the sync' do
    expect(file_resource.retrieve_resource[:file]).to eq(:absent)
    expect(file_resource.property(:file)).to receive(:set).with(tmpfile_path)
    file_resource.property(:file).sync
  end

  it 'fails when file is not an absolute path' do
    expect {
      Puppet::Type.type(:mailalias).new(name: 'x', file: 'afile')
    }.to raise_error Puppet::Error, %r{File paths must be fully qualified}
  end

  it 'fails when both file and recipient are specified' do
    expect {
      Puppet::Type.type(:mailalias).new(name: 'x', file: tmpfile_path,
                                        recipient: 'foo@example.com')
    }.to raise_error Puppet::Error, %r{cannot specify both a recipient and a file}
  end
end
