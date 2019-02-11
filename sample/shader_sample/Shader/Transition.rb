# -*- coding: Windows-31J -*-

class TransitionShader < DXRuby::Shader
  hlsl = <<EOS
  float g_min;
  float g_max;
  float2 scale;
  texture tex0;
  texture tex1;
  sampler Samp0 = sampler_state
  {
   Texture =<tex0>;
  };
  sampler Samp1 = sampler_state
  {
   Texture =<tex1>;
   AddressU = WRAP;
   AddressV = WRAP;
  };

  struct PixelIn
  {
    float2 UV : TEXCOORD0;
  };
  struct PixelOut
  {
    float4 Color : COLOR0;
  };

  PixelOut PS(PixelIn input)
  {
    PixelOut output;
    output.Color = tex2D( Samp0, input.UV );
    output.Color.a *= smoothstep(g_min, g_max, tex2D( Samp1, input.UV * scale ).r );

    return output;
  }

  technique Transition
  {
   pass P0
   {
    PixelShader = compile ps_2_0 PS();
   }
  }
EOS

  @@core = DXRuby::Shader::Core.new(
    hlsl,
    {
      :g_min => :float,
      :g_max => :float,
      :scale => :float, # HLSL側がfloat2の場合は:floatを指定して[Float, Flaot]という形で渡す
      :tex1 => :texture,
    }
  )

  # durationは遷移にかけるフレーム数、imageはルール画像のImageオブジェクト(省略でクロスフェード)、vagueは曖昧さ
  def initialize(duration, image=nil, vague=nil)
    super(@@core, "Transition")
    if image
      @image = image
      @vague = vague == nil ? 40 : vague
    else
      @image = DXRuby::Image.new(1, 1, [0,0,0])
      @vague = 256
    end
    @duration = duration
    @count = 0
    self.g_min = -@vague.fdiv(@duration)
    self.g_max = 0.0
    self.tex1   = @image
    self.scale  = [DXRuby::Window.width.fdiv(@image.width), DXRuby::Window.height.fdiv(@image.height)]
    # シェーダパラメータ設定のメソッドは外部非公開とする
    class << self
      protected :g_min, :g_min=, :g_max, :g_max=, :scale, :scale=, :tex1, :tex1=
    end
  end

  def start(duration = nil)
    @duration = duration if duration
    @count = 0
    self.g_min = -@vague.fdiv(@duration)
    self.g_max = 0.0
  end

  def next
    @count += 1
    self.g_min = (((@vague+@duration).fdiv(@duration)) * @count - @vague).fdiv(@duration)
    self.g_max = (((@vague+@duration).fdiv(@duration)) * @count).fdiv(@duration)
  end

  def frame_count=(c)
    @count = c
    self.g_min = (((@vague+@duration).fdiv(@duration)) * @count - @vague).fdiv(@duration)
    self.g_max = (((@vague+@duration).fdiv(@duration)) * @count).fdiv(@duration)
  end

  def frame_count
    @count
  end

  # 描画対象を指定する。RenderTargetオブジェクトか、Windowを渡す。デフォルトはWindow。
  def target=(t)
    self.scale = [t.width.fdiv(@image.width), t.height.fdiv(@image.height)]
  end
end

