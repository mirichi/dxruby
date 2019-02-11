require 'dxruby'
require './shader/rgsssprite'

Window.width = 800
Window.height = 600

class Wingman
  @@image = Image.load('./bgimage/BG42a.jpg')
  @@shader = SpriteShader.new
  def initialize
    @count = 0
    @flag = false
    @@shader.gray = 255
  end
  def start
    @flag = true
  end
  def update
    @count += 2 if @flag
  end
  def draw
    if @count < 180
      Window.draw_ex(0, 0, @@image, :alpha => 128, :blend => :add, :angle => @count)
      Window.draw_ex(0, 0, @@image, :alpha => 128, :blend => :add, :angle => -@count)
    elsif @count < 300
      @@shader.gray = (@count - 180) * 255 / 120
      Window.draw_ex(0, 0, @@image, :angle => 180, :shader => @@shader)
    else
      Window.draw_ex(0, 0, @@image, :angle => 180, :shader => @@shader)
    end
  end
end

w = Wingman.new

Window.loop do
  w.update
  w.draw
  w.start if Input.key_push?(K_SPACE)
  break if Input.key_push?(K_ESCAPE)
end






