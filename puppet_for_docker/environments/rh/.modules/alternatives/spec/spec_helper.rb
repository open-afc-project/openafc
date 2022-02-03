dir = File.expand_path(File.join(File.dirname(__FILE__), '..'))
$LOAD_PATH.unshift(dir, dir + 'lib', dir + '../lib')

require 'mocha/api'
require 'puppet'

if Dir.exist?(File.expand_path('../../lib', __FILE__)) && RUBY_VERSION !~ %r{^1.9}
  require 'coveralls'
  require 'simplecov'
  require 'simplecov-console'
  SimpleCov.formatters = [
    SimpleCov::Formatter::HTMLFormatter,
    SimpleCov::Formatter::Console,
    Coveralls::SimpleCov::Formatter
  ]
  SimpleCov.start do
    track_files 'lib/**/*.rb'
    add_filter '/spec'
    add_filter '/vendor'
    add_filter '/.vendor'
  end
end

PROJECT_ROOT = File.expand_path('..', File.dirname(__FILE__))

RSpec.configure do |config|
  config.mock_with :mocha
end
