#!ruby -Ks
# DXRuby サンプル 砂
# マウス左クリックで描いて、右クリックで消せます。
require 'dxruby'

# ウィンドウ設定
Window.scale = 2
Window.width = 160
Window.height = 120

# 画面の画像データ
$screen = Image.new(160, 120)

BLACK = [0, 0, 0]
WHITE = [255, 255, 255]

class Dot
  def initialize(x, y)
    @x = x
    @y = y
  end

  # 高速化のために点が移動するときのみ描画するようにしたらちょっとごちゃごちゃ
  def move
    if $screen.compare(@x, @y + 1, BLACK) then # 真下
      $screen[@x, @y] = BLACK
      @y = @y + 1
      return true if @y > 119
      $screen[@x, @y] = WHITE
    elsif rand(2) == 0 then
      if $screen.compare(@x-1, @y + 1, BLACK) then # 左下
        $screen[@x, @y] = BLACK
        @y = @y + 1
        @x = @x - 1
        return true if @y > 119
        $screen[@x, @y] = WHITE
      end
    else
      if $screen.compare(@x+1, @y + 1, BLACK) then # 右下
        $screen[@x, @y] = BLACK
        @y = @y + 1
        @x = @x + 1
        return true if @y > 119
        $screen[@x, @y] = WHITE
      end
    end
    return false
  end
end

dot = []

Window.loop do
  # ドット生成
  dot.push(Dot.new(rand(10)+75, 0))

  # マウスの座標取得
  mx = Input.mousePosX / 2
  my = Input.mousePosY / 2

  # 左クリックで描画
  if Input.mouseDown?(M_LBUTTON) then
    $screen.boxFill(mx - 1, my - 1, mx + 1, my + 1, [0, 255, 0])
  end

  # 右クリックで消去
  if Input.mouseDown?(M_RBUTTON) then
    $screen.boxFill(mx - 1, my - 1, mx + 1, my + 1, [0, 0, 0])
  end

  # ドット移動
  dot.delete_if do |d|
    d.move
  end

  # 画面描画
  Window.draw(0, 0, $screen)

  break if Input.keyPush?(K_ESCAPE)
end
