#!ruby -Ks
require 'dxruby'

class ExitException < StandardError
end

module Window
  @@created = false

  def self.loop
    unless @@created
      Window.create
      @@created = true
    end

    while(true) do
      raise ExitException if Input.update
      yield
      Window.sync
      Window.update
    end
  end

  def self.times(count)
    unless @@created
      Window.create
      @@created = true
    end

    count.times do |i|
      raise ExitException if Input.update
      yield i
      Window.sync
      Window.update
    end
  end
end


class SpriteBase < Sprite
  def initialize(angle)
    self.x = 320 - self.image.width / 2
    self.y = 240 - self.image.height / 2
    speed = rand() * 5 + 2
    @dx = Math.cos(angle / 180.0 * Math::PI) * speed
    @dy = Math.sin(angle / 180.0 * Math::PI) * speed
  end

  def update
    self.x += @dx
    self.y += @dy
  end
end

class Hoge < SpriteBase
  @@image = Image.new(16,16).circle_fill(8,8,8,C_YELLOW)
  def initialize
    self.image = @@image
    super(rand(360))
  end
end

class Fuga < SpriteBase
  @@image = Image.new(16,16).circle_fill(8,8,8,C_GREEN)
  def initialize
    self.image = @@image
    super(rand(180) + 180)
  end

  def update
    super
    @dy += 0.2
  end
end


class Scene
end

class FirstScene < Scene
  def initialize
    @objects = []
    @screen_collision = Sprite.new
    @screen_collision.collision = [0, 0, 639, 479]
  end

  def run
    Window.loop do
      if Input.key_push?(K_SPACE)
        return SecondScene.new
      end
      if Input.key_push?(K_ESCAPE)
        MenuScene.new(self).run
      end

      update
      @objects = @screen_collision.check(@objects)
      draw
    end
  end

  def update
    @objects.push Hoge.new
    Sprite.update @objects
  end

  def draw
    Sprite.draw @objects
  end
end

class SecondScene < Scene
  def initialize
    @objects = []
    @screen_collision = Sprite.new
    @screen_collision.collision = [0, 0, 639, 479]
  end

  def run
    Window.loop do
      if Input.key_push?(K_SPACE)
        return nil
      end
      if Input.key_push?(K_ESCAPE)
        MenuScene.new(self).run
      end

      update
      @objects = @screen_collision.check(@objects)
      draw
    end
  end

  def update
    @objects.push Fuga.new
    Sprite.update @objects
  end

  def draw
    Sprite.draw @objects
  end
end

class MenuScene < Scene
  @@fadeimage = Image.new(1,1,C_BLACK)
  @@backimage = Image.new(1,1,[192,0,0,0])
  @@menuimage = Image.new(100,50,C_BLACK).box(0, 0, 99, 49, C_WHITE)
                                         .draw_font_ex(10,10,"ÒÆ?",Font.new(32))

  def initialize(s)
    @s = s
  end

  def run
    Window.times(30) do |i|
      @s.update
      @s.draw
      Window.draw_ex(0, 0, @@fadeimage, :alpha=>255*i/45, :scale_x=>640, :scale_y=>480, :center_x => 0, :center_y=>0)
    end

    Window.loop do
      if Input.key_push?(K_ESCAPE)
        break
      end

      @s.update
      @s.draw
      Window.draw_scale(0, 0, @@backimage, 640, 480, 0, 0)
      Window.draw(270, 215, @@menuimage)
    end

    Window.times(30) do |i|
      @s.update
      @s.draw
      Window.draw_ex(0, 0, @@fadeimage, :alpha=>255*(29 - i)/45, :scale_x=>640, :scale_y=>480, :center_x => 0, :center_y=>0)
    end

    return nil
  end
  
end


scene = FirstScene.new

begin

loop do
  result = scene.run
  if Scene === result
    scene = result
  else
    break
  end
end

rescue ExitException
end


