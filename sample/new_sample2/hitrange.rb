require 'dxruby'

module HitRange
  def draw
    super
    if self.visible and self.collision_enable and !self.vanished?
      if @hitrange_image == nil or @hitrange_collision != self.collision or
         @hitrange_image_width != self.image.width or 
         @hitrange_image_height != self.image.height

        @hitrange_image_width = self.image.width
        @hitrange_image_height = self.image.height

        @hitrange_image.dispose if @hitrange_image != nil

        color = [255, 255, 255, 0]
        color_fill = [100, 255, 255, 0]

        if self.collision == nil
          @hitrange_collision = nil
          @hitrange_image = Image.new(self.image.width, self.image.height)
          @hitrange_image.box_fill(0, 0, @hitrange_image.width-1, @hitrange_image.height-1, color_fill)
          @hitrange_image.box(0, 0, @hitrange_image.width-1, @hitrange_image.height-1, color)
          @hitrange_x = @hitrange_y = 0
        else
          @hitrange_collision = []
          self.collision.each do |v|
            if v === Array
              @hitrange_collision << v.dup
            else
              @hitrange_collision << v
            end
          end
          temp = self.collision
          temp = [temp] if temp[0].class != Array

          x1 = y1 = 0
          x2 = y2 = 0
          temp.each do |col|
            case col.size
            when 2 # point
              x1 = [x1, col[0]].min
              y1 = [y1, col[1]].min
              x2 = [x2, col[0]].max
              y2 = [y2, col[1]].max
            when 3 # circle
              x1 = [x1, col[0] - col[2]].min
              y1 = [y1, col[1] - col[2]].min
              x2 = [x2, col[0] + col[2]].max
              y2 = [y2, col[1] + col[2]].max
            when 4 # box
              x1 = [x1, col[0], col[2]].min
              y1 = [y1, col[1], col[3]].min
              x2 = [x2, col[0], col[2]].max
              y2 = [y2, col[1], col[3]].max
            when 6 # triangle
              x1 = [x1, col[0], col[2], col[4]].min
              y1 = [y1, col[1], col[3], col[5]].min
              x2 = [x2, col[0], col[2], col[4]].max
              y2 = [y2, col[1], col[3], col[5]].max
            end
          end

          @hitrange_image = Image.new(x2 - x1 + 1, y2 - y1 + 1)
          @hitrange_x = x = x1
          @hitrange_y = y = y1

          temp.each do |col|
            case col.size
            when 2 # point
              @hitrange_image[col[0] - x, col[1] - y] = color
            when 3 # circle
              @hitrange_image.circle_fill(col[0] - x, col[1] - y, col[2], color_fill)
              @hitrange_image.circle(col[0] - x, col[1] - y, col[2], color)
            when 4 # box
              @hitrange_image.box_fill(col[0] - x, col[1] - y, col[2] - x, col[3] - y, color_fill)
              @hitrange_image.box(col[0] - x, col[1] - y, col[2] - x, col[3] - y, color)
            when 6 # triangle
              @hitrange_image.triangle_fill(col[0] - x, col[1] - y, col[2] - x, col[3] - y, col[4] - x, col[5] - y, color_fill)
              @hitrange_image.triangle(col[0] - x, col[1] - y, col[2] - x, col[3] - y, col[4] - x, col[5] - y, color)
            end
          end

        end
      end

      hash = {}
      if self.collision_sync
        hash[:scale_x] = self.scale_x
        hash[:scale_y] = self.scale_y
        hash[:center_x] = self.center_x - @hitrange_x
        hash[:center_y] = self.center_y - @hitrange_y
        hash[:angle] = self.angle
      end
      hash[:z] = self.z + 1

      ox = oy = 0
      if self.offset_sync
        ox = self.center_x
        oy = self.center_y
      end

      target = self.target ? self.target : Window

      target.draw_ex(self.x - ox + @hitrange_x, self.y - oy + @hitrange_y, @hitrange_image, hash)
    end
  end
end



if __FILE__ == $0
  class TestSprite < Sprite
    include HitRange
  end

  s = TestSprite.new(100,100)
  s.image = Image.new(100,100,C_BLUE)
  s.offset_sync=true
  s.center_x = 0
  s.center_y = 0

  y = 90
  Window.loop do
    y += Input.y
    s.collision = [10,10,90,0,50,y]
    s.angle += 1
    s.draw
  end
end



