#!ruby -Ks
#シェーダではないんだけども。吉里吉里のturn拡張トランジションぽいものの実験作。動かしたことないから違うかもしれない。
require 'dxruby'

Window.width = 800
Window.height = 600

class Turn
  class TurnCard
    def initialize(x, y, image1, image2, wait)
      @x = x
      @y = y
      @image1 = image1 # 表
      @image2 = image2 # 裏
      @count = 0
      @wait = wait
    end

    def next
      if @wait > 0
        @wait -= 1
      else
        @count += 1
      end
      if @count == 180
        @count = 0
        @image1, @image2 = @image2, @image1
      end
    end

    def draw
      if @count % 180 < 90
        dx = (Math::cos(Math::PI / 180 * (@count % 180)) + 1.0) / 2.0 * 64
        dz = Math::sin(Math::PI / 180 * (@count % 180)) / 10.0
        Window.draw_morph(@x, @y, @x + dx, @y + 64 - dx, @x + 64, @y + 64, @x + 64 - dx, @y + dx, @image1)
      else
        dx = 64 - (Math::cos(Math::PI / 180 * (@count % 180)) + 1.0) / 2.0 * 64
        Window.draw_morph(@x, @y, @x + dx, @y + 64 - dx, @x + 64, @y + 64, @x + 64 - dx, @y + dx, @image2)
      end
    end
  end

  def initialize
    image = Image.load('./bgimage/BG10a_80.jpg')
    image1 = Image.new((image.width + 63) / 64 * 64, (image.height + 63) / 64 * 64).copy_rect(0, 0, image)

    image = Image.load('./bgimage/BG13a_80.jpg')
    image2 = Image.new((image.width + 63) / 64 * 64, (image.height + 63) / 64 * 64).copy_rect(0, 0, image)

    @card = []
    (0...image1.height / 64).each do |y|
      (0...image1.width / 64).each do |x|
        @card << TurnCard.new(x * 64, y * 64, image1.slice(x * 64, y * 64, 64, 64), image2.slice(x * 64, y * 64, 64, 64), (x + 10 - y)*5)
      end
    end
  end

  def draw
    @card.each do |c|
      c.next
      c.draw
    end
  end

end

o = Turn.new
Window.loop do
  o.draw
  break if Input.key_push?(K_ESCAPE)
end



