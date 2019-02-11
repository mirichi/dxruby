require 'dxruby'

hlsl = <<EOS
  texture tex0;
  float4 color;
  float start_angle, end_angle, flag, length;

  sampler Samp0 = sampler_state
  {
   Texture =<tex0>;
  };

  float4 PS(float2 input : TEXCOORD0) : COLOR0
  {
    float temp;
    temp = degrees(atan2(0.5 - input.y ,0.5 - input.x)) + 180.0;
    clip(float2((temp - start_angle) * (end_angle - temp) * flag, length - distance(float2(0.5, 0.5), input)));

    return color;
  }

  technique
  {
   pass
   {
    PixelShader = compile ps_2_0 PS();
   }
  }
EOS

core = Shader::Core.new(hlsl, :color => :float, :start_angle => :float, :end_angle => :float, :flag => :float, :length => :float)
shader = Shader.new(core)

image = Image.new(300,300)

shader.color = [255, 255, 255, 255]
shader.start_angle = 0
shader.end_angle = 0
shader.flag = 1
shader.length = 0.4

x = 1

Window.loop do
  x += 5
  x = 0 if x > 360
  shader.start_angle = x
  Window.draw_ex(100, 100, image, shader:shader)
end




