class SpriteShader < Shader
  hlsl = <<EOS
  float4 g_blend;
  float4 g_tone;

  texture tex0;
  sampler Samp = sampler_state
  {
   Texture =<tex0>;
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
    float3 temp;
    float3 ntsc = {0.298912, 0.586611, 0.114478};
    float ntscy;
    output.Color = tex2D( Samp, input.UV );

    ntsc = ntsc * output.Color.rgb;
    ntscy = ntsc.r + ntsc.g + ntsc.b;
    temp.r = output.Color.r + ((ntscy - output.Color.r) * g_tone.a);
    temp.g = output.Color.g + ((ntscy - output.Color.g) * g_tone.a);
    temp.b = output.Color.b + ((ntscy - output.Color.b) * g_tone.a);

    output.Color.rgb = min(1.0, temp + g_tone.rgb) * (1.0 - g_blend.a) + g_blend.rgb * g_blend.a;

    return output;
  }

  technique TShader
  {
   pass P0
   {
    PixelShader = compile ps_2_0 PS();
   }
  }
EOS

  @@core = Shader::Core.new(hlsl, 
    {
      :g_blend => :float,
      :g_tone => :float,
    }
  )

  def initialize
    super(@@core, "TShader")
    self.g_blend = [0.0,0.0,0.0,0.0]
    self.g_tone = [0.0,0.0,0.0,0.0]
    @tone = [0.0,0.0,0.0]
    @gray = 0.0
    class << self
      protected :g_blend, :g_blend=, :g_tone, :g_tone=
    end
  end

  def tone=(ary)
    self.g_tone = [ary[0] / 255.0, ary[1] / 255.0, ary[2] / 255.0, @gray / 255.0]
    @tone = ary
  end

  def gray=(gray)
    self.g_tone = [@tone[0] / 255.0, @tone[1] / 255.0, @tone[2] / 255.0, gray / 255.0]
    @gray = gray
  end

  def color=(ary)
    self.g_blend = [ary[1] / 255.0, ary[2] / 255.0, ary[3] / 255.0, ary[0] / 255.0]
  end
end

class RgssSprite < Sprite
  def initialize(x=0, y=0, image=nil)
    super
    self.shader = SpriteShader.new
    @rgss_sprite_color = nil
    @rgss_sprite_flash_color = nil
    @rgss_sprite_flash_duration = 1
    @rgss_sprite_flash_count = 0
  end

  def tone=(ary)
    if ary == nil
      self.shader.tone = [0,0,0]
    else
      self.shader.tone = ary
    end
  end

  def gray=(gray)
    self.shader.gray = gray
  end

  def color=(ary)
    @rgss_sprite_color = ary
  end

  def flash(color=[255,255,255,255], duration=30)
    @rgss_sprite_flash_color = color
    @rgss_sprite_flash_duration = duration
    @rgss_sprite_flash_count = duration
  end

  def update
    @rgss_sprite_flash_count -= 1 if @rgss_sprite_flash_count > 0
  end

  def draw
    if @rgss_sprite_color != nil and @rgss_sprite_flash_count > 0
      if @rgss_sprite_color[0] > @rgss_sprite_flash_color[0] * @rgss_sprite_flash_count / @rgss_sprite_flash_duration
        self.shader.color = @rgss_sprite_color
      else
        self.shader.color = [@rgss_sprite_flash_color[0] * @rgss_sprite_flash_count / @rgss_sprite_flash_duration,
                             @rgss_sprite_flash_color[1],
                             @rgss_sprite_flash_color[2],
                             @rgss_sprite_flash_color[3]
                            ]
      end
    elsif @rgss_sprite_flash_count > 0
      self.shader.color = [@rgss_sprite_flash_color[0] * @rgss_sprite_flash_count / @rgss_sprite_flash_duration,
                           @rgss_sprite_flash_color[1],
                           @rgss_sprite_flash_color[2],
                           @rgss_sprite_flash_color[3]
                          ]
    elsif @rgss_sprite_color != nil
      self.shader.color = @rgss_sprite_color
    else
      self.shader.color = [0,0,0,0]
    end
    super
  end
end

