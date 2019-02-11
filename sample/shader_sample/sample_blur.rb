require 'dxruby'

hlsl = <<EOS
  texture tex0;
  float2 g_size;

  sampler Samp0 = sampler_state
  {
    Texture =<tex0>;
    AddressU=BORDER;
    AddressV=BORDER;
  };

  float4 PS1(float2 In : TEXCOORD0) : COLOR0
  {
    float2 Texel0 = In + float2( -1.0/g_size.x, 0.0f );
    float2 Texel1 = In + float2( -1.0/g_size.x*2, 0.0f );
    float2 Texel2 = In + float2( -1.0/g_size.x*3, 0.0f );
    float2 Texel3 = In + float2( -1.0/g_size.x*4, 0.0f );
    float2 Texel4 = In + float2( -1.0/g_size.x*5, 0.0f );
      
    float2 Texel5 = In + float2( 1.0/g_size.x, 0.0f );
    float2 Texel6 = In + float2( 1.0/g_size.x*2, 0.0f );
    float2 Texel7 = In + float2( 1.0/g_size.x*3, 0.0f );
    float2 Texel8 = In + float2( 1.0/g_size.x*4, 0.0f );
    float2 Texel9 = In + float2( 1.0/g_size.x*5, 0.0f );

    float4 p0  = tex2D( Samp0, In ) * 0.20f;
      
    float4 p1  = tex2D( Samp0, Texel0 ) * 0.12f;
    float4 p2  = tex2D( Samp0, Texel1 ) * 0.10f;
    float4 p3  = tex2D( Samp0, Texel2 ) * 0.08f;
    float4 p4  = tex2D( Samp0, Texel3 ) * 0.06f;      
    float4 p5  = tex2D( Samp0, Texel4 ) * 0.04f;
   
    float4 p6  = tex2D( Samp0, Texel5 ) * 0.12f;
    float4 p7  = tex2D( Samp0, Texel6 ) * 0.10f;
    float4 p8  = tex2D( Samp0, Texel7 ) * 0.08f;
    float4 p9  = tex2D( Samp0, Texel8 ) * 0.06f;
    float4 p10 = tex2D( Samp0, Texel9 ) * 0.04f;

    return p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
  }

  float4 PS2(float2 In : TEXCOORD0) : COLOR0
  {
    float2 Texel0 = In + float2( 0.0f, -1.0/g_size.y );
    float2 Texel1 = In + float2( 0.0f, -1.0/g_size.y*2 );
    float2 Texel2 = In + float2( 0.0f, -1.0/g_size.y*3 );
    float2 Texel3 = In + float2( 0.0f, -1.0/g_size.y*4 );
    float2 Texel4 = In + float2( 0.0f, -1.0/g_size.y*5 );
        
    float2 Texel5 = In + float2( 0.0f, 1.0/g_size.y );
    float2 Texel6 = In + float2( 0.0f, 1.0/g_size.y*2 );
    float2 Texel7 = In + float2( 0.0f, 1.0/g_size.y*3 );
    float2 Texel8 = In + float2( 0.0f, 1.0/g_size.y*4 );
    float2 Texel9 = In + float2( 0.0f, 1.0/g_size.y*5 );
  
    float4 p0  = tex2D( Samp0, In ) * 0.20f;
        
    float4 p1  = tex2D( Samp0, Texel0 ) * 0.12f;
    float4 p2  = tex2D( Samp0, Texel1 ) * 0.10f;
    float4 p3  = tex2D( Samp0, Texel2 ) * 0.08f;
    float4 p4  = tex2D( Samp0, Texel3 ) * 0.06f;      
    float4 p5  = tex2D( Samp0, Texel4 ) * 0.04f;
     
    float4 p6  = tex2D( Samp0, Texel5 ) * 0.12f;
    float4 p7  = tex2D( Samp0, Texel6 ) * 0.10f;
    float4 p8  = tex2D( Samp0, Texel7 ) * 0.08f;
    float4 p9  = tex2D( Samp0, Texel8 ) * 0.06f;
    float4 p10 = tex2D( Samp0, Texel9 ) * 0.04f;
  
    return p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9 + p10;
  }

  technique Blur1
  {
    pass P0
    {
      PixelShader  = compile ps_2_0 PS1();
    }
  }
  technique Blur2
  {
    pass P0
    {
      PixelShader  = compile ps_2_0 PS2();
    }   
  }
EOS

core = Shader::Core.new(hlsl,{:g_size => :float})
shader1 = Shader.new(core, "Blur1")
shader2 = Shader.new(core, "Blur2")

image = Image.load("./image/maptile.png")

shader1.g_size = [image.width,image.height]
shader2.g_size = [image.width,image.height]

rt1 = RenderTarget.new(image.width,image.height)
rt2 = RenderTarget.new(image.width,image.height)

Window.loop do
  Window.draw(50, 50, image)
  rt1.draw_ex(0, 0, image, :blend=>:nond, :shader=>shader1)
  rt1.update
  rt2.draw_ex(0, 0, rt1, :blend=>:nond, :shader=>shader2)
  rt2.update
  Window.draw(350, 50, rt2)
  break if Input.key_push?(K_ESCAPE)
end
