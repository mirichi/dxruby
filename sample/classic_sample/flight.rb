#ruby -Ks
# DXRuby ƒTƒ“ƒvƒ‹ ”ò‚ñ‚Å‚é‚æ‚¤‚È‚à‚Ì
require 'dxruby'

image = Image.new(32,32)
image.boxFill(13, 16, 18, 26, [255, 200, 50, 50]) # –Ø
image.line(13, 16, 13, 26, [255, 0, 0, 0])
image.line(18, 16, 18, 26, [255, 0, 0, 0])
image.circleFill(16, 8, 8, [255, 0, 255, 0])
image.circle(16, 8, 8, [255, 0, 0, 0])

class Bg
  @@ground = Image.new(1000,600, [255, 0, 0, 255])
  attr_accessor :y, :angle

  def initialize
    @y = 290
    @angle = 0
  end

  def update
    @angle -= Input.x*1.8
    if @angle < -75 then
      @angle = -75
    end
    if @angle > 75 then
      @angle = 75
    end
    @y -= Input.y*5
    @y = 400 if @y > 400
    @y = 100 if @y < 100
    Window.drawRot(-180, @y, @@ground, angle, 500, 240-@y, 0)
  end
end

bg = Bg.new
Window.bgcolor = [200,250,255]
x = 320
y = 240
z = []
Obj = Struct.new(:x, :y, :z)
font = Font.new(32)
angle = 0
objx = 0

Window.loop do

  z.push(Obj.new(rand(2000) - 680, 500, 5.0))
  z.push(Obj.new(rand(2000) - 680, 500, 5.0))

  bg.update

  rad = Math::PI/180.0*bg.angle
  sina = Math.sin(rad)
  cosa = Math.cos(rad)
  bgy1 = (530 - bg.y) / 290.0
  bgy2 = bg.y - 240
  z.delete_if do |obj|
    drawx = ((obj.x - 320) / obj.z * cosa - (((obj.y - 240) / obj.z - 50) * bgy1 + bgy2) * sina) + 320
    drawy = ((obj.x - 320) / obj.z * sina + (((obj.y - 240) / obj.z - 50) * bgy1 + bgy2) * cosa) + 240

    Window.drawEx(drawx - image.width / 2, drawy - image.height / 2, image, :scalex=>5.0 / obj.z, :scaley=>5.0 / obj.z, :z=>5 - obj.z, :angle=>bg.angle)

    obj.x += bg.angle / 3.0
    obj.z -= 0.15
    if obj.z < 0.5 then
      true
    else
      false
    end
  end

  break if Input.keyPush?(K_ESCAPE)
  Window.drawFont(0, 448, Window.getLoad.to_i.to_s + " %", font)
end
