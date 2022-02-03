Puppet::Type.newtype(:alternatives) do
  feature :mode, 'The alternative can provide auto and manual modes'

  newparam(:name, isnamevar: true) do
    desc 'The name of the alternative.'
  end

  newproperty(:path) do
    desc 'The path of the desired source for the given alternative'

    validate do |path|
      raise ArgumentError, 'path must be a fully qualified path' unless absolute_path? path
    end
  end

  newproperty(:mode, required_features: [:mode]) do
    desc 'Use the automatic option for this alternative'

    newvalue('auto')
    newvalue('manual')
  end

  # Turns out this isn't a valid hook.
  # validate do
  #  case self[:mode]
  #  when 'auto'
  #    raise ArgumentError, "Mode cannot be 'auto' if a path is given" if self[:path]
  #  when 'manual'
  #    raise ArgumentError, "Mode cannot be 'manual' without a path" unless self[:path]
  #  end
  # end

  autorequire(:alternative_entry) do
    self[:path]
  end
end
