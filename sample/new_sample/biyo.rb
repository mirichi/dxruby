#!ruby -Ks
require 'dxruby'

font = Font.new(100)
image = Image.new(100,100).draw_font(0,0,"‚ ",font)

x = mx = 299
y = my = 299
dx = 0
dy = 0
Window.loop do
  Window.draw_morph(200,200,299,200,x,y,200,299,image, :dividex=>10, :dividey=>10)
  if Input.key_down?(K_SPACE)
    x += 1
    y += 1
  else
    dx += (mx - x) / 10
    dy += (my - y) / 10
    dx *= 0.95
    dy *= 0.95
    x += dx
    y += dy
  end
  break if Input.key_push?(K_ESCAPE)
end
