# 簡易マップエディタ
require 'dxruby'
require './map'

# マップデータ
map = Array.new(32) { [1] * 32 }

# 絵のデータを作る
mapimage = []
mapimage.push(Image.new(32, 32, [100, 100, 200])) # 海
mapimage.push(Image.new(32, 32, [50, 200, 50]))   # 平地
mapimage.push(Image.new(32, 32, [50, 200, 50]).   # 木の根元
                        box_fill(13, 0, 18, 28, [200, 50, 50]))
mapimage.push(Image.new(32, 32, [50, 200, 50]).   # 山
                        triangle_fill(15, 0, 0, 31, 31, 31, [200, 100,100]))
mapimage.push(Image.new(32, 32).  # 木のあたま。背景は透明色にしておく。
                        box_fill(13, 16, 18, 31, [200, 50, 50]).
                        circle_fill(16, 10, 8, [0, 255, 0]))

wakuimage = Image.new(36, 36).box(0, 0, 35, 35, [255, 255, 255, 255]) # 選択枠

rt = RenderTarget.new(32*32, 32*32)

filename = Window.open_filename([["dat file(*.dat)", "*.dat"]], "open file name")
ext = File.extname(filename)
basename = File.basename(filename, ext)

map_base = Map.new(filename, mapimage, rt)
map_sub = Map.new(basename + "_sub" + ext, mapimage, rt)

select = 0

Window.height = 512

# メインループ
Window.loop do

  # マウス左クリック
  if Input.mouse_down?(M_LBUTTON) then
    x, y = Input.mouse_pos_x, Input.mouse_pos_y
    if x >= 512 then # 右のほうなら
      for i in 0..3
        if x >= 560 and x < 592 and y >= i * 64 + 64 and y < i * 64 + 96 then
          select = i
          break
        end
      end
    else            # 左のほうなら
      if x > 0 and y > 0 and y < 512 then
        map_base[x/16, y/16] = select
        map_sub[x/16, y/16-1] = (select == 2 ? 4 : nil)
      end
    end
  end

  # 選択枠描画
  Window.draw(558, select * 64 + 62, wakuimage)

  # マップチップ描画
  for i in 0..3
    Window.draw(560, i * 64 + 64, mapimage[i])
    Window.draw(560, 2 * 64 + 64 - 32, mapimage[4])
  end

  # マップをrtに描画
  map_base.draw(0, 0)
  map_sub.draw(0, 0)

  # 全体図描画
  Window.draw_ex(0, 0, rt, :center_x => 0, :center_y => 0, :scale_x => 0.5, :scale_y => 0.5)

  if Input.key_push?(K_ESCAPE) then
    filename = Window.save_filename([["dat file(*.dat)", "*.dat"]], "save file name")
    if filename
      ext = File.extname(filename)
      basename = File.basename(filename, ext)
      File.open(filename, "wt") do |fh|
        map_base.mapdata.each do |line|
          data = ""
          line.each do |o|
            data += o.to_s
          end
          fh.puts(data)
        end
      end
      File.open(basename + "_sub" + ext, "wt") do |fh|
        map_sub.mapdata.each do |line|
          data = ""
          line.each do |o|
            if o
              data += "4"
            else
              data += "x"
            end
          end
          fh.puts(data)
        end
      end
      break
    end
  end
end
