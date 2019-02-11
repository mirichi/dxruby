module HitRange
  PointImage = Image.new(1, 1, [200, 255, 255, 0])
  BoxImage = {}
  CircleImage = {}
  TriangleImage = {}

  def HitRange.view(ary)
    ary.each do |d|
      range = d.getHitRange
      case range.size
      when 2 # point
        Window.draw(range[0], range[1], PointImage, 10000)
      when 3 # circle
        x = range[0]
        y = range[1]
        r = range[2]
        if CircleImage.has_key?(r) == false then
          image = Image.new(r*2+1, r*2+1).circle(r, r, r, [200, 255, 255, 0])
          CircleImage[r] = image
        end
        Window.draw(x - r, y - r, CircleImage[r], 10000)
      when 4 # box
        x1 = range[0]
        y1 = range[1]
        x2 = range[2]
        y2 = range[3]
        key = (x2 - x1).to_s + "," + (y2 - y1).to_s
        if BoxImage.has_key?(key) == false then
          image = Image.new(x2-x1+1, y2-y1+1)
          image.line(0, 0, x2-x1, 0, [200, 255, 255, 0])
          image.line(x2-x1, 0, x2-x1, y2-y1, [200, 255, 255, 0])
          image.line(x2-x1, y2-y1, 0, y2-y1, [200, 255, 255, 0])
          image.line(0, y2-y1, 0, 0, [200, 255, 255, 0])
          BoxImage[key] = image
        end
        Window.draw(x1, y1, BoxImage[key], 10000)
      when 6 # triangle
        x1 = range[0]
        y1 = range[1]
        x2 = range[2]
        y2 = range[3]
        x3 = range[4]
        y3 = range[5]
        key = (x2 - x1).to_s + "," + (y2 - y1).to_s + "," + (x3 - x1).to_s + "," + (y3 - y1).to_s
        temp = (x1 < x2 ? x1 : x2)
        minx = (temp < x3 ? temp : x3)
        temp = (x1 > x2 ? x1 : x2)
        maxx = (temp > x3 ? temp : x3)
        temp = (y1 < y2 ? y1 : y2)
        miny = (temp < y3 ? temp : y3)
        temp = (y1 > y2 ? y1 : y2)
        maxy = (temp > y3 ? temp : y3)

        if TriangleImage.has_key?(key) == false then
          image = Image.new(maxx-minx+1, maxy-miny+1)
          image.line(x1-minx, y1-miny, x2-minx, y2-miny, [200, 255, 255, 0])
          image.line(x2-minx, y2-miny, x3-minx, y3-miny, [200, 255, 255, 0])
          image.line(x3-minx, y3-miny, x1-minx, y1-miny, [200, 255, 255, 0])
          TriangleImage[key] = image
        end
        Window.draw(minx, miny, TriangleImage[key], 10000)
      end
    end
  end
end

