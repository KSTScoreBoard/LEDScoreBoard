＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝ I2C サンプル ===

本サンプルでは、I2C デバイスのアクセスを行います。

■ 使用法
１．UART0 115200 8N1 で接続する
２．以下の入力を行う
  a -> ADT7410 センサー
  b -> BH1715 センサー
  l -> LIS3DH センサー
  m -> MPL115A2 センサー
  s -> SHT21 センサーの温度
  h -> SHT21 センサーの湿度
  e -> 24AA00 の EEPROM の書き込みと読み出し

■ 実行例
# [h] -->
Start SHT21 humidity sensing...
SHT21 --> 6073[% x 100]

Fire PING Broadcast Message.
[PKT Ad:81001f1c,Ln:035,Seq:005,Lq:126,Tms:15120 "PONG: SHT21 --> 6073[% x 100]..C"]
PONG Message from 81001f1c

■ ソースコード
  Samp_PingPong を流用しています。PingPong サンプルと同様にデータ送信と、
  応答メッセージを受信します。

  Main.c
    vHandleSerialInput()
      各キー入力処理が記述されます。

    cbToCoNet_vHwEvent()
      センサーは変換開始のコマンド入力後一定時間経過後にデータ取得を
      行います。センサーの開始後に u32KickedTimeStamp を設定し、
      この値と現在の u32TickCount_ms と比較しタイムアウトすれば
      I2C 処理によりデータ取得します。

  SMBus.c
    I2C バスのペリフェラル API の手続きを簡素化したライブラリコードです。
    このサンプルでは以下の３関数のみを利用します。他は利用しません。

    vSMBusInit()
      初期化関数です。I2C バスの初期化とクロック速度の設定を行っています。

    bSMBusWrite(uint8 u8Address, uint8 u8Command, uint8 u8Length, uint8* pu8Data)
      I2C バスにデータを書き込みます。
      u8Address I2Cアドレス 1010011 なら 0x53 と指定します。
      u8Command 最初の送信データバイトです。
        指定は I2C デバイスのデータシートを参照してください。
        動作コマンドやアドレスの指定などに利用されます。
      u8Lenght  続くデータ数 (0 なら u8Command で終了)
      pu8Data   続くデータへのポインタ (u8Length == 0 なら NULL を指定)

    bSMBusSequentialRead(uint8 u8Address, uint8 u8Length, uint8* pu8Data)
      I2C バスにデータを読み出します。

      u8Address I2Cアドレス 1010011 なら 0x53 と指定します。
      u8Lenght  読み出すデータ数です。
      pu8Data   読み出したデータの格納左記です。

   SHT21.c
     Senserion SHT21 温湿度センサーの手続きです。
     詳細はデータシートを参照してください。

   BH1715.c
     Rohm BH1715 照度センサーの手続きです。
     詳細はデータシートを参照してください。

   24xx01_1025.c
     EEPROM の 24AA00 などのアクセス手続きです。
     詳細はデータシートを参照してください。

   ADT7410.c
     ANALOG DEVICES ADT7410 温度センサーの手続きです。
     詳細はデータシートを参照してください。

   LIS3DH.c
     STMicroelectronics‎ LIS3DH 加速度センサーの手続きです。
     詳細はデータシートを参照してください。

   MPL115A2.c
     FreeScale MPL115A2 気圧センサーの手続きです。
     詳細はデータシートを参照してください。

＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝