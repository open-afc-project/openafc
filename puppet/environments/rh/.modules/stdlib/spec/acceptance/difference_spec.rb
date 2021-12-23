require 'spec_helper_acceptance'

describe 'difference function' do
  describe 'success' do
    pp = <<-DOC
      $a = ['a','b','c']
      $b = ['b','c','d']
      $c = ['a']
      $o = difference($a, $b)
      if $o == $c {
        notify { 'output correct': }
      }
    DOC
    it 'returns non-duplicates in the first array' do
      apply_manifest(pp, :catch_failures => true) do |r|
        expect(r.stdout).to match(%r{Notice: output correct})
      end
    end
  end
  describe 'failure' do
    it 'handles non-array arguments'
    it 'handles improper argument counts'
  end
end
