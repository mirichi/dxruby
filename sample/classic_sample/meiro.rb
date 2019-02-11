#!ruby -Ks
# DXRuby サンプル ３Ｄ迷路
require 'dxruby'

# マップ
map = [[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
       [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
       [1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1],
       [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1],
       [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1],
       [1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1],
       [1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1],
       [1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1],
       [1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1],
       [1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1],
       [1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1],
       [1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1],
       [1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1],
       [1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1],
       [1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1],
       [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
      ]

x = 1
y = 1
angle = 1

# 向きによる増分
angledata = [[0, -1], [1, 0], [0, 1], [-1, 0]]

block = Image.new(16, 16, [255, 255, 255, 255])
image = Array.new(4) {Array.new(3) {Image.new(256, 256)}}
jibun = Image.new(16, 16, [255, 255, 0, 0])

# 3D画像生成
# 計算でかなり強引に作っている
# それぞれの距離ごとに綺麗な画像を作って読み込んだほうがよいと思います
for i in 0..3
  for j in 0..2
    if i != 3
      x1 = ((32 + 192 * (j - 1))  - 128) / (3 - i) + 128
      y1 = (32                    - 128) / (3 - i) + 128
      x2 = ((224 + 192 * (j - 1)) - 128) / (3 - i) + 128
      y2 = (224                   - 128) / (3 - i) + 128
      image[i][j].boxFill(x1, y1, x2, y2, [255, 255/(3 - i), 255/(3 - i), 255/(3 - i)])
    else
      x1 = 256
      y1 = 0
      x2 = 0
      y2 = 256
    end
    if j == 0
      x3 = (((224 + 192 * (j - 1))  - 128) / (4 - i) + 128)
      y3 = (32 - 128) / (4 - i) + 128
      y4 = (224 - 128) / (4 - i) + 128
      for k in 0..(x3-x2)
        image[i][j].line(k+x2, y1 + (y3 - y1) / (x3-x2) * k, k+x2, y2 + (y4 - y2) / (x3-x2) * k, [255,128/(4 - i),128/(4 - i),255/(4 - i)])
      end
    elsif j == 2
      x3 = (((32 + 192 * (j - 1))  - 128) / (4 - i) + 128)
      y3 = (32 - 128) / (4 - i) + 128
      y4 = (224 - 128) / (4 - i) + 128
      for k in 0..(x1 - x3)
        image[i][j].line(x1 - k, y1 + (y3 - y1) / (x1-x3) * k, x1 - k, y2 + (y4 - y2) / (x1-x3) * k, [255,128/(4 - i),128/(4 - i),255/(4 - i)])
      end
    end
  end
end

Window.loop do
  # 左右おした
  angle += Input.x if Input.padPush?(P_LEFT) or Input.padPush?(P_RIGHT)
  angle = 0 if angle > 3
  angle = 3 if angle < 0

  # 上おした
  if Input.padPush?(P_UP) or Input.padPush?(P_DOWN)
    newx = x + angledata[angle - Input.y - 1][0]
    newy = y + angledata[angle - Input.y - 1][1]
    x, y = newx, newy if map[newy][newx] == 0
  end

  # 3D画面描画
  for i in 0..3
    for j in 0..2
      jx = x + angledata[angle - 3][0] * (j - 1) + angledata[angle - 2][0] * (i - 3)
      iy = y + angledata[angle - 2][1] * (i - 3) + angledata[angle - 3][1] * (j - 1)
      next if iy < 0 or iy > 15 or jx < 0 or jx > 15
      if map[iy][jx] == 1
        Window.draw(0, 0, image[i][j], i - (j - 1).abs)
      end
    end
  end

  # 右のマップ
  for i in 0..15
    for j in 0..15
      Window.draw(j * 16 + 288, i * 16, block) if map[i][j] == 1
    end
  end

  # 自分（赤の四角だけど）描画
  Window.draw(x * 16 + 288, y * 16, jibun)

  break if Input.keyPush?(K_ESCAPE)
end
