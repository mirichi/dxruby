#!ruby -Ks
# DXRuby サンプル ミニレーシング
# 左右で回転、Zで加速、Xでバック。
require 'dxruby'

Window.caption = "ミニレーシング"

# コースデータ
line1 = [53, 430, 568, 438, 568, 438, 607, 261, 607, 261, 520, 200, 520, 200, 531, 151,
 531, 151, 602, 115, 602, 115, 562, 19, 562, 19, 113, 21, 113, 21, 34, 83,
 34, 83, 81, 180, 81, 180, 221, 207, 221, 207, 251, 274, 251, 274, 204, 299,
 204, 299, 61, 262, 61, 262, 26, 358, 26, 358, 53, 430]
line2 = [93, 330, 88, 380, 88, 380, 479, 392, 479, 392, 536, 302, 536, 302, 534, 267,
 534, 267, 461, 210, 461, 210, 459, 151, 459, 151, 518, 91, 518, 91, 487, 53,
 487, 53, 156, 77, 156, 77, 125, 110, 125, 110, 292, 197, 292, 197, 311, 268,
 311, 268, 227, 342, 227, 342, 93, 330]

# コース作成
$course = Image.new(640, 480, [0, 200, 0])
for i in 0..(line1.size/2-2)
  $course.line(line1[i*2], line1[i*2+1], line1[i*2+2], line1[i*2+3], [128, 128, 128])
end
for i in 0..(line2.size/2-2)
  $course.line(line2[i*2], line2[i*2+1], line2[i*2+2], line2[i*2+3], [128, 128, 128])
end

# 自前で塗りつぶし
Green = [0, 200, 0]
Gray = [128, 128, 128]
def paint(x, y, col)
  arr = []
  loop do
    px = x
    flag = [1, 1]
    return if y < 0 or y > 479
    while px > 0 and $course.compare(px-1, y, Green) do # 左端まで検索
      px = px - 1
    end
    lx = px
    while px <= 639 and $course.compare(px, y, Green) do # 右端まで塗る
      (0..1).each do |i|
        if $course.compare(px, y+i*2-1, Green) then
          if flag[i] == 1 then
            arr.push([px, y+i*2-1, col])        # 上下に空きがあれば再帰で塗る登録
            flag[i] = 0
          end
        else
          flag[i] = 1
        end
      end
      px = px + 1
    end
    $course.line(lx, y, px-1, y, Gray)

    if arr.size != 1 then
      break
    end
    x = arr[0][0]
    y = arr[0][1]
    col = arr[0][2]
    arr = []
  end

  arr.each do |ax, ay, acol|
    paint(ax,ay,acol) # 再帰呼び出し
  end

end

paint(200,300,[128,128,128])

# 車データ
carimage = Image.new(8,8)
carimage.boxFill(0, 1, 7, 6, [255, 0, 0]).boxFill(5, 2, 6, 5, [0, 0, 255]).
         line(0, 0, 1, 0, [0, 0, 0]).line(5, 0, 6, 0, [0, 0, 0]).
         line(0, 7, 1, 7, [0, 0, 0]).line(5, 7, 6, 7, [0, 0, 0])

x = 200
y = 300
angle = 0
speed = 0
dx = 0
dy = 0

Window.scale = 2
Window.width = 200
Window.height = 200

image = Image.new(200, 200, Green)

Window.loop do
  # 移動処理
  speed = speed + 0.1 if Input.padDown?(P_BUTTON0) and speed < 4
  speed = speed - 0.1 if Input.padDown?(P_BUTTON1) and speed > -2
  angle = angle - 45 if Input.padPush?(P_LEFT)
  angle = angle + 45 if Input.padPush?(P_RIGHT)

  dx = dx * 0.95 + Math.cos(Math::PI / 180 * angle) * speed * 0.05
  dy = dy * 0.95 + Math.sin(Math::PI / 180 * angle) * speed * 0.05
  x = x + dx
  y = y + dy

  # imageオブジェクト編集
  image.copyRect(0, 0, $course, x, y, 200, 200)

  # 描画
  Window.draw(0, 0, image)
  Window.drawRot(96, 96, carimage, angle)

  if image.compare(100, 100, Green) then
    dx = -dx
    dy = -dy
    x = x + dx
    y = y + dy
    speed = 0.5
  end

  break if Input.keyPush?(K_ESCAPE)
end
