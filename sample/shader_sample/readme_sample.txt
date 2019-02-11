DXRuby シェーダサンプルプログラム

まず最初に、ソースが汚くてごめんなさい。
テキトーに作ったものたちを参考になればって感じで詰め込んであります。

sample_blur.rb
    ・・・ブラー(ぼかし)処理です。

sample_divide.rb
    ・・・マウスで線を引くと画像が真っ二つになります。

sample_flash.rb
    ・・・Zキーで画像がフラッシュします。

sample_lens.rb
    ・・・マウスカーソルの位置の絵が虫眼鏡みたいに拡大されます。

sample_mapping.rb
    ・・・中心から同心円状に縦ラスタスクロールさせて、光と影を計算しています。
          バンプマッピングモドキです。

sample_rasterscroll.rb
    ・・・シンプルなラスタスクロールです。

sample_rgsssprite.rb
    ・・・RGSS2のSpriteクラスと近いエフェクト(flash、tone、color)を実現するシェーダです。

sample_spehari.rb
    ・・・SEGAのスペースハリアーの地面のようなものを描画します。

sample_sphere.rb
    ・・・球体の表面に画像をマッピングするようなエフェクトです。

sample_transition.rb
    ・・・ルール画像に基づいたトランジションを行います。RGSSに近いパラメータになっています。

sample_wingman.rb
    ・・・ウィングマンぽいエフェクトです。古くてすみません。

turn_transition.rb
    ・・・これはシェーダではないのですが、なぜかここに入っています。
          Window.draw_morphを使って、分割した画像を3D回転のように描画します。

