require 'dxruby'

hlsl = <<EOS
  texture tex0;

  sampler Samp0 = sampler_state
  {
   Texture =<tex0>;
   AddressU = WRAP;
   AddressV = WRAP;
  };

  float4 PS(float2 input : TEXCOORD0) : COLOR0
  {
    return tex2D( Samp0, float2((input.x-0.5) / (input.y+0.20), (input.y-1) / (input.y+0.20)) );
  }

  technique SH
  {
   pass P0
   {
    PixelShader = compile ps_2_0 PS();
   }
  }
EOS

core = Shader::Core.new(hlsl,{})
shader = Shader.new(core, "SH")

image = Image.new(80, 80,[0, 255, 0])
image.box_fill(0, 0, 39, 39, [150,250,150])
image.box_fill(40, 0, 79, 39, [100,250,100])
image.box_fill(0, 40, 39, 79, [200,250,200])
image.box_fill(40, 40, 79, 79, [0,220,0])

rt = RenderTarget.new(640,240)
z = 0
x = 0
y = 0
Window.loop do
  z -= 10
  x += Input.x * 5
  y -= Input.y * 2
  y = 0 if y < 0
  y = 100 if y > 100

  rt.draw_tile(0, 0, [[0]], [image], x, z, 8, 3).update
  Window.draw_ex(0, 240, rt, :shader=>shader, :scaley=>y/200.0+0.5, :centery=>240.0)
  break if Input.key_push?(K_ESCAPE)
end
