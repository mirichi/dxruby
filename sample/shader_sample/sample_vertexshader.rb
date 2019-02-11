require 'dxruby'

class Vector
  def initialize(*v)
    @vec = v
  end

  def rotate(angle)
    x = @vec[0] * Math.cos(Math::PI / 180 * angle) - @vec[1] * Math.sin(Math::PI / 180 * angle)
    y = @vec[0] * Math.sin(Math::PI / 180 * angle) + @vec[1] * Math.cos(Math::PI / 180 * angle)
    temp = @vec.dup
    temp[0] = x
    temp[1] = y
    Vector.new(*temp)
  end

  def +(v)
    case v
    when Vector
      Vector.new(*@vec.map.with_index{|s,i|s+v[i]})
    when Array
      Vector.new(*@vec.map.with_index{|s,i|s+v[i]})
    when Numeric
      Vector.new(*@vec.map{|s|s+v})
    else
      nil
    end
  end

  def *(matrix)
    result = []
    for i in 0..(matrix.size-1)
      data = 0
      for j in 0..(@vec.size-1)
        data += @vec[j] * matrix[j][i]
      end
      result.push(data)
    end
    return Vector.new(*result)
  end

  def [](i)
    @vec[i]
  end

  def size
    @vec.size
  end

  def to_a
    @vec
  end

  def x
    @vec[0]
  end
  def y
    @vec[1]
  end
  def z
    @vec[2]
  end
  def w
    @vec[3]
  end
end

class Matrix
  def initialize(*arr)
    @arr = Array.new(4) {|i| Vector.new(*arr[i])}
  end

  def *(a)
    result = []
    for i in 0..(a.size-1)
      result.push(@arr[i] * a)
    end
    return Matrix.new(*result)
  end

  def [](i)
    @arr[i]
  end

  def size
    @arr.size
  end

  def self.create_rotation_z(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [ cos, sin, 0, 0],
     [-sin, cos, 0, 0],
     [   0,   0, 1, 0],
     [   0,   0, 0, 1]
    )
  end

  def self.create_rotation_x(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [   1,   0,   0, 0],
     [   0, cos, sin, 0],
     [   0,-sin, cos, 0],
     [   0,   0,   0, 1]
    )
  end

  def self.create_rotation_y(angle)
    cos = Math.cos(Math::PI/180 * angle)
    sin = Math.sin(Math::PI/180 * angle)
    return Matrix.new(
     [ cos,   0,-sin, 0],
     [   0,   1,   0, 0],
     [ sin,   0, cos, 0],
     [   0,   0,   0, 1]
    )
  end

  def self.create_transration(x, y, z)
    return Matrix.new(
     [   1,   0,   0,   0],
     [   0,   1,   0,   0],
     [   0,   0,   1,   0],
     [   x,   y,   z,   1]
    )
  end

  def to_a
    @arr.map {|v|v.to_a}.flatten
  end
end


hlsl = <<EOS
float4x4 g_world, g_view, g_proj;
texture tex0;

sampler Samp = sampler_state
{
 Texture =<tex0>;
};

struct VS_OUTPUT
{
  float4 pos : POSITION;
  float2 tex : TEXCOORD0;
};

VS_OUTPUT VS(float4 pos: POSITION, float2 tex: TEXCOORD0)
{
  VS_OUTPUT output;

  output.pos = mul(mul(mul(pos, g_world), g_view), g_proj);
  output.tex = tex;

  return output;
}

float4 PS(float2 input : TEXCOORD0) : COLOR0
{
  return tex2D( Samp, input );
}

technique
{
 pass
 {
  VertexShader = compile vs_2_0 VS();
  PixelShader = compile ps_2_0 PS();
 }
}
EOS

core = Shader::Core.new(hlsl, {:g_world=>:float, :g_view=>:float, :g_proj=>:float})
shader = Shader.new(core)

shader.g_view = Matrix.new([1, 0, 0, 0],
                           [0, -1, 0, 0],
                           [0, 0, 1, 0],
                           [-Window.width/2, Window.height/2, 0, 1]
                          ).to_a

zn = 700.0
zf = 5000.0
sw = 640.0
sh = 480.0
shader.g_proj = Matrix.new([2.0 * zn / sw,              0,                    0, 0],
                           [             0, 2.0 * zn / sh,                    0, 0],
                           [             0,             0,       zf / (zf - zn), 1],
                           [             0,             0, -zn * zf / (zf - zn), 0]
                          ).to_a


image = [Image.load("bgimage/BG42a.jpg"), Image.load("bgimage/BG00a1_80.jpg"),
         Image.load("bgimage/BG10a_80.jpg"), Image.load("bgimage/BG13a_80.jpg"),
         Image.load("bgimage/BG32a.jpg")]

a=0
Window.loop do
  a += 1
  idx = 0
  image.map{|img|
  idx += 1
    [img,
    Matrix.create_transration(0,0,-1000) *
    Matrix.create_rotation_y(a+idx*72) * 
    Matrix.create_transration(0,0,1000) * 
    Matrix.create_rotation_x(20) * 
    Matrix.create_transration(300,300,1500)
    ]
  }.sort_by{|ary|
    -(Vector.new(0,0,0,1) * ary[1]).z
  }.each do |ary|
    shader.g_world = ary[1].to_a
    Window.draw_shader(-ary[0].width/2, -ary[0].height/2, ary[0], shader)
  end
  break if Input.key_push?(K_ESCAPE)
end


