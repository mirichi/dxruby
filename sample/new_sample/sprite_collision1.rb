#!ruby -Ks
require "dxruby"

class CollisionObject < Sprite
  def initialize(image)
    super(rand(Window.width-image.width), rand(Window.height-image.height), image)
    @hit = false
    @dx = (rand(4)*2-3)/4.0
    @dy = (rand(4)*2-3)/4.0
  end

  def update
    self.x += @dx
    self.y += @dy
    @dx = -@dx if self.x <= 0 or self.x >= Window.width-self.image.width
    @dy = -@dy if self.y <= 0 or self.y >= Window.height-self.image.height
  end
end

# ‚µ‚©‚­
class Box < CollisionObject
  @@image1 = Image.new(30, 30, [255, 200, 0, 0])
  @@image2 = Image.new(30, 30, [255, 255, 255])
  def initialize
    super(@@image1)
#    @collision = [0, 0, 29, 29]
    self.scale_x = rand(2.0)+1
    self.scale_y = rand(2.0)+1
    self.center_x = rand(self.image.width)
    self.center_y = rand(self.image.height)
    self.angle = rand(360)
    @rot_speed = (rand(2.0)+1)*2-3
  end

  def update
    super
    self.angle += @rot_speed
    self.image = @@image1
  end

  def hit(d)
    self.image = @@image2
  end
end

# ‚µ‚©‚­2
class Box2 < CollisionObject
  @@image1 = Image.new(30, 30, [255, 200, 0, 0])
  @@image2 = Image.new(30, 30, [255, 255, 255])
  def initialize
    super(@@image1)
#    @collision = [0, 0, 29, 29]
  end
  def update
    super
    self.image = @@image1
  end

  def hit(d)
    self.image = @@image2
  end
end

# ‚Ü‚é
class Circle < CollisionObject
  @@image1 = Image.new(30, 30).circle_fill(15, 15, 15, [255, 200, 0])
  @@image2 = Image.new(30, 30).circle_fill(15, 15, 15, [255, 255, 255])
  def initialize
    super(@@image1)
    self.collision = [15, 15, 15]
    self.scale_x = rand(2.0)+1
    self.scale_y = rand(2.0)+1
    self.center_x = rand(self.image.width)
    self.center_y = rand(self.image.height)
    self.angle = rand(360)
    @rot_speed = (rand(2.0)+1)*2-3
  end
  def update
    super
    self.angle += @rot_speed
    self.image = @@image1
  end
  def hit(d)
    self.image = @@image2
  end
end

# ‚Ü‚é2
class Circle2 < CollisionObject
  @@image1 = Image.new(30, 30).circle_fill(15, 15, 15, [255, 200, 0])
  @@image2 = Image.new(30, 30).circle_fill(15, 15, 15, [255, 255, 255])
  def initialize
    super(@@image1)
    self.collision = [15, 15, 15]
  end
  def update
    super
    self.image = @@image1
  end
  def hit(d)
    self.image = @@image2
  end
end

# ‚³‚ñ‚©‚­
class Triangle < CollisionObject
  @@image1 = Image.new(30, 30).triangle_fill(15,0,29,29,0,29,C_GREEN)
  @@image2 = Image.new(30, 30).triangle_fill(15,0,29,29,0,29,C_WHITE)
  def initialize
    super(@@image1)
    self.collision = [15,0,29,29,0,29]
    self.scale_x = rand(2.0)+1
    self.scale_y = rand(2.0)+1
    self.center_x = rand(self.image.width)
    self.center_y = rand(self.image.height)
    self.angle = rand(360)
    @rot_speed = (rand(2.0)+1)*2-3
  end

  def update
    super
    self.angle += @rot_speed
    self.image = @@image1
  end
  def hit(d)
    self.image = @@image2
  end
end

# ‚³‚ñ‚©‚­2
class Triangle2 < CollisionObject
  @@image1 = Image.new(30, 30).triangle_fill(15,0,29,29,0,29,C_GREEN)
  @@image2 = Image.new(30, 30).triangle_fill(15,0,29,29,0,29,C_WHITE)
  def initialize
    super(@@image1)
    self.collision = [15,0,29,29,0,29]
  end
  def update
    super
    self.image = @@image1
  end
  def hit(d)
    self.image = @@image2
  end
end

# ‚Ù‚µ
class Star < CollisionObject
  @@image1 = Image.new(100,100)
  @@image2 = Image.new(100,100)

  bx = 0
  by = -20

  x1 = []
  y1 = []
  (0..4).each do |i|
    x1[i] = bx * Math.cos(Math::PI / 180 * (i * 72 - 36)) - by * Math.sin(Math::PI / 180 * (i * 72 - 36)) + @@image1.width / 2
    y1[i] = bx * Math.sin(Math::PI / 180 * (i * 72 - 36)) + by * Math.cos(Math::PI / 180 * (i * 72 - 36)) + @@image1.height / 2
  end
  sx = x1[0] + (x1[1] - x1[0]) / 2 - @@image1.width / 2
  sy = y1[0] - (x1[1] - x1[0]) / 2 * Math.tan(Math::PI / 180 * 72) - @@image1.height / 2
  x2 = []
  y2 = []
  (0..4).each do |i|
    x2[i] = sx * Math.cos(Math::PI / 180 * i * 72) - sy * Math.sin(Math::PI / 180 * i * 72) + @@image1.width / 2
    y2[i] = sx * Math.sin(Math::PI / 180 * i * 72) + sy * Math.cos(Math::PI / 180 * i * 72) + @@image1.height / 2
  end
  @@cols = [[x2[0], y2[0], x1[2], y1[2], x1[4], y1[4]],
            [x2[1], y2[1], x1[3], y1[3], x1[0], y1[0]],
            [x2[2], y2[2], x1[4], y1[4], x1[1], y1[1]],
            [x2[3], y2[3], x1[0], y1[0], x1[2], y1[2]],
            [x2[4], y2[4], x1[1], y1[1], x1[3], y1[3]]]

  @@cols.each do |ary|
    @@image1.triangle_fill(ary[0], ary[1], ary[2], ary[3], ary[4], ary[5], C_CYAN)
    @@image2.triangle_fill(ary[0], ary[1], ary[2], ary[3], ary[4], ary[5], C_WHITE)
  end

  def initialize
    super(@@image1)
    self.collision = @@cols
  end
  def update
    super
    self.image = @@image1
    self.angle += 1
  end
  def hit(d)
    self.image = @@image2
  end
end



Window.width, Window.height = 800, 600

font = Font.new(24)

object = Array.new(20) {Box.new} + Array.new(20) {Box2.new} +
         Array.new(20) {Triangle.new} + Array.new(20) {Triangle2.new} +
         Array.new(20) {Circle.new} + Array.new(20) {Circle2.new} +
         Array.new(5) {Star.new}

Window.loop do
  Sprite.update(object)

  Sprite.check(object)

  Sprite.draw(object)

  break if Input.keyPush?(K_ESCAPE)

  Window.drawFont(0, 0, Window.fps.to_s + " fps", font)
  Window.drawFont(0, 24, Window.getLoad.to_i.to_s + " %", font)
end
