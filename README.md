# DXRuby

使う人は

    gem install dxruby

でどうぞ。
Ruby2.3～2.6までの32bit用バイナリが入っており、Rubyのバージョンにより自動的に選択されます。RubyInstaller2なら後述する方法で自前ビルドできます。

リファレンスマニュアルは
http://mirichi.github.io/dxruby-doc/index.html
にあります。

## ビルドする方法

1. RubyInstaller2をコンパイラ込みでインストールします。
2. このリポジトリをクローンして、コマンドプロンプトを起動してソースがあるところに移動します。
3. ridk enableと入力してEnterすると、コンパイラが使えるようになります。
4. ruby extconf.rbと入力してEnterすると、Makefileが作成されます。
5. makeと入力してEnterすると、ビルドできます。dxruby.soファイルが作成されたはずです。
6. とりあえずmake installと入力してEnterするとインストールができますが、gemじゃないのでアンインストールは自分でファイルを削除する必要があります。
