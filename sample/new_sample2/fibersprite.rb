require 'dxruby'

module FiberSprite
  def initialize(x=0,y=0,image=nil)
    super
    @fiber = Fiber.new do
      self.fiber_proc
    end
  end

  def update
    @fiber.resume
    super
  end

  def wait(t=1)
    t.times {Fiber.yield}
  end

  def add_x(x, t=1)
    t.times {self.x += x;Fiber.yield}
  end

  def add_y(y, t=1)
    t.times {self.y += y;Fiber.yield}
  end
end

class Enemy < Sprite
  include FiberSprite

  @@image = Image.load_tiles("new_sample/image/enemy2.png", 4, 1)

  def self.objects=(ary)
    @@objects = ary
  end

  def initialize(x=0, y=0)
    super(x, y, @@image[0])
    @angle = 90
  end

  def fire(angle)
    @@objects << Shot.new(self.x+20, self.y+@@image[0].height/2+20, angle)
    @@objects << Shot.new(self.x+108, self.y+@@image[0].height/2+20, angle)
  end

  def fiber_proc
    loop do
      fire(@angle)
      @angle += 15
      wait 5
    end
  end
end

class Shot < Sprite
  @@image = Image.load("new_sample/image/enemyshot2.png")

  def self.objects=(ary)
    @@objects = ary
  end

  def initialize(x=0, y=0, angle=0)
    super(x-@@image.width/2, y-@@image.height/2, @@image)
    @dx = Math.cos(angle / 180.0 * Math::PI) * 3.5
    @dy = Math.sin(angle / 180.0 * Math::PI) * 3.5
  end

  def update
    self.x += @dx
    self.y += @dy
    if self.x < -@@image.width or self.x > 640 or self.y < -@@image.height or self.y > 480
      vanish
    end
  end
end

objs = []
Enemy.objects = objs
Shot.objects = objs

objs << Enemy.new(260,160)

Window.loop do
  Sprite.update(objs)
  Sprite.draw(objs)
  Sprite.clean(objs)
end
