# LED得点掲示板 2023ver.

# 準備するもの
- 電源装置(40V)
- 制御基板 x4
- ACアダプタ(5V) x4
- LEDパネル x12
- 電源ケーブル x4
- Wi-Fiアクセスポイント(携帯のテザリングでも可) x1
- 操作用端末(Chromeブラウザが動けばOK) x1

# 準備
- 電源の電圧が39V~40Vになるように調節してください。

# 組み立て
1. 

# 起動

# 操作

## 送信

![a](/Image/ScoreBoardApp3.png)

各操作を行ったら送信ボタンを押してデータをLEDパネルへ送信してください。4つのブロックの状態を一斉に変更します。

## 得点を変える

![a](/Image/ScoreBoardApp1.png)

各桁の上下にある「＋」、「－」ボタンで数字を操作して目的の得点に変更出来たら最後にページ上部の「送信」ボタンを押してください。また、複数のブロックの得点を同時に変更する場合は各ブロックの得点を操作し終わってから最後にまとめて「送信」を押してください。

## 明るさを変える

![a](/Image/ScoreBoardApp2.png)

スライドバー（8段階）を一番左で消灯、一番右で明るさ最大です。






# メモ
## ArduinoIDEでの書き込みエラー
```
Connecting....

A serial exception error occurred: Write timeout
Note: This error originates from pySerial. It is likely not a problem with esptool, but with the hardware connection or drivers.
For troubleshooting steps visit: https://docs.espressif.com/projects/esptool/en/latest/troubleshooting.html
Failed uploading: uploading error: exit status 1

```
対処法...COMポートを変える。（たぶん若い番号のほうがいい？)

## 通信形式、データフォーマット
ブラウザ

| Websocket

ブラウザ(中継)

| WebSerialAPI [b1_ScoreUpperByte,b1_ScorelowerByte,b1_lv]

Twelite(親機)

| 無線 [address,Score,lv]

TweLite(子機）x4


[def]: //Image/ScoreBoardApp.png