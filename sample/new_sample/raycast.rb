#!ruby -Ks
require 'dxruby'

class Vector
  def initialize(*v)
    @vec = v
  end

  def rotate(angle)
    x = @vec[0] * Math.cos(Math::PI / 180 * angle) - @vec[1] * Math.sin(Math::PI / 180 * angle)
    y = @vec[0] * Math.sin(Math::PI / 180 * angle) + @vec[1] * Math.cos(Math::PI / 180 * angle)
    temp = @vec.dup
    temp[0] = x
    temp[1] = y
    Vector.new(*temp)
  end

  def +(v)
    case v
    when Vector
      Vector.new(*@vec.map.with_index{|s,i|s+v[i]})
    when Array
      Vector.new(*@vec.map.with_index{|s,i|s+v[i]})
    when Numeric
      Vector.new(*@vec.map{|s|s+v})
    else
      nil
    end
  end

  def *(matrix)
    result = []
    for i in 0..(matrix.size-1)
      data = 0
      for j in 0..(@vec.size-1)
        data += @vec[j] * matrix[j][i]
      end
      result.push(data)
    end
    return Vector.new(*result)
  end

  def [](i)
    @vec[i]
  end

  def size
    @vec.size
  end

  def to_a
    @vec
  end

  def x
    @vec[0]
  end
  def y
    @vec[1]
  end
  def z
    @vec[2]
  end
  def w
    @vec[3]
  end
end

class Matrix
  def initialize(*arr)
    @arr = Array.new(4) {|i| Vector.new(*arr[i])}
  end

  def *(a)
    result = []
    for i in 0..(a.size-1)
      result.push(@arr[i] * a)
    end
    return Matrix.new(*result)
  end

  def [](i)
    @arr[i]
  end

  def size
    @arr.size
  end

  def self.create_rotation_z(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [ cos, sin, 0, 0],
     [-sin, cos, 0, 0],
     [   0,   0, 1, 0],
     [   0,   0, 0, 1]
    )
  end

  def self.create_rotation_x(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [   1,   0,   0, 0],
     [   0, cos, sin, 0],
     [   0,-sin, cos, 0],
     [   0,   0,   0, 1]
    )
  end

  def self.create_rotation_y(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [ cos,   0,-sin, 0],
     [   0,   1,   0, 0],
     [ sin,   0, cos, 0],
     [   0,   0,   0, 1]
    )
  end

  def self.create_transration(x, y, z)
    return Matrix.new(
     [   1,   0,   0,   0],
     [   0,   1,   0,   0],
     [   0,   0,   1,   0],
     [   x,   y,   z,   1]
    )
  end

  def to_a
    @arr.map {|v|v.to_a}.flatten
  end
end


line1 = [568, 438, 568, 438, 607, 261, 607, 261, 520, 200, 520, 200, 531, 151,
 531, 151, 602, 115, 602, 115, 562, 19, 562, 19, 113, 21, 113, 21, 34, 83,
 34, 83, 81, 180, 81, 180, 221, 207, 221, 207, 251, 274, 251, 274, 204, 299,
 204, 299, 61, 262, 61, 262, 26, 358, 26, 358, 53, 430]
line2 = [88, 380, 88, 380, 479, 392, 479, 392, 536, 302, 536, 302, 534, 267,
 534, 267, 461, 210, 461, 210, 459, 151, 459, 151, 518, 91, 518, 91, 487, 53,
 487, 53, 156, 77, 156, 77, 125, 110, 125, 110, 292, 197, 292, 197, 311, 268,
 311, 268, 227, 342, 227, 342, 93, 330]

$walls = []

class Wall < Sprite
  attr_reader :height
  def initialize(x1, y1, x2, y2, height)
    super(0,0)
    self.collision = [x1, y1, x2, y2, x2, y2]
    @height = height
  end
end


tx = 53
ty = 430
line1.each_slice(2) do |x, y|
  $walls << Wall.new(tx, ty, x, y, 1)
  tx = x
  ty = y
end
tx = 93
ty = 330
line2.each_slice(2) do |x, y|
  $walls << Wall.new(tx, ty, x, y, 1)
  tx = x
  ty = y
end


$ray = []
(0..639).each do |x|
  s = Sprite.new(0,0)
  temp = Vector.new(400,0).rotate((x / 639.0 * 2 - 1) * 30)
  s.collision = [0, 0, temp.x, temp.y, temp.x, temp.y]
  s.center_x = s.center_y = 0
  $ray << s
end


x = 300 
y = 400
angle = 0

image = Image.new(1,1,C_RED)

Window.loop do

  angle += Input.x*5
  if Input.key_down?(K_UP)
    x += Math.cos(Math::PI / 180 * angle) * 3
    y += Math.sin(Math::PI / 180 * angle) * 3
  end
  if Input.key_down?(K_DOWN)
    x -= Math.cos(Math::PI / 180 * angle) * 3
    y -= Math.sin(Math::PI / 180 * angle) * 3
  end

  $ray.each_with_index do |r, i|
    r.x = x
    r.y = y
    r.angle = angle
    ary = r.check($walls)
    z = 250000
    ax, ay = Vector.new(r.collision[0], r.collision[1]).rotate(angle).to_a
    ax += r.x + 0.5
    ay += r.y + 0.5
    bx, by = Vector.new(r.collision[2], r.collision[3]).rotate(angle).to_a
    bx += r.x + 0.5
    by += r.y + 0.5
    px = 0
    py = 0
    height = 0
    if ary.size > 0 then
      ary.each do |w|
        cx = w.collision[0] + w.x + 0.5
        cy = w.collision[1] + w.y + 0.5
        dx = w.collision[2] + w.x + 0.5
        dy = w.collision[3] + w.y + 0.5
        tempx = dx-cx
        tempy = dy-cy

        dR = (tempy * (cx - ax) - tempx * (cy - ay) ) / ((bx - ax) * tempy - (by - ay) * tempx)

        ix = ax + dR * (bx - ax)
        iy = ay + dR * (by - ay)

        length = (ix - r.x)**2 + (iy - r.y)**2

        if length < z
          z = length
          px = ix
          py = iy
          height = w.height
        end
      end
      z = Math::sqrt(z)
      c = 255 - z / 400 * 255
      Window.draw_line(i, 240 + 400.0 / z * 10, i, 240 - 400.0 / z * 10 * height, [c, c, c])
      Window.draw(px, py, image)
    end
  end
  break if Input.key_push?(K_ESCAPE)
end

