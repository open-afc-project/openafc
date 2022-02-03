Puppet::Type.type(:alternatives).provide(:rpm) do
  confine osfamily: :redhat
  defaultfor osfamily: :redhat

  commands update: 'alternatives'

  has_feature :mode

  # Return all instances for this provider
  #
  # @return [Array<Puppet::Type::Alternatives::ProviderDpkg>] A list of all current provider instances
  def self.instances
    all.map { |name, attributes| new(name: name, path: attributes[:path]) }
  end

  def self.list_alternatives
    Dir.glob('/var/lib/alternatives/*')
  end

  ALT_RPM_QUERY_CURRENT_REGEX = %r{status is (\w+)\.\n\slink currently points to (.*\/[^\/]*)\n}

  # Generate a hash of hashes containing a link name and associated properties
  #
  # This is structured as {'key' => {attributes}} to do fast lookups on entries
  #
  # @return [Hash<String, Hash<Symbol, String>>]
  def self.all
    hash = {}
    list_alternatives.map { |x| File.basename(x) }.each do |name|
      begin
        # rubocop:enable Style/EachWithObject
        output = update('--display', name)
        mode = output.match(ALT_RPM_QUERY_CURRENT_REGEX)[1]
        path = output.match(ALT_RPM_QUERY_CURRENT_REGEX)[2]
        hash[name] = { path: path, mode: mode }
      rescue
        Puppet.warning format(_('Failed to parse alternatives entry %{name}'), name: name)
      end
    end
    hash
  end

  # Retrieve the current path link
  def path
    name = @resource.value(:name)
    self.class.all[name][:path]
  end

  # @param [String] newpath The path to use as the new alternative link
  def path=(newpath)
    name = @resource.value(:name)
    update('--set', name, newpath)
  end

  # @return [String] The alternative mode
  def mode
    output = update('--display', @resource.value(:name))
    first = output.split("\n").first

    if first =~ %r{auto mode}
      'auto'
    elsif first =~ %r{manual mode}
      'manual'
    elsif first =~ %r{status is auto}
      'auto'
    elsif first =~ %r{status is manual}
      'manual'
    else
      raise Puppet::Error, "Could not determine if #{self} is in auto or manual mode"
    end
  end

  # Set the mode to manual or auto.
  # @param [Symbol] newmode Either :auto or :manual for the alternatives mode
  def mode=(newmode)
    if newmode == :auto
      update('--auto', @resource.value(:name))
    elsif newmode == :manual
      # No change in value, but sets it to manual
      update('--set', name, path)
    end
  end
end
