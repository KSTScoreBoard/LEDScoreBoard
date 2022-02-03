# Sns_BME280_SHT30

Bosch の環境センサー BME280/BMP280 と Senserion の温湿度センサー SHT3x を動作させるためのコードが含まれます。



### 本Actの作成にあたって

以下のセンサーモジュールを利用させていただきました。

> 秋月電子通商 ＢＭＥ２８０使用　温湿度・気圧センサモジュールキット (http://akizukidenshi.com/catalog/g/gK-09421/)
>
> M5STACK ENV II Unit (STH30+BMP280) (https://m5stack.com/products/env-ii-unit) 



### ライセンス

[MW-SLA-1](https://mono-wireless.com/jp/products/TWE-NET/license.html) (MONO WIRELESS SOFTWARE LICENSE AGREEMENT) を適用します。



### 目次

[TOC]



## 開発環境について

TWELITE STAGE の配布パッケージに開発環境 MWSDK が含まれます。本コードはMWSDKの配布バージョン、mwxライブラリバージョンに依存しますので更新をお願いします。

* MWSDK [2020_08_UNOFFICIAL](https://github.com/monowireless/MWSDK_COMMON_SNAP/releases/tag/MWSDK2020_08_UNOFFICIAL) 以降に更新してください。([ライブラリの更新方法](https://sdk.twelite.info/latest#rirsunonado))
  * 上記MWSDK付属のmwxライブラリでは動作しません。mwxライブラリを [1.6.1b2](https://github.com/monowireless/mwx/releases/tag/0.1.6b2) に書き換えてください。([mwxライブラリの更新方法](https://mwx.twelite.info/revisions))



**2020/09以降にリリースされる MWSDK では、上記更新は不要です。**





## ハードウェア接続について

Grove 接続のセンサーについては通常電源電圧が 5V となっています。TWELITE 無線モジュールへの接続には、電圧の整合を考慮する必要があります。

TWELITE 側の電圧をそのまま供給する場合は、センサー側の回路構成や電圧（3.3Vの定電圧を供給する）によっては動作する場合もありますが、予期しない問題が発生する場合もあります。ご注意ください。



| 端子         | 内容                                                         |      |
| ------------ | ------------------------------------------------------------ | ---- |
| TWELITE VCC  | 2.3(2.0)V から 3.6Vを供給する（電池など)。                   |      |
| Grove VCC    | 5V を供給。TWELITE VCC から 5V への昇圧DC/DCなど<br />http://akizukidenshi.com/catalog/g/gK-13065/ など |      |
| I2C SDL, SDA | GROVE は 3.3V が原則だが、デバイス毎に確認が必要。TWELITE VCCの電圧と差がある場合は、双方のデバイス間での定格範囲内であれば動作は期待できますが、より厳格な動作（リーク電流の抑制など）が必要な場合は、I2Cバス用のレベル変換 IC などを用いて電圧変換を推奨します。<br />http://akizukidenshi.com/catalog/g/gM-05452/<br />※電圧変換ICを挟むと正常に動作しないデバイスもあります。 |      |
| GND          | TWELITE/Grove 双方のGNDを共通接続する。                      |      |

参考：回路に関する考慮事項



## 動作の流れ

このActプログラムは、計測→スリープ→起床→計測→スリープ... を繰り返します。



1. 初回起動１ `setup()` / 電源投入後の動作です。ハードウェアのチェック・初期化を済ました後 2. へ。
2. `loop()` 中では `do .. while()` による状態遷移で、センサーのデータ取得～無線送信までの処理を順に行う。ADCについては取得出来次第、ADC値を格納している。
3. `E_STATE::INIT`ではセンサーの初期化と計測開始を行う。
4. `E_STATE::CAPTURE_PRE`ではセンサーの計測終了待ちを行う。
5. `E_STATE::CAPTURE`ではセンサーの計測値をUARTに出力している。
6. `E_STATE::TX`ではセンサーデータを含んだ無線パケットを生成し、送信依頼を行っている。
7. `E_STATE::TX_WAIT_COMP`では無線パケットの送信完了待ちを行っている。
8. `E_STATE::SUCCESS`では `sleepNow()` を呼び出しスリープに遷移している。
9. スリープ復帰後は `wakeup()` 関数が呼び出され、ハードウェアの初期化などを行い、`loop() `が呼び出され 2. へ。



### 例外的な動作

* `setup()` でI2Cバス上のセンサーより応答がない場合は、そのセンサーへのアクセスは行わない。
* センサーの初期化、無線送信の失敗などのエラー発生時には、モジュールリセットを行います。`b_found_sht3x`, `b_found_bme280`



### インタラクティブモード

インタラクティブモードは DIO12 (SET) ピンを LOW にして、電源投入またはリセットする。

以下の設定に対応している。

* アプリケーションID
* チャネル
* デバイスID (0x00: 親機の指定は不可)





## アプリケーションの書き換え

**上述の開発環境の更新を行ってください。**



### Actフォルダの中身

Actフォルダ名は適宜変更しても構いません。ただし空白や日本語名が含まれてはいけません。ここでは **ActEx_TheAct** というフォルダ名にします。（通常はGitのリポジトリ名を利用したフォルダ名になります）

```
ActEx_TheAct/
   .vscode/           --- Micorsoft 社の Visual Studio Code 用の定義ファイル
   ActEx_TheAct.cpp   --- ソースファイル（Actによっていくつかのファイルやフォルダが追加されます）
   build/             --- ビルドフォルダ
     Makefile         --- Makefile
     ...
```



### TWELITE STAGEによる書き換え

本Actフォルダ(ここでは **ActEx_TheAct**と記載します)を`MWSTAGE/MWSDK/Act_samples/`に格納します。TWELITE STAGE 上でのビルドと書き換えが可能になります。

ファームウェアを書き込む手順は以下となります。

1. ファームウェアプログラムを書き込む対象(TWELITE DIPなど)を TWELITE R 経由で接続しておきます。
2. TWELITE STAGEを起動します。起動時に TWELITE R のポートを指定します。
3.  [アプリ書き換え] > [Actビルド&書換] > [**ActEx_TheAct**] を選択します。



TWELITE STAGE 0.8.9 の画面表示例です。

```
1: ビューア                                 
2: アプリ書換                    <-----選択
3: インタラクティブモード                          
4: TWELITE® STAGEの設定                    
5: シリアルポート選択[MWXXXXXXX]                  
```

```
1: BINから選択                                           
2: Actビルド&書換                <-----選択    
3: TWELITE APPSビルド&書換                                
4: 指定 [ﾌｫﾙﾀﾞをﾄﾞﾛｯﾌﾟ]                                 
5: 再書換 [無し]                                          
```

```
1: act0                                              
2: act1                                              
3: act2                                              
4: act3                                              
5: act4
6: ActEx_TheAct                <-----選択 
7: BRD_APPTWELITE                                    
8: PAL_AMB                                           
9: PAL_AMB-behavior                                  
0: PAL_AMB-usenap                                    
q: PAL_MAG                                           
```

**ActEx_TheAct**を選択後ビルドが始まります。無事ビルドが終わるとファームウェアが書き込まれます。

```

[ActEx_TheAct_BLUE_L1304_V0-1-0.bin]

ファームウェアを書き込んでいます...
|                                |
書き込みに成功しました(3000ms)

中ボタンまたは[Enter]で
ﾀｰﾐﾅﾙ(ｲﾝﾀﾗｸﾃｨﾌﾞﾓｰﾄﾞ用)を開きます。
```



## 使用方法

本Actはセンサー子機側です。もっぱらセンサーデータ取得と無線パケットを送信します。この無線パケットを受信する場合は App_Wings に書き換え、アプリケーションIDとチャネルの設定を行うようにしてください（本アクトデフォルトのアプリケーションIDは `1234abcd`、チャネルは `13` です）。また Act_samples/Parent-MONOSTICK をひな形として、所望の受信後の処理を行う Act を作成することもできます。

本解説では TWELITE R または R2 と TWELITE DIP を利用します。受信機側は MONOSTICK を利用します。

※ MONOSTICK の替わりに TWELITE R/R2 + TWELITE DIP の組み合わせでも、同じように親機として利用できます。

### 書き込み確認時の運用
1. 親機側 (MONOSTICK) に Parent-MONOSTIC (TWELITE STAGE, Actビルド&書換->Parent-MONOSTICKを選択) を書き込み後、ターミナル(TWELITE STAGE のターミナルなど)で開いておいてください。
  - [App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/) を書き込んだ場合は、書き込み後に設定 (アプリケーションID `1234abcd`、チャネル `13`) が必要です。
2. 子機側のハードウェアの結線を行い、十分確認してください。
3. 子機側を TWELITE R(2) に接続してください。<br />
  TWELITE STAGE でデバイス名(8桁の英数字, COM?ではない)が表示されない場合は、すぐに USB バスから切り離し、子機の電源をカットしてください。適切な接続ではなく過電流が流れるなど問題が出ている可能性があります。
4. TWELITE STAGE から本アクトを選択し、書き込みを行ってください。<br />
  書き込みが正常終了したらそのまま子機側をリセットし動作が開始します。

※ TWELITE R(2) が接続したまま、子機側で配線の変更はTWELITEモジュールの電源ON/OFFを行うと、PC側のシリアルポートの接続が外れる場合があります。TWELITE STAGEを終了し TWELITE R(2) を抜き差しするといった作業が必要になる場合があります。

### 通常の運用
1. 親機(App_Wings など)は稼働状態にしておきます。
2. 子機側はTWELITE R(2)は接続しない結線に変更し、電源（電池）を投入します。



### 使用するピン

| ピン名 | DIP# | DIO# | 意味                                                         |
| ------ | :--: | :--: | ------------------------------------------------------------ |
| SDA    |  19  |  15  | I2C  DATA                                                    |
| SCL    |  2   |  14  | I2C CLOCK                                                    |
| A1     |  22  | ADC1 | ADC1<br />(不要であればオープンでも構わないが、オープン時の計測値は不定) |
| VCC    |  28  | VCC  | 電源                                                         |
| GND    | 1,14 | GND  | グランド                                                     |



TWELITE R/TWELITE R2

| ピン名 | DIP# |  DIO#   | 意味                                                         |
| ------ | :--: | :-----: | ------------------------------------------------------------ |
| RST    |  21  |  RESET  | リセット                                                     |
| RXD    |  3   |    9    | シリアル信号線 (TWELITE 側 RxD)                              |
| PRG    |  7   | SPIMISO | プログラムピン (GND でリセットするとプログラムモードに遷移)  |
| TXD    |  10  |    6    | シリアル信号線 (TWELITE 側 RxD)                              |
| GND    | GND  |  1,14   | グランド                                                     |
| VCC    | VCC  |   28    | (TWELITE R2) 電源供給3.3V                                    |
| SET    |  15  |   12    | (TWELITE R2) SET (リセット時、インタラクティブモードへ遷移)<br />※TWELITE R2 の #7 ピンを接続すれば TWELITE STAGE から制御可能 |

* VCC/SET は TWELITE R2 のみ対応



### ハードウェア配線

以下の結線を行います。

```
 [TWELITE]       [BME280, SHT3x]
  VCC        ---  V+
  GND        ---  GND
  DIO15(SDA) ---  SDA
  DIO14(SCL) ---  SCL

プルアップ抵抗
  DIO15(SDA) ---[4.7KΩ]--- VCC
  DIO14(SCL) ---[4.7KΩ]--- VCC
  
SETピン接続
  DIO12(SET) ---[ボタン]--- GND
  
TWELITE R2 (開発中)
  「使用するピン」の表に基づいて結線する。
```

※ SDA/SCL には適切なプルアップ抵抗(例：4.7kΩ)を追加してください。

※ ファームウェアの書き込みや動作確認、開発中は TWELITE R2 の接続し TWELITE STAGE アプリから利用することで、書き込み、リセット、SETピンの制御、シリアル（UART）ポートの観察などが簡便になります。



### 運用について

* 受信側の親機は [App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/index.html) や Parent-MONOSTICK を利用してください。
* 中継を行う場合は App_Wings を利用してください。



## 設定

Act の設定は、シリアルターミナルの操作で行うもの、DIP SW など起動時のハードウェアの状態を読み出すもの、またソースコードを編集するものがあります。以下に可能な設定を列挙します。

| 当Actの対応状況          | 対応 | 備考 |
| ------------------------ | :--: | ---- |
| シリアルターミナル       |  ✕   |      |
| インタラクティブモード   |  〇  |      |
| IO 設定(DIPスイッチなど) |  ✕   |      |
| App_Twelite M1/M2/M3/BPS |  ✕   |      |
| PAL DIPスイッチ          |  ✕   |      |
| ソースファイルの書き換え |  〇  |      |



### TWELITE PALを用いる場合

TWELITE PAL + センスPALの組み合わせの場合は、Actコード(*.cpp)冒頭部の `#undef USE_PAL` を `#define USE_PAL PAL_MAG`  (開閉センサーパル : `PAL_MAG`, 環境: `PAL_AMB`, 動作: `PAL_MOT`) に書き換えてください。

```C++
#include <TWELITE>

// BOARD SELECTION (comment out either)
#define USE_PAL PAL_MAG // PAL_MOT, PAL_AMB
// #undef USE_PAL // no board defined

...
```

これは PAL 基板に内包されるハードウェアウォッチドッグ(WDT)を処理するためです。

PAL基板を用いる場合はスリープ間隔を60秒以下に設定します。それ以上経過すると（９０秒前後）ウォッチドッグタイマーが動作します。



### 無線送信パラメータ（再送）

```C++
/*** the loop procedure (called every event) */
void loop() {
...
            case E_STATE::TX:
            ...
            	pkt << tx_addr(0x00)  // 0..0xFF (LID 0:parent...)
                    << tx_retry(0x1) // set retry (0x1 send two times in total)
                    << tx_packet_delay(0, 0, 2); // send packet w/ delay

```

`loop()`中の`tx_retry()`の値を変更することで再送回数を指定できます。例えば再送回数を`0x2`にすると、都合３回の送信が行われます。また`tx_packet_delay()`のパラメータは、再送時の遅延や再送間隔を指定します。(詳細は https://mwx.twelite.info/api-reference/classes/packet_tx を参照ください)



### スリープ間隔

```c++
void sleepNow() {
	uint32_t u32ct = 1750 + random(0,500);
	Serial << crlf << format("..%04d/sleeping %dms.", millis() % 8191, u32ct);
	Serial.flush();

	the_twelite.sleep(u32ct);
}
```

`sleepNow()`中の`u32ct`を書き換えます。

たとえば乱数によるタイミングのブレが不要で、可能な限り正確に５秒おきの送信を行いたい場合は以下のようになります。`the_twelite.sleep()`の２番目のパラメータを`true`にすることで、直線の起床タイミングから起算して５秒後に起床する設定となります。





## データ構造

Act での送信形式は以下のようなデータ順になっています。

| バイト位置 | バイト数 | 内容                                                         |
| ---------- | -------- | ------------------------------------------------------------ |
| 0          | 4        | 固定文字が格納されます `{ 'S', 'B', 'S', '1' }`              |
| 4          | 2        | 符号付き16bit値で、SHT3x の温度[℃]の100倍の値が格納されます。24.56℃なら`2456`です。 |
| 6          | 2        | 符号付き16bit値で、SHT3x の湿度[%]の100倍の値が格納されます。40.34%なら`4034`です。 |
| 8          | 2        | 符号付き16bit値で、BMx280 の温度[℃]の100倍の値が格納されます。24.56℃なら`2456`です。 |
| 10         | 2        | 符号付き16bit値で、BMx280 の湿度[%]の100倍の値が格納されます。40.34%なら`4034`です。 |
| 12         | 2        | 符号付き16bit値で、圧力値[hPa]が格納されます。1002hPaなら`1002`です。 |
| 14         | 2        | 電源電圧 [mV]                                                |
| 16         | 2        | ADC1 の電圧 [mV]                                             |

※ センサー値がエラーの場合は -32760～-32767 の値を戻します。(-32767: 接続エラー, それ以外: 未定義エラー)



[App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/index.html) で受信した場合は、シリアルポートに以下のように出力されます。
(Parent-MONOSTICK の場合は書式が少し違いますが、上記データ列は共通ですので読み替えてください。)

```
:01AA008201029100000000A20012534253310A8E12B30AAA800103EA0D210007BE
 ^1^2^3^4      ^5      ^6^7  ^8                                  ^9
```



|  #   | バイト数 | 意味                   | データ例          | データ例の内容                                               | 備考                                                         |
| :--: | :------: | :--------------------- | :---------------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
|  1   |    1     | 送信元の論理デバイスID | 01                | 送信元の論理デバイスIDは0x01                                 | 0xFEは特別なIDを指定しない子機                               |
|  2   |    1     | コマンド種別           | AA                | アクトのパケット                                             | 0xAA固定                                                     |
|  3   |    1     | 応答ID                 | 00                |                                                              | 任意の0x00～0x7Fの値                                         |
|  4   |    4     | 送信元のシリアルID     | 82010291          | 送信元のシリアルIDは2010291                                  |                                                              |
|  5   |    4     | 送信先のシリアルID     | 00000000          |                                                              | 00000000は親機(0x00)宛。                                     |
|  6   |    1     | LQI                    | A2                | 0xA2=162                                                     | 0が最小で255が最大                                           |
|  7   |    2     | データのバイト数       | 0012              | 18バイト                                                     |                                                              |
|  8   |    N     | 上述のActのデータ列    | 5342...<br />0007 | 53425331<br />0A8E<br />12B3<br />0AAA<br />8001<br />03EA<br />0D21<br />0007 | "SBS3"<br />27.02℃<br />47.87%<br />27.30℃<br />湿度なし<br />1002hPa<br />3361mV(VCC)<br />7mV(ADC1) |
|  9   |    1     | チェックサム(LRC)      | BE                |                                                              |                                                              |





## Actのシリアルメッセージについて

本Actでは、起動から起床、計測、無線送信のメッセージを出力するようにしています。主に動作確認を目的としています。



```
--- ENV SENSOR:SBS1 ---                  電源・リセット時のメッセージ
..APP_ID   = 1234ABCD　　　　　　　　　　　  設定値の表示
..CHANNEL  = 13
..LID      = 1(0x01)
..found sht3x at 0x44                    SHT3xのセンサー発見
..found BMP280 ID=58 at 0x76             BMP280のセンサー発見
..0000/start sensor capture.
..0019/finish sensor capture.
  SHT3X    : T=28.45C H=44.64%           センサー値の表示
  BMP280   : T=28.68C P=1002hP
  ADC      : Vcc=3361mV A1=0007mV
..0022/transmit request by id = 1.       送信要求
..0032/transmit complete.                送信完了
..0032/sleeping 1836ms.                  スリープ期間

--- ENV SENSOR:SBS1 wake up ---          スリープ復帰時のメッセージ
..0038/start sensor capture.
..0057/finish sensor capture.
  SHT3X    : T=28.39C H=44.65%
  BMP280   : T=28.66C P=1002hP
  ADC      : Vcc=3361mV A1=0007mV
..0060/transmit request by id = 2.
..0070/transmit complete.
..0070/sleeping 1978ms.
```

