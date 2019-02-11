#!ruby -Ks
require "dxruby"

# しかく
class Box < Sprite
  def initialize(size)
    self.x = rand(639-size)
    self.y = rand(439-size)
    self.collision = [0, 0, size - 1, size - 1]
    self.image = Image.new(size, size, [255, 200, 0, 0])
  end
end

# まる
class Circle < Sprite
  def initialize(size)
    self.x = rand(639-size*2)
    self.y = rand(439-size*2)
    self.collision = [size, size, size]
    self.image = Image.new(size*2, size*2).circle_fill(size, size, size, [255, 0, 0, 200])
  end
end

# さんかく
class Triangle < Sprite
  def initialize(size)
    self.x = rand(639-size)
    self.y = rand(439-size)
    self.collision = [size/2,0,0,size-1,size-1,size-1]
    self.image = Image.new(size, size).triangle_fill(size/2,0,0,size-1,size-1,size-1, [255,0,200,0])
  end
end

font = Font.new(24)

# いまマウスで掴んでるオブジェクト
item = nil

# オブジェクト配列
arr = [Circle.new(20), Circle.new(40),
       Triangle.new(60), Triangle.new(100),
       Box.new(40), Box.new(80)]

# マウスカーソルの衝突判定用Sprite
mouse = Sprite.new
mouse.collision = [0,0]

# メインループ
Window.loop do
  oldx, oldy = mouse.x, mouse.y
  mouse.x, mouse.y = Input.mouse_pos_x, Input.mouse_pos_y

  # ボタンを押したら判定
  if Input.mouse_push?(M_LBUTTON) or Input.mouse_push?(M_RBUTTON)
    arr.each_with_index do |obj, i|
      if mouse === obj
        # オブジェクトをクリックできたら並べ替えとitem設定
        arr.delete_at(i)
        arr.unshift(obj)
        item = obj
        break
      end
    end
  end

  # ボタンを押している間の処理
  if Input.mouse_down?(M_RBUTTON) or Input.mouse_down?(M_LBUTTON)
    if item then
      if Input.mouse_down?(M_RBUTTON)
        item.angle += 5
      end
      if Input.mouse_down?(M_LBUTTON)
        item.x += mouse.x - oldx
        item.y += mouse.y - oldy
      end
    end
  else
    # ボタンが離されたらオブジェクトを解放
    item = nil
  end

  # オブジェクト同士の判定
  if Sprite.check(arr) then
    Window.draw_font(0, 0, "hit!", font)
  end

  # 描画
  arr.reverse.each do |obj|
    obj.draw
  end

  break if Input.key_push?(K_ESCAPE)
end
