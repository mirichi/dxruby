#!ruby -Ks
# DXRuby サンプル ８パズル
# 左クリックでピースを移動、右クリックでシャッフル
# 使用可能な画像はjpg、png、bmpなど通常のフォーマットです。
require 'dxruby'

filename = Window.openFilename([["ｽﾍﾞﾃﾉﾌｧｲﾙ(*.*)", "*.*"]], "パズル用の画像を選択")
image = Image.loadToArray(filename, 3, 3)

# 窓設定
width = image[0].width
height = image[0].height
if width < 32 then
  Window.scale = 48.0 / width
end
Window.width   = image[0].width * 3
Window.height  = image[0].height * 3
Window.caption = "８パズル"

# パズルの情報
piece = [0,1,2,3,4,5,6,7,8]

# クリック時の処理。空いてる位置のとなりだったら入れ替える。
def click(x, y, piece)
  i = piece.index(8) # 空いてるピース検索
  if (((i % 3) - x).abs == 1 and (i / 3) == y) or (((i / 3) - y).abs == 1 and (i % 3) == x) then
    piece[x + y * 3], piece[i] = piece[i], piece[x + y * 3] # 入れ替え a,b = b,aでswapできる
  end
end

# メインループ
Window.loop do

  # 右クリックでシャッフル（ランダムで1000回ほどクリックしてもらってます^^;)
  if Input.mousePush?(M_RBUTTON) then
    1000.times do
      click(rand(3), rand(3), piece)
    end
  end

  # 左クリック処理
  if Input.mousePush?(M_LBUTTON) then
    click((Input.mousePosX / Window.scale / image[0].width).to_i, (Input.mousePosY / Window.scale / image[0].height).to_i, piece)
  end

  # 完了判定と描画
  for i in 0..8
    if piece[i] != 8 or piece == [0,1,2,3,4,5,6,7,8] then
      Window.draw(i % 3 * image[0].width , i / 3 * image[0].height, image[piece[i]])
    end
  end

  # エスケープキーで終了
  break if Input.keyPush?(K_ESCAPE)
end
