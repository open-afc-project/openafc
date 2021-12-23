module Puppet::Parser::Functions
  newfunction(:validate, :type => :rvalue) do |args|
    content = args[0]
    checkscript = args[1]

    # Test content in a temporary file
    tmpfile = Tempfile.new("puppet")
    tmpfile.write(content)
    tmpfile.close
    checkcmd=checkscript+" "+tmpfile.path
    system(checkcmd)
    if $?==0
           return(content)
    end
    raise Puppet::ParseError, "could not validate content with command "+checkscript
  end
end
