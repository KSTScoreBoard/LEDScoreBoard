＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝ SPI サンプル ===

本サンプルでは、SPI デバイスのアクセスを行います。
また、本サンプルはLIS3DHのみ対応しております。
詳細はセンサのデータシートをご覧ください。

本ファームウェアには親子関係はなく、２台のノードに同じファームウェアを
書き込みます。特別な設定もありません。

◆ UART の設定
  UART0 115200bps 8bin None 1

■ 実行例
# [x] -->
Start LIS3DH sensing...
LIS3DH X Axis --> 2

Fire PING Broadcast Message.
[PKT Ad:8100280a,Ln:031,Seq:001,Lq:168,Tms:16004 "PONG: LIS3DH X Axis --> 2..C"]
PONG Message from 8100280a

# [y] -->
Start LIS3DH sensing...
LIS3DH Y Axis --> 0

Fire PING Broadcast Message.
[PKT Ad:8100280a,Ln:031,Seq:002,Lq:159,Tms:64836 "PONG: LIS3DH Y Axis --> 0..C"]
PONG Message from 8100280a

# [z] -->
Start LIS3DH sensing...
LIS3DH Z Axis --> 131

Fire PING Broadcast Message.
[PKT Ad:8100280a,Ln:033,Seq:003,Lq:168,Tms:13436 "PONG: LIS3DH Z Axis --> 131..C"]
PONG Message from 8100280a

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

   LIS3DH.c
     STMicroelectronics LIS3DH 3軸加速度センサの手続きです。
     詳細はデータシートを参照してください。

＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝