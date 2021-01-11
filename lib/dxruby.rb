major, minor, micro, = RUBY_VERSION.split(/\./)
if /x64/ =~ RUBY_PLATFORM
    require "#{major}.#{minor}_x64/dxruby.so"
else
    require "#{major}.#{minor}/dxruby.so"
end
