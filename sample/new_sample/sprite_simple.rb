#!ruby -Ks
require "dxruby"

class Ruby < Sprite
  RT = RenderTarget.new(640,480)
  MAX_X = 639 - 32
  MAX_Y = 479 - 32

  # Imageオブジェクトを作成
  @@image = Image.load("ruby.png")

  # 初期化メソッド
  def initialize
    super
    self.x = rand(MAX_X)+1 # x座標
    self.y = rand(MAX_Y)+1 # y座標
    self.image = @@image  # 画像
    self.target = RT      # 描画先

    @dx = rand(2) * 2 - 1 # x増分
    @dy = rand(2) * 2 - 1 # y増分
  end

  # 更新
  def update
    self.x += @dx
    self.y += @dy
    if self.x <= 0 or MAX_X <= self.x
      @dx = -@dx
    end
    if self.y <= 0 or MAX_Y <= self.y
      @dy = -@dy
    end
  end
end

# Sprite オブジェクトの配列を生成する。
sprites = Array.new(6000){ Ruby.new }

font = Font.new(32)

# ウィンドウのキャプションを設定する。 
Window.caption = "Sprites"

# fpsを設定する。
Window.fps = 60

# メインループ。
# ウィンドウが閉じられた場合は自動的に終了する。
# 画面は毎フレームリセットされる。
Window.loop do
  # ESC キーが押された場合終了する。
  break if Input.keyPush?(K_ESCAPE)

  Sprite.update(sprites)
  Sprite.draw(sprites)

  Ruby::RT.update
  Window.draw(0,0,Ruby::RT)

  Window.drawFont(0,0,Window.fps.to_s + " fps", font)
  Window.drawFont(0,40,sprites.size.to_s + " objects", font)
  Window.drawFont(0,80,Window.getLoad.to_i.to_s + " %", font)
end

