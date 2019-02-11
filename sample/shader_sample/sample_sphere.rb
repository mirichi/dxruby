require 'dxruby'

hlsl = <<EOS
texture tex0;
float2 raito;

sampler Samp0 = sampler_state
{
 Texture =<tex0>;
 AddressU = BORDER;
 AddressV = BORDER;
};

float4 PS1(float2 input : TEXCOORD0) : COLOR0
{
  float pi = acos(-1);
  return tex2D( Samp0, 
                    float2( ((1 - (acos((input.x*2 - 1.0) ) / pi)) - 0.5) + 0.5
                          , ((1 - (acos((input.y*2 - 1.0) ) / pi)) - 0.5) + 0.5) );
//                          , input.y ));
}

float4 PS2(float2 input : TEXCOORD0) : COLOR0
{
  return tex2D( Samp0, 
                  float2((input.x - 0.5) / cos(asin((input.y*2 - 1.0))) / raito.x + 0.5
                       , (input.y - 0.5) / raito.y + 0.5) );
}

technique Sphere1
{
 pass P0
 {
  PixelShader = compile ps_2_0 PS1();
 }
}
technique Sphere2
{
 pass P0
 {
  PixelShader = compile ps_2_0 PS2();
 }
}
EOS

Window.width, Window.height = 800, 600
image = Image.load("bgimage/world_map2.png")
image = image.slice(4, 5, image.width - 10, image.height - 9)

core = DXRuby::Shader::Core.new(hlsl, {:raito=>:float})
shader1 = Shader.new(core, "Sphere1")
shader2 = Shader.new(core, "Sphere2")
shader1.raito = shader2.raito = [600.quo(Window.width), 600.quo(Window.height)]

rt1 = RenderTarget.new(image.width/2, image.height)
rt2 = RenderTarget.new(Window.width, Window.height)
x = 0

Window.loop do
  x -= 3
  rt1.draw_tile(0, 0, [[0]], [image], x, 0, 2, 1).update
  rt2.draw_ex(0, 0, rt1, :shader=>shader1, :centerx=>0, :centery=>0, :scalex=>Window.width.quo(rt1.width), :scaley=>Window.height.quo(rt1.height) ).update
  Window.draw_ex(0, 0, rt2, :shader=>shader2,:angle=>23.4)
  break if Input.key_push?(K_ESCAPE)
end

