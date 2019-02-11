# スクロールサンプルその２(端で止まるスクロール)
require 'dxruby'
require './map'

# 絵のデータを作る
mapimage = []
mapimage.push(Image.new(32, 32, [100, 100, 200])) # 海
mapimage.push(Image.new(32, 32, [50, 200, 50]))   # 平地
mapimage.push(Image.new(32, 32, [50, 200, 50]).   # 木の根元
                        box_fill(13, 0, 18, 28, [200, 50, 50]))
mapimage.push(Image.new(32, 32, [50, 200, 50]).   # 山
                        triangle_fill(15, 0, 0, 31, 31, 31, [200, 100,100]))
mapimage.push(Image.new(32, 32).  # 木のあたま。背景は透明色にしておく。
                        box_fill(13, 16, 18, 31, [200, 50, 50]).
                        circle_fill(16, 10, 8, [0, 255, 0]))

# Fiberを使いやすくするモジュール
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
    t.times{Fiber.yield}
  end
end

# 自キャラ
class Player < Sprite
  include FiberSprite
  attr_accessor :mx, :my

  def initialize(x, y, map, target=Window)
    @mx, @my, @map, self.target = x, y, map, target
    super(8.5 * 32, 6 * 32)

    # 頭は上にはみ出して描画されるのでそのぶん位置補正する細工
    self.center_x = 0
    self.center_y = 16
    self.offset_sync = true

    # 棒人間画像
    self.image = Image.new(32, 48).
                       circle(15, 5, 5, [255, 255, 255]).
                       line(5, 18, 26, 18, [255, 255, 255]).
                       line(15, 10, 15, 31, [255, 255, 255]).
                       line(15, 31, 5, 47, [255, 255, 255]).
                       line(15, 31, 25, 47, [255, 255, 255])
  end

  # Player#updateすると呼ばれるFiberの中身
  def fiber_proc
    loop do
      ix, iy = Input.x, Input.y

      # 押されたチェック
      if ix + iy != 0 and (ix == 0 or iy == 0) and # ナナメは却下
         @map[@mx/32+ix, @my/32+iy] == 1 and       # 移動先が平地のときのみ
         @mx/32 + ix >= 0 and @mx/32 + ix < @map.size_x and # マップの端までしか行けない
         @my/32 + iy >= 0 and @my/32 + iy < @map.size_y
        # 8フレームで1マス移動
        8.times do
          @mx += ix * 4 # マップ上の座標
          @my += iy * 4
          self.x += ix * 4 # 画面上の座標
          self.y += iy * 4
          wait # waitすると次のフレームへ
        end
      else
        wait
      end
    end
  end
end

# RenderTarget作成
rt = RenderTarget.new(640-64, 480-64)

# マップの作成
map_base = Map.new("map.dat", mapimage, rt)
map_sub = Map.new("map_sub.dat", mapimage, rt)

# 自キャラ
player = Player.new(480, 480, map_base, rt)

# マップ座標
map_x = player.mx - player.x
map_y = player.my - player.y

# 画面内の自キャラ移動範囲
min_x = 128
max_x = 640 - 64 - 128 - 32
min_y = 128
max_y = 480 - 64 - 128 - 32

Window.loop do
  # 人移動処理
  player.update

  # ある程度画面の端まで行ったらマップをスクロールさせて自キャラは止める
  if player.x < min_x
    # マップの端じゃない時だけ処理する
    if map_x > 0
      map_x -= 4
      player.x += 4
    end
  end
  if player.x > max_x
    if map_x < map_base.size_x * 32 - rt.width
      map_x += 4
      player.x -= 4
    end
  end
  if player.y < min_y
    if map_y > 0
      map_y -= 4
      player.y += 4
    end
  end
  if player.y > max_y
    if map_y < map_base.size_y * 32 - rt.height
      map_y += 4
      player.y -= 4
    end
  end

  # rtにベースマップを描画
  map_base.draw(map_x, map_y)

  # rtに人描画
  player.draw

  # rtに上層マップを描画
  map_sub.draw(map_x, map_y)

  # rtを画面に描画
  Window.draw(32, 32, rt)

  # エスケープキーで終了
  break if Input.key_push?(K_ESCAPE)
end
