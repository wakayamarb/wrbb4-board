Wakayama.rb Board(WRBB4) V2 library Firmware
------
sample
　行頭にファームウェアのバージョンが入っているのでバージョンを確認してください。

　実装しているrubyメソッドなどの使い方は、各フォルダ内のdescriptionフォルダ内にあるRuby Firmware on WRBB4.pdfを参照してください。
また、mrubyフォルダ内にあるbuild_config.rbが、[mruby](https://github.com/mruby/mruby)(libmruby.a)をmakeする際に使用したbuild_config.rbです。

　V2ライブラリを使ったWakayama.rb のRubyボードWRBB4用のソースと実行バイナリです。
　V2ライブラリとは、ルネサスさんが提供しているRX631のV2ライブラリを示します。

  バージョンのARIDAやUmeJamの後の数字は動作する基板種類を表します。
  バージョン最後のf2やf3はバイトコードフォーマットの番号です。mruby1.0.0ベースの場合はf2となります。

  mruby ver1.0.0 -> ByteCode Format Ver.0002  
  mruby ver1.1.0 -> ByteCode Format Ver.0003  
  mruby ver1.2.0 -> ByteCode Format Ver.0003  

    WAKAYAMA.RB Board Ver.ARIDA5-1.52(2016/3/19)f3(256KB), mruby 1.2.0
                            |      |      |      |   |
                            |      |      |      |   |
                            |      |      |      |  RAM Size
                            |      |      |      |
                            |      |      |   ByteCode Format Number
                            |      |      |
                            |      |    作成日
                            |      |
                            | Firmware Version
                            |
                       回路基板種類


How to use Wakayama.rb board
------
https://github.com/wakayamarb/wrbb4-board/tree/master/firmware_release/description/Ruby%20Firmware%20on%20GR-CITRUS.pdf

Link
------


License
------
 Wakayama.rb-Board(WRBB4)-V2-library-Firmware is released under the [MIT License](MITL).
