require 'dxruby'

hlsl = <<EOS
texture tex0;
texture tex1;
float2 scale;
float2 pos;
float r, distance;

sampler Samp0 = sampler_state
{
 Texture =<tex0>;
};
sampler Samp1 = sampler_state
{
 Texture =<tex1>;
 AddressU = BORDER;
 AddressV = BORDER;
};

float4 PS(float2 input : TEXCOORD0) : COLOR0
{
  float4 output;
  float d;

  clip(tex2D( Samp0, input ).r - 1.0);

  d = sqrt((input.x-0.5)*(input.x-0.5) + (input.y-0.5)*(input.y-0.5))+1;
  output = tex2D( Samp1, float2( pos.x + (input.x-0.5) / scale.x * distance / r * d, pos.y + (input.y-0.5) / scale.y * distance / r * d));
  return output;
}

technique Lens
{
 pass P0
 {
  PixelShader = compile ps_2_0 PS();
 }
}
EOS

Window.width, Window.height = 800, 600
bgimage = Image.load("bgimage/BG42a.jpg")
image = Image.new(200,200).circle_fill(100,100,100,C_WHITE)
loupeimage = Image.new(200,200).circle(100,100,100,C_WHITE)

core = DXRuby::Shader::Core.new(hlsl, {:scale=>:float, :r=>:float, :distance=>:float, :tex1=>:texture, :pos=>:float})
shader = Shader.new(core, "Lens")
shader.scale = [bgimage.width.quo(image.width), bgimage.height.quo(image.height)]
shader.r = 1000
shader.distance = 500
shader.tex1 = bgimage

Window.loop do
  shader.pos = [Input.mouse_pos_x.quo(bgimage.width), Input.mouse_pos_y.quo(bgimage.height)]
  shader.distance += Input.y*10
  shader.distance = 0 if shader.distance < 0
  shader.distance = shader.r if shader.distance > shader.r

  Window.draw(0,0,bgimage)
  Window.draw_shader(Input.mouse_pos_x-image.width/2, Input.mouse_pos_y-image.width/2, image, shader)
  Window.draw(Input.mouse_pos_x-image.width/2, Input.mouse_pos_y-image.width/2, loupeimage)
  break if Input.key_push?(K_ESCAPE)
end

