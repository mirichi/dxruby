require 'dxruby'

x1 = 0
x2 = 0
image = Image.new(100,100,C_WHITE)

proc1 = Proc.new do 
  x1 += 2
  Window.draw(x1, 100, image)
end

proc2 = Proc.new do 
  x2 += 2
  Window.draw(x2, 201, image)
end

Window.before_call[:test] = proc1
Window.after_call[:test] = proc2

Window.loop do
  break if Input.key_push?(K_ESCAPE)
end


