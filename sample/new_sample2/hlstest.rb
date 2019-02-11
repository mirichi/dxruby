require 'dxruby'

image = Image.load("./shader_sample/bgimage/BG10a_80.jpg")
Window.width, Window.height = 800, 600
Window.mag_filter = TEXF_POINT

hue = 0
luminance = 0
saturation = 0

Window.loop do
  hue += Input.y*5
  saturation += Input.x
  if Input.key_down?(K_Z)
    luminance -= 1
  end
  if Input.key_down?(K_X)
    luminance += 1
  end

  img = image.change_hls(hue, luminance, saturation)
  img.delayed_dispose
  Window.draw(0,0,img)
end



