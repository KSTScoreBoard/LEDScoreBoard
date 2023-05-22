# LED得点掲示板 2023ver.

# 準備するもの
- 電源装置(DC 40V / 300W程度) x4
- 制御基板 x4
- 制御基板用フック x4
- ACアダプタ(5V) x4
- LEDパネル x12
- 電源ケーブル x4
- Wi-Fiアクセスポイント(Windowsのモバイルホットスポット/携帯のテザリング　等) x1
- 操作用端末(Chromeブラウザが動けばOK) x1

# 準備
## 電源
長い間電源を使っていなかった場合は、LEDに繋ぐ前に電源の電圧が39~40Vの範囲にあるかをテスターを使って確認してください。  

## Wi-Fiアクセスポイント
Windowsのモバイルホットスポット/携帯のテザリングとうで制御基板が接続するためのアクセスポイントを用意してください。

# 組み立て
1. 各ブロックLEDパネル3枚を窓とアルミの柱の間に入れて固定する
2. 真ん中のパネル付近に制御基板を取り付ける。このとき基板裏のファンがケーブル等と干渉しないように注意する
3. 制御基板とLEDパネルを接続する
4. 電源装置と制御基板を接続する（電源はまだ入れない）

# 起動
1. 制御基板にACアダプタを接続する
2. 制御基板の起動とネットワーク接続を確認する。(auth ok,ready)と表示されていればOK
3. 電源装置とコンセントを接続する。　※コンセントは2系統から分割して電源をとること。
4. 制御アプリで「送信」を押す or 制御基板のボタンを押す　でLEDが点灯することを確認する
5. 得点「888」を表示してすべてのセグメントが点灯していることを確認する

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

## 得点を隠す

![a](/Image/ScoreBoardApp4.png)

得点発表時などに得点を隠すには「非表示」ボタンを使用します。3つのボタンが各桁に対応しており、推すとグレーになります。ボタンがグレーになった状態で「送信」することで非表示が押されてる桁は数字を隠せます。


# 詳細
## Wi-Fiについて
Wi-Fiは使用しているマイコンの関係で2.4Ghz帯を使用する必要があります。校舎に飛んでいるWi-FIは5Ghzのため、Windwosの「モバイルホットスポット」等の機能を使って2.4Ghzネットワークを用意してください。SSIDとパスワードは任意で、変更する場合はマイコンのプログラムも変更してください。

## マイコンの設定情報について
通信に使用するIDとWi-Fi設定はメインのプログラムとは別のファイル **defines.h** に書き込まれています。プログラムを変更する場合は以下にソースコードがあるのでコピーしてください
<details><summary>defines.h</summary>

```
#ifndef defines_h
#define defines_h

#if !( defined(ESP8266) ||  defined(ESP32) )
#error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#elif ( ARDUINO_ESP32S2_DEV || ARDUINO_FEATHERS2 || ARDUINO_ESP32S2_THING_PLUS || ARDUINO_MICROS2 || \
        ARDUINO_METRO_ESP32S2 || ARDUINO_MAGTAG29_ESP32S2 || ARDUINO_FUNHOUSE_ESP32S2 || \
        ARDUINO_ADAFRUIT_FEATHER_ESP32S2_NOPSRAM )
#define BOARD_TYPE      "ESP32-S2"
#elif ( ARDUINO_ESP32C3_DEV )
#warning Using ESP32-C3 boards
#define BOARD_TYPE      "ESP32-C3"
#else
#define BOARD_TYPE      "ESP32"
#endif

#ifndef BOARD_NAME
  #define BOARD_NAME    BOARD_TYPE
#endif

#define DEBUG_WEBSOCKETS_PORT     Serial
// Debug Level from 0 to 4
#define _WEBSOCKETS_LOGLEVEL_     3

const char* ssid = "your ssid";          //Enter SSID
const char* password = "your passsword"; //Enter Password

const String identification = "1";       //Enter ID

// Deprecated echo.websocket.org to be replaced or it won't work
const char* websockets_connection_string = "wss://echo.websocket.org/"; //Enter server adress

// KH, This certificate was updated 15.04.2021,
// Issued on Mar 15th 2021, expired on June 13th 2021
const char echo_org_ssl_ca_cert[] PROGMEM = \
                                            "-----BEGIN CERTIFICATE-----\n" \
                                            "MIIEZTCCA02gAwIBAgIQQAF1BIMUpMghjISpDBbN3zANBgkqhkiG9w0BAQsFADA/\n" \
                                            "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
                                            "DkRTVCBSb290IENBIFgzMB4XDTIwMTAwNzE5MjE0MFoXDTIxMDkyOTE5MjE0MFow\n" \
                                            "MjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxCzAJBgNVBAMT\n" \
                                            "AlIzMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuwIVKMz2oJTTDxLs\n" \
                                            "jVWSw/iC8ZmmekKIp10mqrUrucVMsa+Oa/l1yKPXD0eUFFU1V4yeqKI5GfWCPEKp\n" \
                                            "Tm71O8Mu243AsFzzWTjn7c9p8FoLG77AlCQlh/o3cbMT5xys4Zvv2+Q7RVJFlqnB\n" \
                                            "U840yFLuta7tj95gcOKlVKu2bQ6XpUA0ayvTvGbrZjR8+muLj1cpmfgwF126cm/7\n" \
                                            "gcWt0oZYPRfH5wm78Sv3htzB2nFd1EbjzK0lwYi8YGd1ZrPxGPeiXOZT/zqItkel\n" \
                                            "/xMY6pgJdz+dU/nPAeX1pnAXFK9jpP+Zs5Od3FOnBv5IhR2haa4ldbsTzFID9e1R\n" \
                                            "oYvbFQIDAQABo4IBaDCCAWQwEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8E\n" \
                                            "BAMCAYYwSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5p\n" \
                                            "ZGVudHJ1c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTE\n" \
                                            "p7Gkeyxx+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEE\n" \
                                            "AYLfEwEBATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2Vu\n" \
                                            "Y3J5cHQub3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0\n" \
                                            "LmNvbS9EU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYf\n" \
                                            "r52LFMLGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjANBgkqhkiG9w0B\n" \
                                            "AQsFAAOCAQEA2UzgyfWEiDcx27sT4rP8i2tiEmxYt0l+PAK3qB8oYevO4C5z70kH\n" \
                                            "ejWEHx2taPDY/laBL21/WKZuNTYQHHPD5b1tXgHXbnL7KqC401dk5VvCadTQsvd8\n" \
                                            "S8MXjohyc9z9/G2948kLjmE6Flh9dDYrVYA9x2O+hEPGOaEOa1eePynBgPayvUfL\n" \
                                            "qjBstzLhWVQLGAkXXmNs+5ZnPBxzDJOLxhF2JIbeQAcH5H0tZrUlo5ZYyOqA7s9p\n" \
                                            "O5b85o3AM/OJ+CktFBQtfvBhcJVd9wvlwPsk+uyOy2HI7mNxKKgsBTt375teA2Tw\n" \
                                            "UdHkhVNcsAKX1H7GNNLOEADksd86wuoXvg==\n" \
                                            "-----END CERTIFICATE-----\n";


#endif      //defines_h

```

</details>