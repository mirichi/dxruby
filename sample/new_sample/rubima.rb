#!ruby -Ks
# るびまサンプルゲーム
require 'dxruby'

# 背景描画
class Map
  @@map = [[0, 0, 0, 0, 0, 0, 0, 0, 29, 11, 11, 30, 34, 66, 67, 67],
           [0, 0, 0, 24, 25, 26, 0, 0, 29, 11, 11, 39, 40, 6, 34, 34],
           [0, 0, 24, 17, 31, 35, 0, 0, 12, 20, 11, 11, 11, 39, 40, 40],
           [0, 24, 17, 34, 7, 44, 0, 28, 28, 29, 11, 11, 11, 11, 11, 11],
           [0, 33, 31, 34, 35, 0, 28, 3, 37, 38, 11, 11, 11, 18, 19, 19],
           [0, 42, 43, 43, 44, 28, 3, 38, 11, 11, 11, 18, 19, 13, 28, 28],
           [0, 0, 0, 0, 3, 37, 38, 11, 11, 18, 19, 13, 28, 28, 28, 0],
           [0, 0, 0, 3, 38, 11, 11, 11, 18, 13, 28, 28, 51, 52, 52, 52],
           [0, 0, 3, 38, 11, 11, 18, 19, 13, 51, 52, 52, 86, 58, 61, 76],
           [28, 0, 29, 11, 11, 18, 13, 28, 51, 86, 58, 58, 61, 61, 58, 62],
           [0, 28, 29, 11, 18, 13, 28, 0, 60, 58, 61, 61, 61, 61, 76, 71],
           [0, 28, 29, 11, 27, 28, 28, 51, 86, 61, 61, 58, 76, 70, 71, 0],
           [0, 0, 29, 11, 36, 4, 28, 60, 58, 61, 58, 76, 71, 0, 1, 2],
           [0, 28, 29, 11, 11, 36, 4, 69, 70, 70, 70, 71, 0, 1, 2, 0],
           [0, 0, 12, 20, 11, 11, 27, 0, 1, 0, 1, 1, 1, 2, 2, 0],
           [0, 0, 28, 12, 20, 11, 27, 0, 0, 0, 2, 2, 0, 2, 2, 0],
           [0, 0, 0, 2, 29, 11, 27, 1, 2, 2, 2, 0, 0, 2, 2, 2],
           [0, 0, 0, 2, 29, 11, 27, 1, 0, 1, 1, 2, 2, 0, 0, 2],
           [0, 0, 0, 0, 29, 11, 27, 1, 0, 2, 2, 2, 1, 1, 2, 2],
           [0, 45, 47, 2, 29, 11, 36, 4, 1, 2, 2, 0, 0, 2, 2, 0],
           [45, 82, 56, 0, 29, 11, 11, 36, 4, 1, 2, 2, 2, 2, 0, 0],
           [54, 0, 56, 0, 12, 20, 11, 11, 36, 37, 4, 0, 2, 2, 2, 2],
           [54, 55, 81, 46, 47, 12, 20, 11, 11, 11, 36, 4, 1, 1, 1, 2],
           [54, 55, 0, 0, 56, 0, 12, 19, 20, 11, 11, 36, 37, 4, 1, 1],
           [54, 0, 55, 55, 56, 0, 0, 0, 12, 20, 11, 11, 11, 36, 37, 37],
           [63, 73, 55, 55, 56, 0, 0, 2, 2, 29, 11, 11, 11, 11, 11, 11],
           [0, 54, 0, 55, 81, 47, 0, 2, 3, 38, 11, 11, 11, 11, 11, 11],
           [0, 54, 0, 0, 55, 56, 2, 0, 29, 11, 11, 11, 21, 22, 22, 22],
           [0, 63, 64, 64, 64, 65, 0, 0, 29, 11, 11, 21, 15, 48, 49, 49],
           [0, 0, 0, 0, 0, 0, 0, 0, 29, 11, 11, 30, 34, 57, 34, 34],
          ]
  temp = Image.load("image/maptile.png")
  x = temp.width / 32
  y = temp.height / 32
  @@images = temp.slice_tiles(x, y, true)

  # 初期化
  def initialize
    @y = 14 * 32    # マップの初期位置
    @count = 0      # 1チップぶん移動するフレーム数
  end

  # マップ更新
  def update
    @count -= 1
  end

  # マップ描画
  def draw
    $rt.draw_tile(0, 0, @@map, @@images, 0, @y + @count, 16, 16, 0)
  end
end

# 敵１のやられ処理
class Enemy1bomb < Sprite
  @@image0 = Image.load_tiles("image/enemy1bomb.png", 4, 2, true)
  @@image1 = @@image0.map {|image| image.flush([128, 0, 0, 0])}

  # インスタンス初期化
  def initialize(x, y)
    super(x, y)
    self.z = 10
    self.target = $rt
    @count = 0
  end

  # 更新
  def update
    self.y += 1.5
    @count += 1
    if @count >= 40
      self.vanish
    end
  end

  # 描画
  def draw
    self.image = @@image0[@count / 5]
    super
    $rt.draw(self.x-16, self.y-16, @@image1[@count / 5], 1)
  end
end

# 敵２のやられ処理
class Enemy2bomb < Sprite
  @@image0 = Image.load_tiles("image/enemy2bomb.png", 4, 2, true)
  @@image1 = @@image0.map {|image| image.flush([128, 0, 0, 0])}

  # インスタンス初期化
  def initialize(x, y)
    super(x, y)
    self.z = 10
    self.target = $rt
    @count = 0
  end

  # 更新
  def update
    self.y += 0.5
    @count += 1
    if @count >= 40
      self.vanish
    end
  end

  # 描画
  def draw
    self.image = @@image0[@count / 5]
    super
    $rt.draw(self.x-16, self.y-16, @@image1[@count / 5], 1)
  end
end

# 敵１用ショットヒット後
class EnemyShot1Hit < Sprite
  @@image = Image.load("image/enemyshot1.png")

  # インスタンス初期化
  def initialize(x, y, angle)
    super(x, y, @@image)
    self.z = 20
    self.alpha = 255
    self.target = $rt
    temp2 = angle + 180
    @dx = Math.cos((temp2) / 180.0 * Math::PI)
    @dy = Math.sin((temp2) / 180.0 * Math::PI)
  end

  # 更新
  def update
    self.x += @dx
    self.y += @dy
    self.alpha -= 10

    if self.alpha < 0
      self.vanish
    end
  end
end

# 敵１用ショット
class EnemyShot1 < Sprite
  @@image = Image.load("image/enemyshot1.png")
  @@sound = Array.new(3) do
    v = 60
    SoundEffect.new(60,WAVE_TRI) do
      v = v - 1
      [800, v]
    end
  end
  @@soundnumber = 0
  @@soundflag = false

  # インスタンス初期化
  def initialize(x, y, angle)
    super(x, y, @@image)
    self.z = 20
    self.collision = [4, 4, 11, 11]
    self.target = $rt
    @dx = Math.cos(angle / 180.0 * Math::PI) * 1.5
    @dy = Math.sin(angle / 180.0 * Math::PI) * 1.5
    @shot_angle = angle
  end

  # 更新
  def update
    self.x += @dx
    self.y += @dy
    @@soundflag = false
  end

  def shot(obj)
    self.vanish
    $etc_objects << EnemyShot1Hit.new(self.x, self.y, @shot_angle)
    if @@soundflag == false
      @@sound[@@soundnumber].play
      @@soundnumber += 1
      @@soundnumber = 0 if @@soundnumber > 2
      @@soundflag = true
    end
  end
end

# 敵２用ショットヒット後
class EnemyShot2Hit < Sprite
  @@image = Image.load("image/enemyshot2.png")

  # インスタンス初期化
  def initialize(x, y, angle)
    super(x, y, @@image)
    self.z = 20
    self.alpha = 255    # アルファ値
    self.target = $rt
    temp2 = angle + 180
    @dx = Math.cos((temp2) / 180.0 * Math::PI) * 3.5
    @dy = Math.sin((temp2) / 180.0 * Math::PI) * 3.5
  end

  # 更新
  def update
    # 移動
    self.x += @dx
    self.y += @dy
    self.alpha -= 10
    if self.alpha < 0
      self.vanish
    end
  end
end

# 敵２用ショット
class EnemyShot2 < Sprite
  @@image = Image.load("image/enemyshot2.png")
  v = 60
  @@sound = Array.new(3) do
    v = 60
    SoundEffect.new(60,WAVE_TRI) do
      v = v - 1
      [600, v]
    end
  end
  @@soundnumber = 0
  @@soundflag = false

  # インスタンス初期化
  def initialize(x, y, angle)
    super(x, y, @@image)
    self.z = 20
    self.collision = [6, 6, 17, 17]
    self.target = $rt
    @dx = Math.cos(angle / 180.0 * Math::PI) * 3.5
    @dy = Math.sin(angle / 180.0 * Math::PI) * 3.5
    @shot_angle = angle
  end

  # 更新
  def update
    # 移動
    self.x += @dx
    self.y += @dy
    @@soundflag = false
  end

  # 自機に当たったときに呼ばれるメソッド
  def shot(obj)
    self.vanish
    $etc_objects << EnemyShot2Hit.new(self.x, self.y, @shot_angle)
    if @@soundflag == false
      @@sound[@@soundnumber].play
      @@soundnumber += 1
      @@soundnumber = 0 if @@soundnumber > 2
      @@soundflag = true
    end
  end
end

# 敵１
class Enemy1 < Sprite
  # 画像読み込み＆フラッシュ/影画像生成
  image0 = Image.load_tiles("image/enemy1.png", 4, 1, true)  # 読み込み
  image1 = image0.map {|image| image.flush([255, 200, 200, 200])}
  image2 = image0.map {|image| image.flush([128, 0, 0, 0])}
  @@image = [image0, image1, image2]

  # SoundEffectでやられ効果音生成。３つまでの多重再生ができるよう配列化。
  @@sound = Array.new(3) do
    v = 60
    f = 500
    SoundEffect.new(120,WAVE_SAW) do
      v = v - 0.5
      f = f - 1
      [f, v]
    end
  end
  @@soundnumber = 0
  @@soundflag = false

  # インスタンス初期化
  def initialize(x, y)
    super(x, y)
    self.z = 10
    self.collision = [0, 0, 47, 47] # 衝突判定
    self.target = $rt
    @hp = 15         # ヒットポイント
    @shotcount = 0   # 弾を撃つ間隔を測るカウンタ
    @imagenumber = 0 # 被弾したら1(フラッシュを表す)
    @animecount = 0  # アニメーション用カウンタ
  end

  # 更新
  def update
    # 移動
    self.y += 2

    # 弾を撃つ判定
    @shotcount += 1
    if @shotcount > 40
      # 角度計算
      angle = (Math.atan2($myship.y + 16 - (self.y + 24 + 8), $myship.x + 16 - (self.x + 16 + 8)) / Math::PI * 180)

      # 弾を撃つ
      $enemy_shots << EnemyShot1.new(self.x + 16, self.y + 24, angle)

      # カウント初期化
      @shotcount = 0
    end

    # とりあず毎フレーム、フラッシュしていない状態にする
    @imagenumber = 0

    # アニメーション用カウント
    @animecount += 1
    @animecount -= 80 if @animecount >= 80

    @@soundflag = false
  end

  # 自機ショットに当たったときに呼ばれるメソッド
  def hit(obj)
    # HPを減らす
    @hp = @hp - obj.damage

    # やられ処理
    if @hp <= 0
      self.vanish
      $etc_objects << Enemy1bomb.new(self.x, self.y)

      # やられ効果音の多重再生
      if @@soundflag == false
        @@sound[@@soundnumber].play
        @@soundnumber += 1
        @@soundnumber = 0 if @@soundnumber > 2
        @@soundflag = true
      end
    end

    # フラッシュ中にする
    @imagenumber = 1
  end

  def draw
    self.image = @@image[@imagenumber][@animecount / 20]
    super
    $rt.draw(self.x-16, self.y-16, @@image[2][@animecount / 20], 1)
  end
end

# 敵２
class Enemy2 < Sprite
  # 画像読み込み＆フラッシュ/影画像生成
  image0 = Image.load_tiles("image/enemy2.png", 4, 1, true)  # 読み込み
  image1 = image0.map {|image| image.flush([255, 200, 200, 200])}
  image2 = image0.map {|image| image.flush([128, 0, 0, 0])}
  @@image = [image0, image1, image2]
  v = 80
  @@sound = SoundEffect.new(4000,WAVE_RECT, 5000) do
    v = v - 0.03
    [rand(300), v]
  end

  # インスタンス初期化
  def initialize(x, y)
    super(x, y)
    self.z = 10
    self.collision = [0, 0, 127, 63] # 衝突判定
    self.target = $rt
    @dy = 10 # 縦移動量
    @hp = 400 # ヒットポイント
    @shotcount = 0   # 弾を撃つ間隔を測るカウンタ
    @imagenumber = 0 # 被弾したら1(フラッシュを表す)
    @animecount = 0  # アニメーション用カウンタ
  end

  # 更新
  def update
    # 移動
    self.y += @dy

    if @dy > 0         # 下に移動中
      @dy -= 0.3         # 減速
    else               # 移動完了していたら
      @shotcount += 1    # ショットカウントを足して
      if @shotcount > 60 # カウントが60を超えたら弾を撃つ
        # 角度計算
        angle = (Math.atan2($myship.y + 16 - (self.y + 40 + 12), $myship.x + 16 - (self.x + 56 + 12)) / Math::PI * 180)
        # 5発撃つ
        $enemy_shots << EnemyShot2.new(self.x + 56, self.y + 40, angle - 45)
        $enemy_shots << EnemyShot2.new(self.x + 56, self.y + 40, angle - 22.5)
        $enemy_shots << EnemyShot2.new(self.x + 56, self.y + 40, angle)
        $enemy_shots << EnemyShot2.new(self.x + 56, self.y + 40, angle + 22.5)
        $enemy_shots << EnemyShot2.new(self.x + 56, self.y + 40, angle + 45)
        # カウント初期化
        @shotcount = 0
      end
    end

    # とりあず毎フレーム、フラッシュしていない状態にする
    @imagenumber = 0

    # アニメーション用カウント
    @animecount += 1
    @animecount -= 40 if @animecount >= 40
  end

  # 自機ショットが当たったときに呼ばれるメソッド
  def hit(obj)
    # HPを減らす
    @hp = @hp - obj.damage

    # やられ処理
    if @hp <= 0
      self.vanish
      $etc_objects << Enemy2bomb.new(self.x, self.y)
      @@sound.play
    end

    # フラッシュ中にする
    @imagenumber = 1
  end

  # 描画
  def draw
    self.image = @@image[@imagenumber][@animecount / 10]
    super
    $rt.draw(self.x-16, self.y-16, @@image[2][@animecount / 10], 1)       # 影
  end
end

# 自機用ショット
class MyShot < Sprite
  @@image = Image.load("image/myshot.png")
  attr_accessor :damage

  # インスタンス初期化
  def initialize(x, y, angle)
    super(x, y, @@image)
    self.z = 15
    self.collision = [0, 0, 31, 31] # 衝突判定
    self.angle = angle + 90 # 角度
    self.target = $rt
    @dx = Math.cos(angle / 180.0 * Math::PI) * 16 # 横移動量
    @dy = Math.sin(angle / 180.0 * Math::PI) * 16 # 縦移動量
    @damage = 5     # 敵に当たったときに与えるダメージ
  end

  # 更新
  def update
    # ショット移動
    self.x += @dx
    self.y += @dy
  end

  # 敵に当たったときに呼ばれるメソッド
  def shot(obj)
    self.vanish
  end
end

# 自機
class MyShip < Sprite

  # 画像読み込みと影画像生成
  @@image0 = Image.load_tiles("image/myship.png", 4, 1, true)
  @@image1 = @@image0.map {|image| image.flush([128, 0, 0, 0])}

  # ショット音生成
  f = 4000
  @@sound = SoundEffect.new(20, WAVE_TRI) do   # 20ms の三角波を生成する
    f = f - 120      # 周波数は 4000Hz から 1ms ごとに 120Hz 下げる
    [f, 15]          # [ 周波数, 音量 ] の配列を返す
  end

  # 初期化処理
  def initialize
    super(200, 400)
    self.z = 15
    self.collision = [4, 4, 27, 27]  # 衝突判定
    self.target = $rt
    @animecount = 0   # アニメーション用カウント

  end

  def update
    # 移動
    dx = Input.x * 3
    dy = Input.y * 3
    if Input.x != 0 and Input.y != 0   # ナナメの時は 0.7 倍
      dx *= 0.7
      dy *= 0.7
    end
    self.x += dx
    self.y += dy

    # 画面端の判定
    self.x = 0 if self.x < 0
    self.x = 448 - 32 if self.x > 448 - 32
    self.y = 0 if self.y < 0
    self.y = 480 - 32 if self.y > 480 - 32

    # ショット
    if Input.pad_push?(P_BUTTON0)
      $my_shots << MyShot.new(self.x - 18, self.y - 32, 270)
      $my_shots << MyShot.new(self.x + 18, self.y - 32, 270)
      $my_shots << MyShot.new(self.x + 32, self.y - 16, 300)
      $my_shots << MyShot.new(self.x - 32, self.y - 16, 240)
      @@sound.play
    end

    # アニメーション用カウント
    @animecount += 1
    @animecount -= 80 if @animecount >= 80
  end

  # 描画
  def draw
    self.image = @@image0[@animecount / 20]
    super
    $rt.draw(self.x - 16, self.y - 16, @@image1[@animecount / 20], 1)  # 影
  end
end

# ↑ここまでがクラス定義

# ↓ここからがゲームのメイン処理

Window.caption = "るびま用サンプルゲームのDXRuby1.4版" # ウィンドウのキャプション設定
Window.width = 360        # ウィンドウの横サイズ設定
Window.height = 480       # ウィンドウの縦サイズ設定
Input.set_repeat(0, 5)     # キーのオートリピート設定。5 フレームに 1 回 on

$etc_objects = []          # オブジェクト配列
$my_shots = []              # 弾
$enemies = []              # 敵
$enemy_shots = []          # 敵の弾

$rt = RenderTarget.new(448,480)
screen_sprite = Sprite.new(0, 0)
screen_sprite.collision = [0, 0, 448, 480]

count = 0                 # 敵出現処理用カウント
font = Font.new(32)       # フォント生成

$myship = MyShip.new      # 自機生成
$etc_objects << $myship  # 自機をオブジェクト配列に追加
$etc_objects << Map.new  # 背景オブジェクト生成＆オブジェクト配列に追加

# メインループ
Window.loop do

  # 敵出現処理
  count += 1
  if count % 20 == 0      #  20 カウントに 1 回
    if count % 400 == 0   # 400 カウントに 1 回
      # 敵 2 のオブジェクト生成＆オブジェクト配列に追加
      $enemies << Enemy2.new(rand(240), -64)
      count = 0
    else
      # 敵 1 のオブジェクト生成＆オブジェクト配列に追加
      $enemies << Enemy1.new(rand(320), -48)
    end
  end

  # オブジェクト情報更新
  Sprite.update([$etc_objects, $my_shots, $enemies, $enemy_shots])

  # 画面から出たやつを消す
  $my_shots = screen_sprite.check($my_shots)
  $enemies = screen_sprite.check($enemies)
  $enemy_shots = screen_sprite.check($enemy_shots)

  # 衝突判定
  Sprite.check($my_shots, $enemies)     # 自機ショットと敵
  Sprite.check($enemy_shots, $myship)   # 敵ショットと自機

  # 衝突判定で消えたキャラを配列から削除
  Sprite.clean([$etc_objects, $my_shots, $enemies, $enemy_shots])

  # オブジェクトを描画
  Sprite.draw([$etc_objects, $my_shots, $enemies, $enemy_shots])

  $rt.update
  Window.draw(-$myship.x/5,0,$rt)

  # Esc キーで終了
  break if Input.key_push?(K_ESCAPE)

  # 各種情報出力
  Window.draw_font(0, 0, Window.get_load.to_i.to_s + " %", font, :z => 100)
  Window.draw_font(0, 32, [$etc_objects, $my_shots, $enemies, $enemy_shots].flatten.size.to_s + " objects", font, :z => 100)
end

