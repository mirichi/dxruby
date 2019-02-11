require 'dxruby'

count = 0
image = Image.new(200, 200).box_fill(0, 0, 99, 99, C_RED)
                           .box_fill(100, 0, 199, 99, C_YELLOW)
                           .box_fill(0, 100, 99, 199, C_GREEN)
                           .box_fill(100, 100, 199, 199, C_BLUE)

Window.loop do
  Window.drawTile(0, 0, [[0]], [image], count, count, 5, 4)
  count += 1
  break if Input.key_push?(K_ESCAPE)
end
