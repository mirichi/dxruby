#!ruby -Ks
# DXRuby サンプルプログラム

# ライブラリの取り込み
require 'dxruby'

# 窓位置。省略するとOSデフォルト
#Window.x       = 0
#Window.y       = 0

# スクリーンイメージサイズ。省略するとこのサイズ
#Window.width   = 640
#Window.height  = 480

# ウィンドウのキャプション。省略すると「DXRuby Application」
Window.caption = "サンプル"

# スクリーンイメージサイズに対するウィンドウの倍率。２にすると縦横2倍で表示される。
# 勝手にアンチエイリアシングされる。
# 省略すると１。
#Window.scale   = 1

# ウィンドウモードにするかどうか。trueでウィンドウ。falseでフルスクリーン。
# 省略するとtrue。
#Window.windowed = true

# 秒間更新頻度を設定する。
# 省略すると60。
#Window.fps = 60

# 重くてfpsが守れない場合に描画をスキップする指定。
# 省略するとfalse
#Window.frameskip = false

# 画像ファイルを読み込んでイメージオブジェクトを作成する。
# jpgとかbmpとかpngとかが使える。
image = Image.load("data.png")

# キャラの状態を設定する
x = 100
y = 100
rot = 0
scale = 1
alpha = 255

# ウィンドウが閉じられたら自動的に終了するループ
Window.loop do

  # キャラを動かしたりする
  # xとyメソッドはパッド入力をx座標とy座標の増分(-1、0、1)で返す。
  # 引数はパッド番号。0はキーボードと兼用。省略は0。
  x = x + Input.x
  y = y + Input.y

  # Zを押したらクルクル回る
  # 第2引数は入力するボタン。
  # Zにはパッドのボタン0が標準で割り当てられている。
  if Input.keyDown?(K_Z) == true then
    rot = rot + 5
    if rot > 359 then
      rot = rot - 360
    end
  end

  # Xを押したらサイズ変更
  # Xにはパッドのボタン1が標準で割り当てられている。
  if Input.keyDown?(K_X) == true then
    scale = scale + 0.05
    if scale > 2 then
      scale= 0.2
    end
  end

  # Cを押したら半透明
  # Cにはパッドのボタン2が標準で割り当てられている。
  if Input.keyDown?(K_C) == true then
    alpha = alpha - 2
    if alpha < 0 then
      alpha= 255
    end
  end

  # 普通に描画する
  Window.draw(x + 50, y + 100, image)

  # なんかしてキャラを描画する
  Window.drawRot(x, y, image, rot)                 # 回転(0が基準で右回り。360で一周)
  Window.drawScale(x + 50, y, image, scale, scale) # 拡大縮小(1が基準で倍率を表す)
  Window.drawAlpha(x + 100, y, image, alpha)       # 半透明(0が透明で255が通常描画)

  # 全部同時に適用するメソッドはこちら
  Window.drawEx(x + 50, y + 50, image, :angle => rot, :scalex => scale, :scaley => scale, :alpha => alpha)

  # エスケープキーで終了
  if Input.keyDown?(K_ESCAPE) == true then
    break
  end

  # スクリーンショット機能
  if Input.keyPush?(K_F12) == true then
    if ! File.exist?("screenshot") then
      Dir.mkdir("screenshot")
    end
    Window.getScreenShot("screenshot/screenshot" + Time.now.strftime("%Y%m%d_%H%M%S") + ".jpg")
  end
end




