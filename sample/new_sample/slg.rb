require 'dxruby'

map_image = [
  Image.new(32,32,[128,128,128]),
  Image.new(32,32,[100,230,100]),
  Image.new(32,32,[0,200,0]),
  Image.new(32,32,[255,220,100]),
  Image.new(32,32,[255,100,0]),
  Image.new(32,32,[0,0,255])
]

$map = [
  [1,3,3,3,1,0,1,2,2,2,2,4,4,4,4,4],
  [3,3,3,3,2,0,1,1,2,2,2,4,4,4,4,4],
  [1,3,3,3,1,0,2,1,1,2,2,2,2,2,2,1],
  [5,1,3,3,1,0,1,1,1,1,1,1,2,2,2,2],
  [5,1,3,3,1,0,1,2,1,1,1,2,1,1,1,1],
  [5,1,2,3,1,0,0,1,1,2,1,1,1,1,2,1],
  [5,5,1,1,1,1,0,0,0,0,0,1,1,2,1,1],
  [5,5,5,5,5,1,1,1,1,1,0,0,0,0,1,1],
  [5,5,5,5,5,5,5,5,5,1,1,1,1,0,0,2],
  [5,5,5,5,1,1,1,1,5,5,5,5,1,1,0,1],
  [5,5,5,1,1,1,2,1,1,1,1,5,5,5,0,5],
  [5,5,5,1,2,4,4,4,4,4,1,1,2,1,0,1],
]

x = 5
y = 5
s = Sprite.new(x*32, y*32, Image.new(32,32).circle_fill(16,16,16,C_WHITE))
speed = 9

$d = [[0,0],[0,0],[0,1],[0,0],[-1,0],[0,0],[1,0],[0,0],[0,-1],[0,0]]

def find(x, y, speed, temp)
  [2,4,6,8].each do |i|
    tx = x + $d[i][0]
    ty = y + $d[i][1]
    next if tx < 0 or tx > 15 or ty < 0 or ty > 11

    if speed - $map[ty][tx] - 1 >= 0
      if temp[ty][tx] == nil or temp[ty][tx] < speed - $map[ty][tx] - 1
        temp[ty][tx] = speed - $map[ty][tx] - 1
        find(tx, ty, speed - $map[ty][tx] - 1, temp)
      end
    end
  end
end

range = Image.new(32,32,[150,255,180,180])
cursor = Image.new(32,32).box(0,0,31,31,C_WHITE)

temp = Array.new(12) { Array.new(15) }
find(x, y, speed, temp)

Window.loop do

  if Input.mouse_push?(M_LBUTTON)
    if temp[Input.mouse_pos_y / 32][Input.mouse_pos_x / 32]
      x = Input.mouse_pos_x / 32
      y = Input.mouse_pos_y / 32
      temp = Array.new(12) { Array.new(15) }
      find(x, y, speed, temp)
      s.x = x * 32
      s.y = y * 32
    end
  end

  Window.draw_tile(0,0,$map,map_image,0,0,16,12)

  (0..11).each do |iy|
    (0..15).each do |ix|
      if temp[iy][ix]
        Window.draw(ix*32, iy*32, range)
      end
    end
  end

  s.draw
  Window.draw((Input.mouse_pos_x / 32) * 32, (Input.mouse_pos_y / 32) * 32, cursor)

  break if Input.key_push?(K_ESCAPE)
end
