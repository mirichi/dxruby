# -*- coding: Windows-31J -*-

class RasterScrollShader < DXRuby::Shader
hlsl = <<EOS
float g_start;
float g_level;
texture tex0;
sampler Samp = sampler_state
{
 Texture =<tex0>;
 AddressU = CLAMP;
 AddressV = CLAMP;
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
  input.UV.x = input.UV.x + sin(radians(input.UV.y*360-g_start))*g_level;
  output.Color = tex2D( Samp, input.UV );

  return output;
}

technique Raster
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
      :g_start => :float,
      :g_level => :float,
    }
  )

  attr_accessor :speed, :level

  # speedは1フレームに進む角度(360度系)、levelは振幅の大きさ(1.0でImageサイズ)
  def initialize(speed=5, level=0.1)
    super(@@core, "Raster")
    self.g_start = 0.0
    self.g_level = 0.0
    @speed = speed
    @level = level

    # シェーダパラメータ設定のメソッドは外部非公開とする
    class << self
      protected :g_start, :g_start=, :g_level, :g_level=
    end
  end

  def update
    self.g_start += @speed
    self.g_level = @level
  end
end

