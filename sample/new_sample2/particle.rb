require 'dxruby'

class Sprite
  def on_screen?(range = nil)
    if range == nil
      if self.target == nil
        screen = [0, 0, Window.width, Window.height]
      else
        screen = [0, 0, self.target.width, self.target.height]
      end
    else
      screen = range
    end

    xary = [0, self.image.width, 0, self.image.width]
    yary = [0, 0, self.image.height, self.image.height]

    if self.angle != 0 or self.scale_x != 1.0 or self.scale_y != 1.0
      cos = Math.cos(self.angle / 180.0 * Math::PI)
      sin = Math.sin(self.angle / 180.0 * Math::PI)
      4.times do |i|
        tempx = ((xary[i] - self.center_x) * cos - (yary[i] - self.center_x) * sin) * scale_x + self.center_x
        tempy = ((xary[i] - self.center_x) * sin + (yary[i] - self.center_x) * cos) * scale_y + self.center_x
        xary[i] = tempx
        yary[i] = tempy
      end
    end
    return !(screen[0] >= xary.max+self.x or screen[1] >= yary.max+self.y or screen[2] <= xary.min+self.x or screen[3] <= yary.min+self.y)
  end
end

class StandardParticle < Sprite
  attr_accessor :dx, :dy, :lifetime, :parent

  def initialize
    @dx = 0
    @dy = 0
    @lifetime = nil
  end

  def update
    if @lifetime
      @lifetime -= 1
      if @lifetime == 0
        self.vanish 
        return
      end
    end

    if self.alpha > 0
      self.alpha += @parent.d_alpha
      self.alpha = 0 if self.alpha < 0
    end

    self.x += @dx
    self.y += @dy

    self.vanish unless self.on_screen?

    @dy += @parent.gravity
  end
end

class ParticleFactory
  attr_accessor :particles

  @@all_objects = []

  def self.step
    obj = @@all_objects.map{|o|o.particles}
    Sprite.update obj
    Sprite.draw obj
    Sprite.clean obj
    @@all_objects.delete_if {|v| v.particles.empty?}
  end

  def self.count
    c = 0
    @@all_objects.each do |o|
      c += o.particles.size
    end
    c
  end

  def initialize(particle_class)
    @particle_class = particle_class
    @particles = []
  end

  def generate(count)
    flag = @particles.empty?
    count.times do |i|
      temp = @particle_class.new
      yield temp, i
      @particles << temp
    end
    @@all_objects.push(self) if flag
  end
end

class SampleEmitter
  attr_accessor :gravity, :d_alpha, :image, :lifetime

  class EmitterGenerator
    attr_accessor :b, :interval, :count
    def initialize(b, interval)
      @b = b
      @interval = interval
      @count = 0
    end
  end

  def initialize(particle_class)
    @factory = ParticleFactory.new(particle_class)
    @screen = Sprite.new
    @screen.collision = [0, 0, 639, 479]
    @generator = []
    @d_alpha = 0
  end

  def generate(count)
    @factory.generate(count) do |p, i|
      p.parent = self
      p.lifetime = @lifetime
      yield p, i
    end
  end

  def generate_line(count, x1, y1, x2, y2)
    diff_x = x2 - x1 + 1
    diff_y = y2 - y1 + 1
    @factory.generate(count) do |p, i|
      r = rand()
      p.x = x1 + r * diff_x
      p.y = y1 + r * diff_y
      p.parent = self
      p.lifetime = @lifetime
      p.image = @image
      yield p, i if block_given?
    end
  end

  def generate_circle(count, x, y, r, angle_min, angle_max)
    @factory.generate(count) do |p, i|
      angle = angle_min + rand() * (angle_max - angle_min)
      p.x = x + Math.cos(angle / 180.0 * Math::PI) * r
      p.y = y + Math.sin(angle / 180.0 * Math::PI) * r
      p.parent = self
      p.lifetime = @lifetime
      p.image = @image
      yield p, i, angle if block_given?
    end
  end

  def add_generator(interval, &b)
    @generator << EmitterGenerator.new(b, interval)
  end

  def update
    @generator.each do |g|
      g.count += 1
      if g.count >= g.interval
        g.count = 0
        g.b.call
      end
    end
  end
end


class TestParticle < StandardParticle

  def initialize
    super
  end

  def hit(o)
    @dy = -@dy * 0.5
    self.collision_enable = false
  end
end

class TestEmitter < SampleEmitter
  attr_accessor :wall

  def update
    super
    Sprite.check @wall, @factory.particles
  end
end

image = Image.new(3,3)
image[1,0] = [128, 255, 255, 255]
image[1,2] = [128, 255, 255, 255]
image[0,1] = [128, 255, 255, 255]
image[2,1] = [128, 255, 255, 255]
image[1,1] = C_WHITE

emitter = TestEmitter.new(TestParticle)
emitter.gravity = 0.1
emitter.image = image
emitter.wall = Sprite.new(0, 400, Image.new(640, 180, C_WHITE))

emitter.add_generator(1) do
  emitter.generate_circle(5, 200, 100, 1, 0, 360) do |p, i, angle|
    p.dx = Math.cos(angle / 180.0 * Math::PI)*3
    p.dy = Math.sin(angle / 180.0 * Math::PI)*3
    p.blend = :add
  end
end

font = Font.new(32)
Window.after_call[:particle] = ParticleFactory.method(:step)

Window.loop do
  emitter.update
  Window.draw_font(0,0,ParticleFactory.count.to_s, font)
  Window.draw_font(0,32,Window.fps.to_s, font)
end

