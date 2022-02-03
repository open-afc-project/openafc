Puppet::Type.type(:alternatives).provide(:dpkg) do
  confine osfamily: :debian
  defaultfor operatingsystem: [:debian, :ubuntu]

  commands update: 'update-alternatives'

  has_feature :mode

  # Return all instances for this provider
  #
  # @return [Array<Puppet::Type::Alternatives::ProviderDpkg>] A list of all current provider instances
  def self.instances
    all.map { |name, attributes| new(name: name, path: attributes[:path]) }
  end

  # Generate a hash of hashes containing a link name and associated properties
  #
  # This is structured as {'key' => {attributes}} to do fast lookups on entries
  #
  # @return [Hash<String, Hash<Symbol, String>>]
  def self.all
    output = update('--get-selections')
    # Ruby 1.8.7 does not have each_with_object
    # rubocop:disable Style/EachWithObject
    output.split(%r{\n}).reduce({}) do |hash, line|
      # rubocop:enable Style/EachWithObject
      name, mode, path = line.split(%r{\s+})
      hash[name] = { path: path, mode: mode }
      hash
    end
  end

  # Retrieve the current path link
  def path
    name = @resource.value(:name)
    # rubocop:disable Style/GuardClause
    if (attrs = self.class.all[name])
      # rubocop:enable Style/GuardClause
      attrs[:path]
    end
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
