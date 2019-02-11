require 'dxruby'

def create_block_sprite(*ary)
  image = Image.new(640,480)
  col = []
  ary.each do |a|
    case a.size
      when 3
        image.circle_fill(a[0], a[1], a[2], C_RED)
      when 4
        image.box_fill(a[0], a[1], a[2], a[3], C_CYAN)
    end
    col << a
  end
  s = Sprite.new(0, 0, image)
  s.collision = col
  return s
end

block1 = create_block_sprite([100,100,150,380], [490,100,540,380])
block2 = create_block_sprite([100,100,150,380], [490,100,540,380])
block2.angle = 90
block3 = create_block_sprite([100,100,150,380], [490,100,540,380])
block3.angle = 45
block4 = create_block_sprite([100,100,150,380], [490,100,540,380])
block4.angle = 135
circle = create_block_sprite([320, 240, 100])
bar = create_block_sprite([300, 60, 340, 420])

objects = [block1, block2, block3, block4, circle, bar]

cursor = Sprite.new(0, 0, Image.new(10, 10).circle_fill(5, 5, 5, C_WHITE))
Input.set_mouse_pos(0, 0)

Window.loop do
  cursor.x, cursor.y = Input.mouse_pos_x, Input.mouse_pos_y
  block1.angle += 0.05
  block2.angle += 0.05
  block3.angle -= 0.1
  block4.angle -= 0.1
  bar.angle += 1
  Sprite.draw(objects)

  Input.set_mouse_pos(0, 0) if cursor === objects

  cursor.draw

  break if Input.key_push?(K_ESCAPE)
end
