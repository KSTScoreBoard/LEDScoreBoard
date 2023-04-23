# LED得点掲示板 2023ver.
編集中

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
