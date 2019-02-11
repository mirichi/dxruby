#!ruby -Ks
# DXRuby 効果音サンプル
# ZXCキーで音が出ます
require 'dxruby'

# かきーん
v = 100.0
c = 60
f = 1300
s1 = SoundEffect.new(500) do
  c = c - 1
  if c < 0 then
    v = v - 0.2
    f = 1760
  end
  [f, v]
end

# 和音
s2 = SoundEffect.new(1000,WAVE_TRI) do # 低音は三角波
  [110, 80]
end
s2.add(WAVE_RECT) do # 矩形波
  [275, 40]
end
s2.add(WAVE_RECT) do # 矩形波
  [330, 40]
end

# 周波数変調
f = 0
v = 150.0
s3 = SoundEffect.new(500, WAVE_SIN) do
f = f + 1
v = v - 0.2
  [880 + Math.sin(f) * 200, v]
end

# メインループ
Window.loop do
  break if Input.keyDown?(K_ESCAPE)
  s1.play if Input.keyPush?(K_Z)
  s2.play if Input.keyPush?(K_X)
  s3.play if Input.keyPush?(K_C)
end

