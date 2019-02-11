require 'dxruby'

class AnimeSprite < Sprite

  def initialize(x=0, y=0, image=nil)
    super
    @anime_sprite_count = 0
    @anime_sprite_frame_count = 1
    @anime_sprite_pause = true
    @anime_sprite_animation_pattern = nil
    @anime_sprite_hash = {}
    @anime_sprite_next = nil
  end

  def start_animation(v, animation_pattern=nil, nxt=nil)
    temp = @anime_sprite_hash[v]
    if temp
      @anime_sprite_frame_count = temp[0]
      @anime_sprite_animation_pattern = temp[1]
      @anime_sprite_next = temp[2]
    else
      @anime_sprite_frame_count = v
      @anime_sprite_animation_pattern = animation_pattern
      @anime_sprite_next = nxt
    end
    @anime_sprite_pause = false
    @anime_sprite_count = 0 
  end

  def change_animation(v, animation_pattern=nil, nxt=nil)
    temp = @anime_sprite_hash[v]
    if temp
      @anime_sprite_frame_count = temp[0]
      @anime_sprite_animation_pattern = temp[1]
      @anime_sprite_next = temp[2]
    else
      @anime_sprite_frame_count = v
      @anime_sprite_animation_pattern = animation_pattern
      @anime_sprite_next = nxt
    end
  end

  def pause_animation
    @anime_sprite_pause = true
  end

  def resume_animation
    @anime_sprite_pause = false
  end

  def add_animation(v, frame_count, animation_pattern=nil, nxt=nil)
    @anime_sprite_hash[v] = [frame_count, animation_pattern, nxt]
  end

  def update
    if @anime_sprite_animation_pattern
      unless @anime_sprite_pause
        @anime_sprite_count += 1
        if @anime_sprite_count >= @anime_sprite_frame_count * @anime_sprite_animation_pattern.size
          if @anime_sprite_next
            temp = @anime_sprite_hash[@anime_sprite_next]
            if temp
              @anime_sprite_frame_count = temp[0]
              @anime_sprite_animation_pattern = temp[1]
              @anime_sprite_next = temp[2]
            else
              self.send @anime_sprite_next
            end
          end
          @anime_sprite_count = 0 
        end
      end
      self.image = @anime_sprite_animation_image[@anime_sprite_animation_pattern[@anime_sprite_count / @anime_sprite_frame_count]]
    else
      unless @anime_sprite_pause
        @anime_sprite_count += 1
        if @anime_sprite_count >= @anime_sprite_frame_count * @anime_sprite_animation_image.size
          if @anime_sprite_next
            temp = @anime_sprite_hash[@anime_sprite_next]
            if temp
              @anime_sprite_frame_count = temp[0]
              @anime_sprite_animation_pattern = temp[1]
              @anime_sprite_next = temp[2]
            else
              self.send @anime_sprite_next
            end
          end
          @anime_sprite_count = 0
        end
      end
      self.image = @anime_sprite_animation_image[@anime_sprite_count / @anime_sprite_frame_count]
    end
    super
  end

  def animation_image
    @anime_sprite_animation_image
  end

  def animation_image=(v)
    @anime_sprite_animation_image = v
    self.image = v[0]
  end
end


image = Image.load_tiles("./image/enemy1.png", 4, 1)

s = AnimeSprite.new
s.x = 0
s.y = 0
s.animation_image = image
s.add_animation(:anime1, 10,[0,1,2,3], :anime1)
s.add_animation(:anime2, 4,[0,1,0,1,0,1,0,1,0,1], :anime1)
s.start_animation(:anime1)

Window.loop do
  if Input.key_push?(K_Z)
    s.pause_animation
  end
  if Input.key_push?(K_X)
    s.resume_animation
  end
  if Input.key_push?(K_C)
    s.start_animation(:anime2)
  end
  s.update
  s.draw
  break if Input.key_push?(K_ESCAPE)
end



