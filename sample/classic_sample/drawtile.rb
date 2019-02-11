# マップデータから背景描画サンプル
# RenderTargetクラスの使い方のサンプルになっています。
require 'dxruby'

# マップデータ
map1 = [
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1],
  [1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 2, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 1, 1],
  [1, 1, 1, 2, 0, 0, 0, 3, 3, 3, 3, 2, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1],
  [1, 1, 1, 2, 0, 0, 0, 0, 3, 3, 3, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1],
  [1, 1, 2, 2, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1],
  [1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 3, 1, 1, 1],
  [1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 3, 3, 1, 1, 1],
  [1, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1],
  [1, 1, 2, 1, 1, 1, 1, 2, 2, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 3, 0, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 0, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 0, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1, 1, 3, 3, 3, 0, 3, 3, 3, 3, 1, 1, 1, 1, 1, 2, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 2, 3, 3, 3, 1, 1, 1, 1, 1, 2, 2, 2],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2],
  [1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1],
  [1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
  [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]
]

# 手前に描画する用の上層マップデータ（4は木のあたま。nilにすると描画しない）
map2 = [
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil],
  [nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, 4, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, 4, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, 4, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, 4, nil, nil, nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, 4, 4],
  [nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, 4, 4, 4],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, 4, 4, nil],
  [nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, 4, 4, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil],
  [nil, nil, nil, nil, nil, nil, nil, nil, nil, 4, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil]
]


# 絵のデータを作る
mapimage = []
mapimage.push(Image.new(32, 32, [100, 100, 200])) # 海
mapimage.push(Image.new(32, 32, [50, 200, 50]))   # 平地
mapimage.push(Image.new(32, 32, [50, 200, 50]))   # 木の根元
mapimage[2].boxFill(13, 0, 18, 28, [200, 50, 50])
mapimage.push(Image.new(32, 32, [50, 200, 50]))   # 山
for i in 1..30 do
  mapimage[3].line(15 - i/2, i, 16 + i/2, i, [200, 100, 100])
end
mapimage.push(Image.new(32, 32))                       # 木のあたま。背景は透明色にしておく。
mapimage[4].boxFill(13, 16, 18, 31, [200, 50, 50])
mapimage[4].circleFill(16, 10, 8, [0, 255, 0])

image = Image.new(32, 48).                           # 主人公
      circle(15, 5, 5, [255, 255, 255]).
      line(5, 18, 26, 18, [255, 255, 255]).
      line(15, 10, 15, 31, [255, 255, 255]).
      line(15, 31, 5, 47, [255, 255, 255]).
      line(15, 31, 25, 47, [255, 255, 255])

# 座標初期位置
x = 0
y = 0
dx = 0
dy = 0
font = Font.new(32)

# 0:入力待ち中、1:移動中
state = 0

# 移動カウント
count = 0

# RenderTarget作成
rt_main = RenderTarget.new(640-64, 480-64)
rt_sub  = RenderTarget.new(640-64, 480-64)

Window.loop do

  # 移動中はカウントアップするだけ
  if state == 1 then
    count = count + 4   # カウントアップ
    if count >= 32 then  # カウント完了なら入力待ちに遷移
      state = 0
      count = 0
      x = x + dx   # x座標移動
      y = y + dy   # y座標移動
      # 座標ループ処理
      x = 0 if x > 31
      x = 31 if x < 0
      y = 0 if y > 31
      y = 31 if y < 0
    end
  end

  # 入力待ち中
  if state == 0 then
    ix = Input.x
    iy = Input.y
    # 押されたチェック
    if ix + iy != 0 and (ix == 0 or iy == 0) then # ナナメは却下
      tempx = x + ix
      tempy = y + iy
      tempx = tempx - 32 if tempx > 31
      tempy = tempy - 32 if tempy > 31
      if map1[tempy][tempx] == 1 then
        state = 1    # スクロール中に遷移
        dx = ix
        dy = iy
      end
    end
  end

  # ベースマップをrt_mainに描画
  rt_main.drawTile(0, 0, map1, mapimage, (x - 8.5) * 32 + count * dx, (y - 6) * 32 + count * dy, 18, 13)
  rt_main.update

  # 上層マップをrt_subに描画
  rt_sub.drawTile(0, 0, map2, mapimage, (x - 8.5) * 32 + count * dx, (y - 6) * 32 + count * dy, 18, 13)
  rt_sub.update

  # ベースマップを画面に描画
  Window.draw(32, 32, rt_main)

  # 人描画
  Window.draw(9.5 * 32, 7 * 32 - 16, image)

  # 上層マップを画面に描画
  Window.draw(32, 32, rt_sub)

  # エスケープキーで終了
  break if Input.keyPush?(K_ESCAPE)
  Window.drawFont(32,32,Window.fps.to_s + " fps", font)
  Window.drawFont(32,64,Window.getLoad.to_i.to_s + " %", font)
end
