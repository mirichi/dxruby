require 'dxruby'

hlsl = <<EOS
  float3 p1, p2;
  texture tex0;

  sampler Samp0 = sampler_state
  {
   Texture =<tex0>;
  };

  float4 PS1(float2 input : TEXCOORD0) : COLOR0
  {
    clip( cross( float3(p2.x - p1.x, p2.y - p1.y, 0), float3(input.x - p1.x, input.y - p1.y, 0) ).z );
    return tex2D( Samp0, input );
  }

  float4 PS2(float2 input : TEXCOORD0) : COLOR0
  {
    clip( cross( float3(p1.x - p2.x, p1.y - p2.y, 0), float3(input.x - p1.x, input.y - p1.y, 0) ).z );
    return tex2D( Samp0, input );
  }

  technique Zan1
  {
   pass P0
   {
    PixelShader = compile ps_2_0 PS1();
   }
  }
  technique Zan2
  {
   pass P0
   {
    PixelShader = compile ps_2_0 PS2();
   }
  }
EOS

Window.width, Window.height = 800, 600

core = Shader::Core.new(hlsl,{:p1=>:float, :p2=>:float})
shader1 = Shader.new(core, "Zan1")
shader2 = Shader.new(core, "Zan2")
image = Image.load("bgimage/BG42a.jpg")
image1 = image2 = nil

rt = RenderTarget.new(image.width, image.height)

point = nil
transition_flg = false

x1 = y1 = x2 = y2 = 0
angle = 0
alpha = 255

Window.loop do
  if !transition_flg
    if Input.mouse_push?(M_LBUTTON)
      point = [Input.mouse_pos_x, Input.mouse_pos_y]
    end

    if point
      if Input.mouse_down?(M_LBUTTON)
        Window.draw_line(point[0], point[1], Input.mouse_pos_x, Input.mouse_pos_y, C_WHITE, 1)
      else
        transition_flg = true
        shader1.p1 = shader2.p1 = [point[0].quo(image.width), point[1].quo(image.height),0]
        shader1.p2 = shader2.p2 = [Input.mouse_pos_x.quo(image.width), Input.mouse_pos_y.quo(image.height),0]
        angle = Math.atan2( Input.mouse_pos_y - point[1], Input.mouse_pos_x - point[0] )
        image1 = rt.draw_shader(0, 0, image, shader1).update.to_image
        image2 = rt.draw_shader(0, 0, image, shader2).update.to_image
        alpha = 255
        point = nil
        x1 = y1 = x2 = y2 = 0
      end
    end
    Window.draw(0, 0, image)
  else
    x1 -= Math.cos(angle) * 5
    y1 -= Math.sin(angle) * 5
    x2 += Math.cos(angle) * 5
    y2 += Math.sin(angle) * 5
    alpha -= 3
    Window.draw_alpha(x1, y1, image1, alpha)
    Window.draw_alpha(x2, y2, image2, alpha)
    transition_flg = false if alpha < 10
  end
  break if Input.key_push?(K_ESCAPE)
end

