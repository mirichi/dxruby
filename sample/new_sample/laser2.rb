#!ruby -Ks
require 'dxruby'

Window.mag_filter = TEXF_POINT
TO_RAD = Math::PI / 180
TO_DEG = 1 / Math::PI * 180

# ベースパーツ
class BaseParts < Sprite
  attr_accessor :next, :parts, :to_a
  @@image = []
  40.times do |i|
    @@image.push Image.new(60, 60).circle_fill(30, 30, 30, [255, 255*i/40, 255])
  end

  def initialize(x, y)
    super

    # 触手パーツ生成
    prev_part = nil
    parts1 = Array.new(8) do
      temp = Parts.new # nextに次のパーツオブジェクトを入れる
      prev_part.next = temp if prev_part
      prev_part = temp
      temp.joint_xp = 1 # 前のパーツからの接続x
      temp.joint_yp = 10 # 前のパーツからの接続y
      temp.joint_xn = 19 # 次のパーツへの接続x
      temp.joint_yn = 10 # 次のパーツへの接続y
      temp.collision = [10, 10, 10]
      temp.joint_angle = 15
      temp
    end
    prev_part = nil
    parts2 = Array.new(8).map do
      temp = Parts.new # nextに次のパーツオブジェクトを入れる
      prev_part.next = temp if prev_part
      prev_part = temp
      temp.joint_xp = 1 # 前のパーツからの接続x
      temp.joint_yp = 10 # 前のパーツからの接続y
      temp.joint_xn = 19 # 次のパーツへの接続x
      temp.joint_yn = 10 # 次のパーツへの接続y
      temp.collision = [10, 10, 10]
      temp.joint_angle = -15
      temp
    end
    parts2[0].joint_angle_base = 180

    self.image = @@image[0]
    self.x = x
    self.y = y
    self.collision = [30, 30, 30]
    @parts= [parts1, parts2]
    @next = [[58, 30, parts1[0]], [2, 30, parts2[0]]]
    @hp = 8000
    @to_a = [self] + parts[0] + parts[1]
    @count = 0
    guide
    update
  end

  # 描画位置、角度調整
  def update
    self.image = @@image[0]
    @hitcount = 0
    if @count == 0
      self.y += 0.4
      self.scale_x = self.scale_y = (@hp / 8000.0) * 0.5 + 0.5
      @next[0][0] = (58.0 - center_x) * self.scale_x + center_x
      @next[1][0] = (2.0 - center_x) * self.scale_x + center_x
      @next.each do |ary|
        ary[2].update(ary[0] + self.x, ary[1] + self.y, 0)
      end
      vanish if self.y > 480 + 100
    end
  end

  # 描画
  def draw
    super
    if @count == 0
      @parts[0].each do |ary|
        ary.draw
      end
      @parts[1].each do |ary|
        ary.draw
      end
    else
      @count += 1
      vanish if @count > 60
      self.scale_x += 0.1
      self.scale_y += 0.1
      self.alpha -= 4
      @parts[0].each do |ary|
        ary.scale_x += 0.1
        ary.scale_y += 0.1
        ary.alpha -= 4
        ary.draw
      end
      @parts[1].each do |ary|
        ary.scale_x += 0.1
        ary.scale_y += 0.1
        ary.alpha -= 4
        ary.draw
      end
    end
  end

  # マウスカーソルにむかって誘導
  def guide
    if @count == 0
      @parts[0][7].guide
      @parts[1][7].guide
    end
  end

  def hit
    @hp -= 1
    self.image = @@image[@hitcount]
    @hitcount += 1
    if @hp <= 0
      @count = 1
      @to_a = []
    end
  end
end

# 触手パーツ
class Parts < Sprite
  attr_accessor :prev, :joint_xp, :joint_yp, :joint_xn, :joint_yn, :joint_angle, :joint_angle_base
  attr_reader :next
  @@image1 = Image.new(20,20).circle_fill(10,10,10,[255,0,0]).circle_fill(10,10,8,[255,160,160])
  @@image2 = Image.new(20,20).circle_fill(10,10,10,[255,200,200]).circle_fill(10,10,8,[255,240,240])

  def initialize
    super
    @next = nil
    @joint_xp = @joint_yp = @joint_xn = @joint_yn = @joint_angle = @joint_angle_base = 0
    @ang = 0
    self.image = @@image1
  end

  def next=(n)
    @next = n
    n.prev = self
  end

  # 描画位置、角度調整
  def update(jx = nil, jy = nil, angle = 0)
    self.image = @@image1
    a = @ang * TO_RAD
    sina = Math.sin(a)
    cosa = Math.cos(a)
    tempx = -@joint_xp + center_x
    tempy = -@joint_yp + center_y
    @joint_angle += rand() * 1.2 - 0.6
    @ang = @joint_angle + angle + @joint_angle_base
    self.x = tempx * cosa - tempy * sina + jx - center_x
    self.y = tempx * sina + tempy * cosa + jy - center_y
    if @next
      x = -@joint_xp + @joint_xn
      y = -@joint_yp + @joint_yn
      jnx = x * cosa - y * sina + jx
      jny = x * sina + y * cosa + jy
      @next.update(jnx, jny, @ang)
    end
  end

  # マウスカーソルにむかって誘導
  def guide
    mx = Input.mouse_pos_x
    my = Input.mouse_pos_y
    a = @ang * TO_RAD
    sina = Math.sin(a)
    cosa = Math.cos(a)
    tempx = @joint_xn - center_x
    tempy = @joint_yn - center_y
    x = tempx * cosa - tempy * sina + self.x + center_x
    y = tempx * sina + tempy * cosa + self.y + center_y
    mangle = Math.atan2(my - y, mx - x) * TO_DEG
    angle = (mangle + (180 - (@ang % 360))) % 360

    if 180 < angle
      @joint_angle = @joint_angle + 1
      if @joint_angle > 30
        @joint_angle = 30
        @prev.guide if @prev
      end
    elsif 180 > angle
      @joint_angle = @joint_angle - 1
      if @joint_angle < -30
        @joint_angle = -30
        @prev.guide if @prev
      end
    end
  end

  def hit
    self.image = @@image2
  end
end

class Laser
  @@image_houdai = Image.new(10,10,[0,255,0])

  def initialize(x, y)
    @s = []
    20.times do |i|
      t = Sprite.new(x, y, Image.new(20, 1)) # 縦1ピクセルの横長の画像
      t.image[i, 0] = [128, 255, 255, 255] # 該当箇所に点をうつ
      t.scale_y = 800 # 長さ
      t.center_x = 10  # 画像の真ん中
      t.center_y = 1  # 画像の下の端
      t.collision = [i, 0, i, 0] # 矩形で1ピクセルの判定範囲
      @s.push t
    end
    @s_collision = Sprite.new(x, y, Image.new(20, 1, [128, 255, 255, 255]))
    @s_collision.collision = [0, 0, 19, 0]
    @s_collision.scale_y = 800
    @s_collision.center_x = 10  # 画像の真ん中
    @s_collision.center_y = 1  # 画像の下の端
  end

  # レーザー発射、敵との衝突判定および描画
  def update
    if Input.mouse_down?(M_LBUTTON)
      mx = Input.mouse_pos_x
      my = Input.mouse_pos_y
      es = []
      Enemy.each do |e|
        es += e.to_a
      end
      @s_collision.angle = angle = Math.atan2(mx - @s_collision.x, @s_collision.y - my) * TO_DEG
      collision_ary = @s_collision.check(es)
      if collision_ary.size > 0
        @s.each do |i|
          i.angle = angle
          i.scale_y = 640000 #800**2
          temp = i.check(collision_ary)
          if temp.size > 0
            min_length = 640000 #800**2
            min_obj = nil
            temp.each do |e|
              length = ((e.x + e.center_x) - (i.x+10))**2 + ((e.y + e.center_y) - (i.y+10))**2
              if min_length > length
                min_obj = e
                min_length = length
              end
            end
            i.scale_y = min_length
            min_obj.hit
          end
          i.scale_y = Math::sqrt(i.scale_y)
          i.draw
        end
      else
        @s_collision.draw
      end
    end
    Window.draw(150,470,@@image_houdai)
    Window.draw(510,470,@@image_houdai)
  end
end

Enemy = [BaseParts.new(rand()*400+120, -100)]
laser = [Laser.new(145, 474), Laser.new(505, 474)]

x = 145
y = 474
count = 0
font = Font.new(32)

Window.loop do
  count += 1
  if count > 280
    Enemy.push BaseParts.new(rand()*400+120, -100)
    count = 0
  end

  Enemy.each do |e|
    e.guide
    e.update
  end
  Sprite.clean(Enemy)

  Sprite.update(laser)
  Sprite.draw(Enemy)

  Window.drawFont(0,0,Window.getLoad.to_i.to_s + " %", font, :z=>3)

  break if Input.key_push?(K_ESCAPE)
end

