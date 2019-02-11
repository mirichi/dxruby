require 'dxruby'

hlsl = <<EOS
float g_start;
float g_level;
texture tex0;
sampler Samp = sampler_state
{
 Texture =<tex0>;
 AddressU = BORDER;
 AddressV = BORDER;
};

float4 PS(float2 input : TEXCOORD0) : COLOR0
{
  float4 output;
  float dist = radians(distance(input, float2(0.5, 0.5)) * 360 * 4 - g_start);
  float height = sin(dist);
  float slope = cos(dist);
  float d = clamp(-1,1,dot(normalize(float3(input.y - 0.5, input.x - 0.5,0 )), float3(0.5,-0.5,0.5)))*slope+1;
  input.y = input.y + height * g_level;

  output = tex2D( Samp, input ) * d;

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

Window.width, Window.height = 800, 600
core = DXRuby::Shader::Core.new(hlsl, {:g_start=>:float, :g_level=>:float})
shader = Shader.new(core, "Raster")
shader.g_start = 0
shader.g_level = 0
image = Image.load("bgimage/BG42a.jpg")

Window.loop do
  shader.g_start += 3
  shader.g_level = 0.15
  Window.draw_shader(0, 0, image, shader)
  break if Input.key_push?(K_ESCAPE)
end

