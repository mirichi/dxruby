require 'dxruby'

Window.width = 300
Window.height = 300

bg = Image.new(300,300,C_BLUE)
image = Image.new(300,900)
repeat_x = 3
repeat_y = 9
octave = 5
subsistence = 0.4
w = 1/image.width.to_f*repeat_x
h = 1/image.height.to_f*repeat_y

Image.perlin_seed(1.hash)

image.height.times do |y|
  image.width.times do |x|
    c = (Image.octave_perlin_noise(x*w,
                                  y*h,
                                  1, octave, subsistence, repeat_x, repeat_y) * 255 - 70) * 2.2
    image[x, y] = [c>255?255:c, 255, 255, 255] if c>0
  end
end
y = 0
Window.loop do
  Window.draw(0,0,bg)
  Window.draw_tile(0,0,[[0]], [image], nil, y, nil, nil)
  y-=5
 break if Input.key_push?(K_ESCAPE)
end


