#!ruby -Ks
require 'dxruby'
require './shader/rgsssprite'

Window.width = 800
Window.height = 600

s = RgssSprite.new
s.image = Image.load('./bgimage/BG10a_80.jpg')

Window.loop do

  # Zでフラッシュさせる
  s.flash if Input.key_push?(K_Z)

  # Xを押している間グレイスケールになる
  if Input.key_down?(K_X)
    s.gray = 255
  else
    s.gray = 0
  end

  # Cを押している間、赤色が無くなる
  if Input.key_down?(K_C)
    s.tone = [-255,0,0]
  else
    s.tone = nil
  end

  # Vを押している間、青をブレンドする
  if Input.key_down?(K_V)
    s.color = [60,0,0,255]
  else
    s.color = nil
  end

  s.update
  s.draw
  break if Input.key_push?(K_ESCAPE)
end

