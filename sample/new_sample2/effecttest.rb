#!ruby -Ks
require 'dxruby'

# レンダリング済みフォントを作るクラス
class ImageFontMaker
  ImageFontData = Struct.new(:x, :y, :width, :ox)
  ImageFontSaveData = Struct.new(:data_hash, :image, :height)

  def initialize(size, fontname = nil)
    @font = Font.new(size, fontname)
    @hash = {}
  end

  def add_data(str)
    str.each_char do |c|
      @hash[c] = @font.info(c)
    end
  end

  def output(filename)
    # 必要な画像サイズを調べる
    # 計算がややこしいことになってるのはfやjなど左右にはみ出す特殊な文字の対応
    width = 0
    height = 0
    keys = @hash.keys.shuffle
    keys.each do |k|
      v = @hash[k]
      ox = v.gmpt_glyphorigin_x < 0 ? -v.gmpt_glyphorigin_x : 0
      cx = (v.gm_blackbox_x + v.gmpt_glyphorigin_x) > v.gm_cellinc_x + ox ? (v.gm_blackbox_x + v.gmpt_glyphorigin_x) : v.gm_cellinc_x + ox
      width += cx
      if width > 640
        height += @font.size
        width = cx
      end
    end
    height += @font.size

    # 画像生成
    image = Image.new(640, height, C_BLACK)

    # 文字描き込み
    x = 0
    y = 0
    data_hash = {}
    keys.each do |k|
      v = @hash[k]
      ox = v.gmpt_glyphorigin_x < 0 ? -v.gmpt_glyphorigin_x : 0
      cx = (v.gm_blackbox_x + v.gmpt_glyphorigin_x) > v.gm_cellinc_x + ox ? (v.gm_blackbox_x + v.gmpt_glyphorigin_x) : v.gm_cellinc_x + ox
      if x + cx > 640
        x = 0
        y += @font.size
      end
      image.draw_font(x + ox, y, k, @font)
      data_hash[k] = ImageFontData.new(x, y, cx, ox)
      x += cx
    end

    # いったん保存
    image.save(filename + ".png")

    # バイナリで読み込み
    image_binary = nil
    open(filename + ".png", "rb") do |fh|
      image_binary = fh.read
    end

    # マーシャルしてファイルへ
    imagefont = ImageFontSaveData.new(data_hash, image_binary, @font.size)

    open(filename, "wb") do |fh|
      fh.write(Marshal.dump(imagefont))
    end
  end
end

# レンダリング済みフォントを使うクラス
class ImageFont
  attr_accessor :target

  # 生成時のパラメータはdraw_font_exのハッシュ部分と同じ
  def initialize(filename)
    open(filename, "rb") do |fh|
      temp = Marshal.load(fh.read)
      @data_hash = temp.data_hash
      @image = Image.load_from_file_in_memory(temp.image)
      @height = temp.height
    end
    @cache = {}
  end

  # 文字列描画
  def draw(x, y, s, hash)
    rt = @target == nil ? Window : @target
    edge_width = hash[:edge_width] == nil ? 2 : hash[:edge_width]

    image_hash = nil
    if !@cache.has_key?(hash)
      image_hash = @cache[hash] = {}
    else
      image_hash = @cache[hash]
    end

    s.each_char do |c|
      temp = @data_hash[c]

      if !image_hash.has_key?(c)
        image = @image.slice(temp.x, temp.y, temp.width, @height)
        image_hash[c] = image.effect_image_font(hash)
        image.delayed_dispose
      end
      rt.draw(x - temp.ox - edge_width, y - edge_width, image_hash[c])
      x += temp.width - temp.ox
    end
  end
end

Window.bgcolor = [100,100,100]

# レンダリング済みフォント作成
imagefontmaker = ImageFontMaker.new(48, "ＭＳ Ｐゴシック")
imagefontmaker.add_data(" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefさghijklmnopqrstuvwxyzあいうえおかきくけこがぎぐげごしすせそざじずぜぞたちつてとだぢづでどなにぬねのはひふへほばびぶべぼぱぴぷぺぽまみむめもやゆよらりるれろわをんっぁぃぅぇぉゃゅょ、。")
imagefontmaker.output("ImageFont.dat")

# レンダリング済みフォント読み込み
imagefont = ImageFont.new("ImageFont.dat")

Window.loop do
  imagefont.draw(50, 60, "きょうはあさからよるだった。", :shadow=>true, :edge=>true, :edge_color=>C_CYAN)
  imagefont.draw(50, 108, "fあいうえおかきくけこ", :shadow=>false, :edge=>true, :edge_color=>C_CYAN)
  imagefont.draw(50, 156, "jさしすせそたちつてと", :shadow=>true, :edge=>false, :edge_color=>C_CYAN)
  imagefont.draw(50, 204, "kなにぬねのはひふへほ", :shadow=>true, :edge=>true, :edge_color=>C_RED)
end




