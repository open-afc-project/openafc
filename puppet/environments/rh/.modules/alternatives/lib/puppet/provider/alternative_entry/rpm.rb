Puppet::Type.type(:alternative_entry).provide(:rpm) do
  confine osfamily: :redhat
  defaultfor osfamily: :redhat

  commands update: '/usr/sbin/alternatives'

  mk_resource_methods

  def create
    update('--install',
           @resource.value(:altlink),
           @resource.value(:altname),
           @resource.value(:name),
           @resource.value(:priority))
  end

  def exists?
    @property_hash[:ensure] == :present
  end

  def destroy
    # rubocop:disable Style/RedundantBegin
    begin
      # rubocop::enable Style/RedundantBegin
      update('--remove', @resource.value(:altname), @resource.value(:name))
      # rubocop:disable Lint/HandleExceptions
    rescue
      # rubocop:enable Lint/HandleExceptions
    end
  end

  def self.instances
    output = Dir.glob('/var/lib/alternatives/*').map { |x| File.basename(x) }

    entries = []

    output.each do |altname|
      query_alternative(altname).each do |alt|
        entries << new(alt) unless alt.empty?
      end
    end
    entries
  end

  def self.prefetch(resources)
    catalog = resources.values.first.catalog
    instances.each do |prov|
      catalog.resources.each do |item|
        if item.class.to_s == 'Puppet::Type::Alternative_entry' && item.name == prov.name && item.parameter('altlink').value == prov.altlink
          item.provider = prov
        end
      end
    end
  end

  ALT_RPM_QUERY_REGEX = %r{^(.*\/[^\/]*) -.*priority (\w+)$}

  def self.query_alternative(altname)
    begin
      output = update('--display', altname)
      altlink = File.readlines('/var/lib/alternatives/' + altname)[1].chomp
      output.scan(ALT_RPM_QUERY_REGEX).map do |(path, priority)|
        { altname: altname, altlink: altlink, name: path, priority: priority, altlink_ro: altlink, ensure: :present }
      end
    rescue
      Puppet.warning format(_('Failed to parse alternatives entry %{name}'), name: name)
      {}
    end
  end

  def name=(new_name)
    rebuild do
      @property_hash[:name] = new_name
    end
  end

  def altname=(new_altname)
    rebuild do
      @property_hash[:altname] = new_altname
    end
  end

  def altlink=(new_altlink)
    rebuild do
      @property_hash[:altlink] = new_altlink
    end
  end

  def priority=(new_priority)
    rebuild do
      @property_hash[:priority] = new_priority
    end
  end

  private

  def rebuild(&_block)
    destroy
    yield
    create
  end
end
