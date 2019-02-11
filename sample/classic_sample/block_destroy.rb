#!ruby -Ks
# DXRuby サンプル ブロック崩しモドキ
# 左クリックでボールが飛びます。後はマウスで打ち返して。
require 'dxruby'

Window.caption = "ブロック崩しモドキ"

# ブロックの構造体
Block = Struct.new(:x, :y, :image)

# 音データ作成
v = 80  # ボリューム
s1 = SoundEffect.new(50) do # 50mSecの効果音。ブロック内は1msの変化
  v = v - 4 if v > 0        # ボリュームダウン
  [220, v]                  # ブロックの戻りの配列が周波数とボリューム
end
v = 80
s2 = SoundEffect.new(50) do
  v = v - 4 if v > 0
  [440, v]
end
v = 80
s3 = SoundEffect.new(50) do
  v = v - 4 if v > 0
  [880, v]
end

# 画像データ作成。
# パッドの画像
padimage = Image.new(100, 20)
padimage.boxFill(0, 0, 99, 19, [255, 255, 255])

# ブロックの画像。ちょっと綺麗にしようと思ったらずいぶん増えちゃった
blockimage = [Image.new(60, 20, [255, 0, 0]),
              Image.new(60, 20, [255, 255, 0]),
              Image.new(60, 20, [0, 255, 0])]
blockimage[0].line(0, 0, 59, 0, [255, 150, 150])
blockimage[0].line(0, 0, 0, 19, [255, 150, 150])
blockimage[0].line(0, 19, 59, 19, [120, 0, 0])
blockimage[0].line(59, 0, 59, 19, [120, 0, 0])
blockimage[1].line(0, 0, 59, 0, [255, 255, 150])
blockimage[1].line(0, 0, 0, 19, [255, 255, 150])
blockimage[1].line(0, 19, 59, 19, [150, 150, 0])
blockimage[1].line(59, 0, 59, 19, [150, 150, 0])
blockimage[2].line(0, 0, 59, 0, [150, 255, 120])
blockimage[2].line(0, 0, 0, 19, [150, 255, 120])
blockimage[2].line(0, 19, 59, 19, [0, 150, 0])
blockimage[2].line(59, 0, 59, 19, [0, 150, 0])

# 弾の画像
ballimage = Image.new(24, 24)
ballimage.circleFill(12, 12, 11, [255, 255, 255])

# 玉の情報
tx = 0
ty = 416
dtx = 0
dty = 0

# 0は初期化、1は玉が飛んでない。2は飛んでる。
state = 0

block = []

# メインループ
Window.loop do
  x = Input.mousePosX - 50
  x = 0 if x < 0
  x = 540 if x > 540
  y = Input.mousePosY - 10
  y = 220 if y < 220
  y = 460 if y > 460

  case state
  when 0
    # ブロック配列初期化
    block.clear
    # ブロック生成
    for i in 0..9
      for j in 0..5
        block.push(Block.new(i * 64 + 2, j * 24 + 50, blockimage[j / 2]))
      end
    end
    state = 1

  when 1
    # 飛んでないときはパッドにくっついている
    if Input.mousePush?(M_LBUTTON) then # マウスの左クリックで飛ばす
      state = 2
      dtx = rand(2) * 16 - 8
      dty = -8
    else   # くっついてるとき
      tx = x + 38
      ty = y - 24
    end

  when 2
    # ゲームメイン処理
    # ブロックとの判定
    block.delete_if do |b|
      flag = false
      # 横にブロックがあったら横にはねかえる
      if tx + dtx + 24 > b.x and tx + dtx < b.x + 60 and
         ty + 24 > b.y and ty < b.y + 20 then
        dtx = -dtx
        s3.play
        # ナナメにあった場合は縦にもはねかえる
        if tx + 24 > b.x and tx < b.x + 60 and
           ty + dty + 24 > b.y and ty + dty < b.y + 20 then
          dty = -dty
        end
        flag = true

      # 縦にブロックがあったら縦にはねかえる
      elsif tx + 24 > b.x and tx < b.x + 60 and
         ty + dty + 24 > b.y and ty + dty < b.y + 20 then
        dty = -dty
        s3.play
        flag = true
      end
      flag
    end
    # 移動
    tx = tx + dtx
    ty = ty + dty
    # パッドとの判定
    if tx + 24 > x and tx < x + 100 and ty + 24 > y and ty < y + 20 and dty > 0 then
      dty = -8
      if tx < x + 38 - dtx * 2 then
        dtx = -8
      else
        dtx = 8
      end
      s1.play
    end
    # 画面外の判定
    if tx > 607 then
      dtx = -8
      s2.play
    end
    if tx < 8 then
      dtx = 8
      s2.play
    end
    if ty <= 0 then
      dty = 8
      s2.play
    end
    state = 1 if ty > 479        # ボールが落ちた
    state = 0 if block.size == 0 # ブロックが全部消えた
  end

  # 画像描画
  Window.draw(x, y, padimage)
  Window.draw(tx, ty, ballimage)
  block.each do |b|
    Window.draw(b.x, b.y, b.image)
  end

  break if Input.keyDown?(K_ESCAPE)
end
