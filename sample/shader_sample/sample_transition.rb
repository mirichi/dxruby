#!ruby -Ks
require 'dxruby'
require './shader/transition'

Window.width = 800
Window.height = 600

shader = []
# ルール画像が640*480、背景画像が800*600なのでルール画像を拡大してシェーダに設定する
# こんなメソッドチェインができてしまうという噂
# メソッドの戻り値は結構いい加減なので、できていいはずなのにできないメソッドがあったら連絡ください
shader << TransitionShader.new(100,
                               RenderTarget.new(800,600).
                                            draw_scale(0, 0, Image.load("./rule/右渦巻き.png"), 800/640.0, 600/480.0, 0, 0).
                                            update.
                                            to_image,
                               20)

# 小さい画像をそのまま食わせると画面全体に繰り返して使う
shader << TransitionShader.new(100,
                               Image.load("./rule/チェッカー.png"),
                               20)
shader << TransitionShader.new(100,
                               Image.load("./rule/横ブラインド.png"),
                               20)


# 背景画像
bg = [Image.load('./bgimage/BG10a_80.jpg'),
      Image.load('./bgimage/BG13a_80.jpg'),
      Image.load('./bgimage/BG00a1_80.jpg')
     ]

count = 100
mode = 0
image_new = bg[0]
image_old = nil

# スタート時はnewのみ表示
# Zを押したらnewがoldになって、oldが手前に描画され、トランジション処理される。
# newは3枚の画像を順番に。
Window.loop do
  if count < 100
    count += 1
    shader[mode].frame_count = count
  end

  if Input.key_down?(K_Z) && count == 100
    count = 0
    image_old = image_new
    mode = mode == 2 ? 0 : mode + 1
    image_new = bg[mode]
    shader[mode].start
  end

  Window.draw(0,0,image_new)
  Window.draw_shader(0,0,image_old, shader[mode]) if count < 100

  break if Input.key_push?(K_ESCAPE)
end


