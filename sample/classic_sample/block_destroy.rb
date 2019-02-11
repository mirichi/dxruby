#!ruby -Ks
# DXRuby �T���v�� �u���b�N�������h�L
# ���N���b�N�Ń{�[������т܂��B��̓}�E�X�őł��Ԃ��āB
require 'dxruby'

Window.caption = "�u���b�N�������h�L"

# �u���b�N�̍\����
Block = Struct.new(:x, :y, :image)

# ���f�[�^�쐬
v = 80  # �{�����[��
s1 = SoundEffect.new(50) do # 50mSec�̌��ʉ��B�u���b�N����1ms�̕ω�
  v = v - 4 if v > 0        # �{�����[���_�E��
  [220, v]                  # �u���b�N�̖߂�̔z�񂪎��g���ƃ{�����[��
end
v = 80
s2 = SoundEffect.new(50) do
  v = v - 4 if v > 0
  [440, v]
end
v = 80
s3 = SoundEffect.new(50) do
  v = v - 4 if v > 0
  [880, v]
end

# �摜�f�[�^�쐬�B
# �p�b�h�̉摜
padimage = Image.new(100, 20)
padimage.boxFill(0, 0, 99, 19, [255, 255, 255])

# �u���b�N�̉摜�B��������Y��ɂ��悤�Ǝv�����炸���Ԃ񑝂��������
blockimage = [Image.new(60, 20, [255, 0, 0]),
              Image.new(60, 20, [255, 255, 0]),
              Image.new(60, 20, [0, 255, 0])]
blockimage[0].line(0, 0, 59, 0, [255, 150, 150])
blockimage[0].line(0, 0, 0, 19, [255, 150, 150])
blockimage[0].line(0, 19, 59, 19, [120, 0, 0])
blockimage[0].line(59, 0, 59, 19, [120, 0, 0])
blockimage[1].line(0, 0, 59, 0, [255, 255, 150])
blockimage[1].line(0, 0, 0, 19, [255, 255, 150])
blockimage[1].line(0, 19, 59, 19, [150, 150, 0])
blockimage[1].line(59, 0, 59, 19, [150, 150, 0])
blockimage[2].line(0, 0, 59, 0, [150, 255, 120])
blockimage[2].line(0, 0, 0, 19, [150, 255, 120])
blockimage[2].line(0, 19, 59, 19, [0, 150, 0])
blockimage[2].line(59, 0, 59, 19, [0, 150, 0])

# �e�̉摜
ballimage = Image.new(24, 24)
ballimage.circleFill(12, 12, 11, [255, 255, 255])

# �ʂ̏��
tx = 0
ty = 416
dtx = 0
dty = 0

# 0�͏������A1�͋ʂ����łȂ��B2�͔��ł�B
state = 0

block = []

# ���C�����[�v
Window.loop do
  x = Input.mousePosX - 50
  x = 0 if x < 0
  x = 540 if x > 540
  y = Input.mousePosY - 10
  y = 220 if y < 220
  y = 460 if y > 460

  case state
  when 0
    # �u���b�N�z�񏉊���
    block.clear
    # �u���b�N����
    for i in 0..9
      for j in 0..5
        block.push(Block.new(i * 64 + 2, j * 24 + 50, blockimage[j / 2]))
      end
    end
    state = 1

  when 1
    # ���łȂ��Ƃ��̓p�b�h�ɂ������Ă���
    if Input.mousePush?(M_LBUTTON) then # �}�E�X�̍��N���b�N�Ŕ�΂�
      state = 2
      dtx = rand(2) * 16 - 8
      dty = -8
    else   # �������Ă�Ƃ�
      tx = x + 38
      ty = y - 24
    end

  when 2
    # �Q�[�����C������
    # �u���b�N�Ƃ̔���
    block.delete_if do |b|
      flag = false
      # ���Ƀu���b�N���������牡�ɂ͂˂�����
      if tx + dtx + 24 > b.x and tx + dtx < b.x + 60 and
         ty + 24 > b.y and ty < b.y + 20 then
        dtx = -dtx
        s3.play
        # �i�i���ɂ������ꍇ�͏c�ɂ��͂˂�����
        if tx + 24 > b.x and tx < b.x + 60 and
           ty + dty + 24 > b.y and ty + dty < b.y + 20 then
          dty = -dty
        end
        flag = true

      # �c�Ƀu���b�N����������c�ɂ͂˂�����
      elsif tx + 24 > b.x and tx < b.x + 60 and
         ty + dty + 24 > b.y and ty + dty < b.y + 20 then
        dty = -dty
        s3.play
        flag = true
      end
      flag
    end
    # �ړ�
    tx = tx + dtx
    ty = ty + dty
    # �p�b�h�Ƃ̔���
    if tx + 24 > x and tx < x + 100 and ty + 24 > y and ty < y + 20 and dty > 0 then
      dty = -8
      if tx < x + 38 - dtx * 2 then
        dtx = -8
      else
        dtx = 8
      end
      s1.play
    end
    # ��ʊO�̔���
    if tx > 607 then
      dtx = -8
      s2.play
    end
    if tx < 8 then
      dtx = 8
      s2.play
    end
    if ty <= 0 then
      dty = 8
      s2.play
    end
    state = 1 if ty > 479        # �{�[����������
    state = 0 if block.size == 0 # �u���b�N���S��������
  end

  # �摜�`��
  Window.draw(x, y, padimage)
  Window.draw(tx, ty, ballimage)
  block.each do |b|
    Window.draw(b.x, b.y, b.image)
  end

  break if Input.keyDown?(K_ESCAPE)
end