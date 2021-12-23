Puppet::Type.newtype(:alternative_entry) do
  ensurable

  newparam(:name) do
    desc 'The path to the actual alternative'

    isnamevar

    validate do |path|
      raise ArgumentError, 'path must be a fully qualified path' unless absolute_path? path
    end
  end

  newparam(:altlink) do
    desc 'The name of the generic symlink for this alternative entry'

    isnamevar

    validate do |path|
      raise ArgumentError, 'path must be a fully qualified path' unless absolute_path? path
    end
  end

  newproperty(:altlink_ro, readonly: true) do
    desc 'The name of the generic symlink for this alternative entry as parameter. This is readonly.'
  end

  newproperty(:altname) do
    desc 'The name of symlink in the alternatives directory'
  end

  newproperty(:priority) do
    desc 'The value of the priority of this alternative'

    validate do |prio|
      begin
        Integer(prio)
      rescue
        raise ArgumentError, 'priority must be an integer'
      end
    end
  end

  def self.title_patterns
    [
      [
        %r{^([^:]+)$},
        [
          [:name]
        ]
      ],
      [
        %r{^(.*):([a-z]:(/|\\).*)$}i,
        [
          [:name],
          [:altlink]
        ]
      ],
      [
        %r{^(.*):(.*)$},
        [
          [:name],
          [:altlink]
        ]
      ]
    ]
  end
end
