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
end


class Scene
end

class FirstScene < Scene
  @@image = Image.new(1,1,C_GREEN)
  def initialize
  end

  def run
    Window.loop do
      if Input.key_push?(K_ESCAPE)
        break
      end
      if Input.key_push?(K_SPACE)
        MenuScene.new([self], 100, 100).run
      end

      draw
    end
    return nil
  end

  def draw
    Window.draw_scale(0,0,@@image,640,480,0,0)
  end
end

class MenuScene < Scene

  @@font = Font.new(24)
  @@image = Image.new(204,124,C_BLACK).box_fill(2,2,201,121,C_WHITE).box_fill(7,7,196,116,C_BLACK)
  @@image.draw_font_ex(32, 22, "0001", @@font).draw_font_ex(120, 22, "0002", @@font)
  @@image.draw_font_ex(32, 54, "0003", @@font).draw_font_ex(120, 54, "0004", @@font)
  @@image.draw_font_ex(32, 86, "0005", @@font).draw_font_ex(120, 86, "0006", @@font)
  @@image.box_fill(60, 0, 139, 23, [0,0,0,0])
  @@cursor = Image.new(12,12).triangle_fill(0,0,5,5,0,10,C_WHITE)
  @@menu = Image.new(80, 24, C_BLACK).draw_font_ex(10, -2, "MENU", @@font)

  def initialize(s, x, y)
    @s = s
    @x = x
    @y = y
    @cx = 0
    @cy = 0
  end

  def run
    Window.loop do
      if Input.key_push?(K_ESCAPE)
        break
      end
      if Input.key_push?(K_SPACE)
        MenuScene.new(@s + [self], @x + @cx * 88 - 45, @y + @cy * 32 + 24).run
      end

      @cx = 1 - @cx if Input.key_push?(K_LEFT) or Input.key_push?(K_RIGHT)
      @cy -= 1 if Input.key_push?(K_UP)
      @cy += 1 if Input.key_push?(K_DOWN)
      @cy = 2 if @cy < 0
      @cy = 0 if @cy >2

      Sprite.draw @s
      draw
      Window.draw(@x + @cx * 88 + 16,@y + @cy * 32 + 29, @@cursor)
    end

    return nil
  end

  def draw
      Window.draw(@x, @y, @@image)
      if @s.size == 1
        Window.draw(@x + 60, @y, @@menu)
      end
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


