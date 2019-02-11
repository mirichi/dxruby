# -*- coding: Windows-31J -*-

class FlashShader < DXRuby::Shader
  hlsl = <<EOS
// (1) グローバル変数
  float3 g_color;
  float g_level;
  texture tex0;

// (2) サンプラ
  sampler Samp0 = sampler_state
  {
   Texture =<tex0>;
   AddressU=BORDER;
   AddressV=BORDER;
  };

// (3) 入出力の構造体
  struct PixelIn
  {
    float2 UV : TEXCOORD0;
  };
  struct PixelOut
  {
    float4 Color : COLOR0;
  };

// (4) ピクセルシェーダのプログラム
  PixelOut PS(PixelIn input)
  {
    PixelOut output;
    output.Color.rgb = tex2D( Samp0, input.UV ).rgb * g_level + g_color * (1.0-g_level);
    output.Color.a = tex2D( Samp0, input.UV ).a;

    return output;
  }

// (5) technique定義
  technique Flash
  {
   pass P0 // パス
   {
    PixelShader = compile ps_2_0 PS();
   }
  }
EOS

  @@core = DXRuby::Shader::Core.new(
    hlsl,
    {
      :g_color => :float,
      :g_level => :float,
    }
  )


  # colorはフラッシュ色、durationは遷移にかけるフレーム数
  def initialize(duration, color = [255, 255, 255])
    super(@@core, "Flash")
    self.color = color
    self.g_level = 1.0
    @duration = duration
    @count = 0

    # シェーダパラメータ設定のメソッドは外部非公開とする
    class << self
      protected :g_color, :g_color=, :g_level, :g_level=
    end
  end

  def color=(c)
      raise DXRuby::DXRubyError, "色配列が不正です" if c.size != 3
      self.g_color = c.map{|v| v.fdiv(255)}
  end

  def duration=(d)
    @duration = d
  end

  def start
    @count = 0
    self.g_level = 0.0
  end

  def next
    @count += 1
    if @count > @duration
      self.g_level = 1.0
    else
      self.g_level = @count.fdiv(@duration)
    end
  end
end
