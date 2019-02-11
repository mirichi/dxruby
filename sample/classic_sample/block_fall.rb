#!ruby -Ks
# DXRuby サンプル ブロック落とし
# SPACEで開始、左右で移動、ZorXで回転、下で落とす。
require 'dxruby'

Window.caption = "ブロック落としモドキ"

# ブロックデータ
blank = [0, 0, 0, 0]
BLOCK = [[],#ダミー
          [blank,
           [0, 1, 1, 0],# O
           [0, 1, 1, 0],
           blank],
          [blank,
           [2, 2, 2, 2],# I
           [0, 0, 0, 0],
           blank],
          [blank,
           [0, 0, 3, 3],# S
           [0, 3, 3, 0],
           blank],
          [blank,
           [0, 4, 4, 0],# Z
           [0, 0, 4, 4],
           blank],
          [blank,
           [0, 5, 5, 5],# L
           [0, 5, 0, 0],
           blank],
          [blank,
           [0, 6, 6, 6],# J
           [0, 0, 0, 6],
           blank],
          [blank,
           [0, 7, 7, 7],# T
           [0, 0, 7, 0],
           blank]
         ]

# 画像データの作成
blockimage = [[]]
(1..7). each do |i| # ブロックの絵
  # ブロックの種類が多いので計算で色を作ってみた
  # こんなことするより画像ファイルから読むほうをオススメします
  data = Image.new(16, 16, [(i / 4)*220, ((i % 4) / 2)*220, (i % 2)*220])
  data.line(0, 0, 14, 0, [(i / 4)*128+127, ((i % 4) / 2)*128+127, (i % 2)*128+127])
  data.line(0, 0, 0, 14, [(i / 4)*128+127, ((i % 4) / 2)*128+127, (i % 2)*128+127])
  data.line(15, 1, 15, 15, [(i / 4)*128, ((i % 4) / 2)*128, (i % 2)*128])
  data.line(1, 15, 15, 15, [(i / 4)*128, ((i % 4) / 2)*128, (i % 2)*128])
  blockimage.push(data)
end

blockimage.push(Image.new(16, 16, [100,100,100])) # 壁の絵

# フィールドデータ
$field = Array.new(20){[8] + [0]*10 + [8]*2}.push([8]*13).push([8]*13)

# 4*4の配列を指定回数だけ右回転
def rotation(kind, angle)
  b = BLOCK[kind]
  angle = angle + 4 if angle < 0  # angleが-1になることもある
  angle.times do
    a = [[], [], [], []]
    for y in 0..3 do
      for x in 0..3 do
        a[x].push(b[3 - y][x])
      end
    end
    b = a
  end
  return b
end

# 当たり判定
def check(b, x, y)
  for i in 0..3 do
    for j in 0..3 do
      if b[i][j] != 0 and $field[(y+15) / 16 + i][x / 16 + j] != 0 then
        return true
      end
    end
  end
  return false
end

x = 0
y = 0
kind = 0
angle = 0
state = 2

# キーのオートリピート設定
Input.setRepeat(10,2)

# ウィンドウサイズ設定
Window.width = 192
Window.height = 336

# フォント作成
font = Font.new(16)
fontback = Image.new(192, 32, [100,100,100])

# メインループ
Window.loop do
  case state
  when 0 # 初期化
    x = 64 # 横
    y = 0  # 縦
    kind = rand(7)+1 # ブロックの種類
    angle = 0      # 角度
    $field = Array.new(20){[8] + [0]*10 + [8]*2}.push([8]*13).push([8]*13)
    state = 1

  when 1
    # 回転操作
    if Input.padPush?(P_BUTTON0) and check(rotation(kind, angle-1), x, y) == false then
      if angle == 0 then
        angle = 3
      else
        angle = angle - 1
      end
    end
    if Input.padPush?(P_BUTTON1) and check(rotation(kind, angle+1), x, y) == false then
      if angle == 3 then
        angle = 0
      else
        angle = angle + 1
      end
    end

    b = rotation(kind, angle) # 回転したデータを作成

    # ブロックの移動
    x = x - 16 if Input.padPush?(P_LEFT) and check(b, x-16, y) == false
    x = x + 16 if Input.padPush?(P_RIGHT) and check(b, x+16, y) == false
    y = y + 1
    y = y + 3 if Input.padDown?(P_DOWN)

    # 当たり判定
    if check(b, x, y) == true then # 当たっていたら
      for i in 0..3 do
        for j in 0..3 do  # フィールド配列に設定
          $field[(y+15) / 16 + i - 1][x / 16 + j] += b[i][j]
        end
      end

      for i in 0..19 do # 揃った行を削除&上に空行追加
        if $field[i].index(0) == nil then
          $field.delete_at(i)
          $field.unshift([-1] + [0]*10 + [-1]*2)
        end
      end

      # 位置の初期化
      x = 64
      y = 0
      kind = rand(7)+1
      angle = 0

      # ゲームオーバー判定
      b = BLOCK[kind]
      if check(b, x, y) == true then
        state = 2
        for i in 0..3 do
          for j in 0..3 do  # フィールド配列に設定
            $field[(y+15) / 16 + i][x / 16 + j] = b[i][j] if b[i][j] != 0
          end
        end
      end
    else  # 当たっていなかったら
      # 画像描画
      for i in 0..3 do
        for j in 0..3 do
          Window.draw(j * 16 + x, i * 16 + y, blockimage[kind]) if b[i][j] != 0
        end
      end
    end

  when 2 # スペース入力待ち
    Window.draw(0, 92, fontback, 9)
    Window.drawFont(64, 100, "push space", font, :color=>[255,255,255,255],:z=>10)
    state = 0 if Input.keyPush?(K_SPACE)
  end

  # フィールド描画
  for i in 0..20 do
    for j in 0..11 do
      Window.draw(j * 16, i * 16, blockimage[$field[i][j]]) if $field[i][j] != 0
    end
  end

  break if Input.keyDown?(K_ESCAPE)
end
