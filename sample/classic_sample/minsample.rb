#!ruby -Ks
# DXRuby data.pngがカーソルキーで動くサンプル
require 'dxruby'

x = 0                         # x座標の変数
y = 0                         # y座標の変数
image = Image.load("data.png") # data.pngを読み込む

Window.loop do                # メインループ
  x = x + Input.x              # 横方向の入力でx座標を変更
  y = y + Input.y              # 縦方向の入力でy座標を変更

  Window.draw(x, y, image)     # data.bmpを座標の位置に表示
end
