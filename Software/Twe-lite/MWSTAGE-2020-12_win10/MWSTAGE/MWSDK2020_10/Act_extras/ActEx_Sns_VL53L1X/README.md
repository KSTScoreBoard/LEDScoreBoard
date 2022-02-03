# Sns_VL53L1X

ToF 距離測定モジュール VL53L1X (**STMicroelectronics**社製) を動作させるためのActです。



### 本Actの作成にあたって

以下のライブラリコードを利用させていただきました。

> https://github.com/pololu/vl53l1x-arduino



以下のセンサーモジュールを利用させていただきました。

> [AE-VL53L1X](http://akizukidenshi.com/catalog/g/gM-14249/) ＶＬ５３Ｌ１Ｘ使用　レーザー測距センサモジュール（ＴｏＦ）-  株式会社秋月電子通商



### ライセンス

[MW-SLA-1](https://mono-wireless.com/jp/products/TWE-NET/license.html) (MONO WIRELESS SOFTWARE LICENSE AGREEMENT) を適用します。

ただし vl53l1xディレクトリ内は、[vl53l1x/LICENSE.txt](vl53l1x/LICENSE.txt) を参照ください。



### 目次

[TOC]



## 動作の流れ

このActプログラムは、計測→スリープ→起床→計測→スリープ... を繰り返します。



1. 初回起動１ `setup()` / 電源投入後の動作です。ハードウェアの初期化を済ました後 2. へ。
2. 初回起動２ `begin()` / 1.の直後でTWENETの各種初期化が行われた後に呼び出されます。初回のスリープに遷移します。スリープ起床後 3. へ。
3. スリープ起床 `wakeup()` / XSHUTをHiに戻しセンサーを起床させて計測を開始(`kick_sensor()`)します。以後、4. のループが呼び続けられます。
   ※ センサーからのデータが安定しない場合はこの場所に`delay()`を追加して、待ち時間を設けても良いでしょう。
4. ループ `loop()` / センサーは連続的に計測を行っています。`sns_tof.read()`を呼び出すことで、センサーの結果取得が得られるまで待ち処理を行い、結果が得られた時点で計測値を戻します。ここでは８回計測が終わった時点で処理を終了しています。



### 例外的な動作

* ８回取得する計測値のうち、最初の２ケは不安定な値になる場合があります。本Actでは、最初の２回分を読み飛ばすようにしています。
* センサーの初期化、無線送信の失敗などのエラー発生時には、モジュールリセットを行います。





## アプリケーションの書き換え

### TWELITE STAGE の導入とアップデート

MWSTAGE2020_05（または今後リリースされるより新しい版）をインストールしてください。

MWSTAGE2020_05ではライブラリ更新が必要です（今後のリリースでは更新は不要になります）。以下のファイルをダウンロード、展開して、MWSTAGE/MWSDK/TWENET ディレクトリと置き換えてください。

> https://github.com/monowireless/MWSDK_COMMON_SNAP/releases/tag/MWSDK2020_07_UNOFFICIAL



### TWELITE STAGEによる書き換え

**ActEx_Sns_vl53l1x**はTWELITE SDKのmwxライブラリ上で記述された Act です。**ActEx_Sns_vl53l1x**ディレクトリを`MWSTAGE/MWSDK/Act_samples/`に格納します。TWELITE STAGE 上でのビルドと書き換えが可能になります。

**ActEx_Sns_vl53l1x**は親機と子機で同じファームウェアを書き込んでください。

ファームウェアを書き込む手順は以下となります。

1. ファームウェアプログラムを書き込む対象(TWELITE DIPなど)を TWELITE R 経由で接続しておきます。
2. TWELITE STAGEを起動します。起動時に TWELITE R のポートを指定します。
3.  [アプリ書き換え] > [Actビルド&書換] > [**ActEx_Sns_vl53l1x**] を選択します。



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
6: ActEx_Sns_vl53l1x             <-----選択 
7: BRD_APPTWELITE                                    
8: PAL_AMB                                           
9: PAL_AMB-behavior                                  
0: PAL_AMB-usenap                                    
q: PAL_MAG                                           
```

**ActEx_Sns_vl53l1x**を選択後ビルドが始まります。無事ビルドが終わるとファームウェアが書き込まれます。

```

[ActEx_Sns_vl53lx_BLUE_L1304_V0-1-0.bin]

ファームウェアを書き込んでいます...
|                                |
書き込みに成功しました(2823ms)

中ボタンまたは[Enter]で
ﾀｰﾐﾅﾙ(ｲﾝﾀﾗｸﾃｨﾌﾞﾓｰﾄﾞ用)を開きます。
```



## 使用方法

本解説では TWELITE R または R2 と TWELITE DIP を利用します。受信機側は MONOSTICK を利用します。

1. アプリケーションを書き換え(上述参照)てください。
   1. センサーを接続する子機には本Actを書き込みます。
   2. 親機には [App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/) を書き込んでおきます。TWELITE STAGE では[TWELITE APPSビルド&書換]にあります。
2. ハードウェア結線を行います。



※ TWELITE PAL + PAL センサーボードに接続することもできますが、設定（後述）が必要になります。

※ MONOSTICK は TWELITE R/R2 + TWELITE DIP も利用できます。



### 使用するピン

App_Tweliteのピン配置に倣います。

| ピン名 | DIP# | DIO# | 意味                       |
| ------ | :--: | :--: | -------------------------- |
| SDA    |  19  |  15  | I2C  DATA                  |
| SCL    |  2   |  14  | I2C CLOCK                  |
| DIO8   |  11  |  8   | シャットダウンピン (XSHUT) |



### ハードウェア配線

以下の結線を行います。

```
 [TWELITE]       [VL53L1X]
  VCC        ---  V+
  GND        ---  GND
  DIO15(SDA) ---  SDA
  DIO14(SCL) ---  SCL
  DIO8       ---  XSHUT
```

※ VL53L1X の電源電圧を考慮の上、必要に応じてレギュレータなどを用いて 3.3V 系を供給するといった対処を行ってください。



### 運用について

* 受信側の親機は [App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/index.html) や Parent-MONOSTICK を利用してください。
* 中継を行う場合は App_Wings を利用してください。



## 設定

Act の設定は、シリアルターミナルの操作で行うもの、DIP SW など起動時のハードウェアの状態を読み出すもの、またソースコードを編集するものがあります。以下に可能な設定を列挙します。

| 当Actの対応状況          | 対応 | 備考 |
| ------------------------ | :--: | ---- |
| シリアルターミナル       |  ✕   |      |
| インタラクティブモード   |  ✕   |      |
| IO 設定(DIPスイッチなど) |  ✕   |      |
| App_Twelite M1/M2/M3/BPS |  ✕   |      |
| PAL DIPスイッチ          |  ✕   |      |
| ソースファイルの書き換え |  〇  |      |



### TWELITE PALを用いる場合

TWELITE PAL + センスPALの組み合わせの場合は、Sns_vl53l1x.cppの冒頭部の `#undef USE_PAL` を `#define USE_PAL PAL_MAG`  (開閉センサーパル : `PAL_MAG`, 環境: `PAL_AMB`, 動作: `PAL_MOT`) に書き換えてください。

```C++
#include <TWELITE>

// BOARD SELECTION (comment out either)
#define USE_PAL PAL_MAG // PAL_MOT, PAL_AMB
// #undef USE_PAL // no board defined

...
```

これは PAL 基板に内包されるハードウェアウォッチドッグ(WDT)を処理するためです。



### アプリケーションID、チャネル、子機ID

```C++
// application ID
const uint32_t APP_ID = 0x1234abcd;

// channel
const uint8_t CHANNEL = 13;

// id
uint8_t u8ID = 0xFE;
```

Sns_vl53l1x.cppの冒頭部の上記定義を編集してください。



### センサーの計測回数

```C++
// measurement result
const uint8_t CT_MEASURE_MAX = 8;
const uint8_t CT_MEASURE_PRE = 2; // skipping count
```

Sns_vl53l1x.cppの冒頭部の上記定義を編集してください。CT_MEAUSRE_MAX が計測回数（無線パケットに格納される個数）、CT_MEASURE_PREが読み飛ばし回数となります。読み飛ばしは２としています。



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

Sns_vl53l1x.cppの`loop()`中の`tx_retry()`の値を変更することで再送回数を指定できます。例えば再送回数を`0x2`にすると、都合３回の送信が行われます。また`tx_packet_delay()`のパラメータは、再送時の遅延や再送間隔を指定します。(詳細は https://mwx.twelite.info/api-reference/classes/packet_tx を参照ください)



### スリープ間隔

```c++
void sleepNow() {
	uint32_t u32ct = 2000;
    u32ct = random(u32ct - u32ct / 8, u32ct + u32ct / 8); // add random to sleeping ms.
	Serial << "..sleeping " << int(u32ct) << "ms." << mwx::crlf;

	the_twelite.sleep(u32ct, false);
}
```

Sns_vl53l1x.cppの`sleepNow()`中の`u32ct`を書き換えます。

たとえば乱数によるタイミングのブレが不要で、可能な限り正確に５秒おきの送信を行いたい場合は以下のようになります。`the_twelite.sleep()`の２番目のパラメータを`true`にすることで、直線の起床タイミングから起算して５秒後に起床する設定となります。

```C++
void sleepNow() {
	uint32_t u32ct = 5000;
	Serial << "..sleeping " << int(u32ct) << "ms." << mwx::crlf;

	the_twelite.sleep(u32ct, true);
}
```





## データ構造

Act での送信形式は以下のようなデータ順になっています。

| バイト位置 | バイト数             | 内容                                                    |
| ---------- | -------------------- | ------------------------------------------------------- |
| 0          | 4                    | 固定文字が格納されます `{ 'S', 'V', '5', '3' }`         |
| 4          | 1                    | サンプルデータ数(CT_MEASURE_MAX=8`) が格納されます。    |
| 5          | 2 x `CT_MEASURE_MAX` | 符号なし16bit値で、センサーの計測値[mm]が格納されます。 |



[App_Wings](https://mono-wireless.com/jp/products/TWE-APPS/App_Wings/index.html) で受信した場合は、シリアルポートに以下のように出力されます。

```
:FEAA00810C7A6400000000C6001553563533080083008B008700880081008800830083CD
 ^1^2^3^4      ^5      ^6^7  ^8                                        ^9
```



|  #   | バイト数 | 意味                   | データ例          | データ例の内容                                | 備考                                                   |
| :--: | :------: | :--------------------- | :---------------- | :-------------------------------------------- | :----------------------------------------------------- |
|  1   |    1     | 送信元の論理デバイスID | FE                | 送信元の論理デバイスIDは0xFE                  | 0xFEは特別なIDを指定しない子機                         |
|  2   |    1     | コマンド種別           | AA                | アクトのパケット                              | 0xAA固定                                               |
|  3   |    1     | 応答ID                 | 00                |                                               | 任意の0x00～0x7Fの値                                   |
|  4   |    4     | 送信元のシリアルID     | 810C7A64          | 送信元のシリアルIDは10C7A64                   |                                                        |
|  5   |    4     | 送信先のシリアルID     | 00000000          |                                               | 00000000のときは論理デバイスIDを指定して送信している。 |
|  6   |    1     | LQI                    | C6                | 0xC6=198                                      | 0が最小で255が最大                                     |
|  7   |    2     | データのバイト数       | 0015              | 21バイト                                      |                                                        |
|  8   |    N     | 上述のActのデータ列    | 5356...<br />0083 | 53563533<br />08<br />0083<br />008B<br />... | "SV53"<br />8<br />L1=131mm<br />L2=139mm<br />...     |
|  9   |    1     | チェックサム(LRC)      | CD                |                                               |                                                        |



## Actのシリアルメッセージについて

本Actでは、起動から起床、計測、無線送信のメッセージを出力するようにしています。主に動作確認を目的としています。



`..wake up.`から動作が開始されます。`[INIT(00002)]`は`loop()`内の初期化状態の処理です。`()`内の数字は、内部のmsカウンタです。

`[ss.........]`は計測完了を示すメッセージです。`s`は読み飛ばし２計測分、`.`は通常計測、`t`はタイムアウトです。

計測が終了すると `[TX REQUEST(00946)] id = 1`の行が表示されます。無線送信要求を `id=1` で行ったことを示します。

最期に `[SUCCESS(00959)]` 、`..sleeping 2203ms` が表示されスリープを実行します。スリープの期間は 2000 ± 250[ms] としています。

```
--- VL53L1X sample code ---
..sleeping 2232ms.

..wake up.
[INIT(00002)]
[ss........]
L[mm]=  133  147  137  143  139  137  147  145 ave= 141
[TX REQUEST(00946)] id = 1.
[SUCCESS(00959)]
..sleeping 2203ms.

........

..wake up.
[INIT(28829)]
[ss........]
L[mm]=  138  133  137  136  129  141  135  137 ave= 135
[TX REQUEST(29773)] id = 31.
[SUCCESS(29786)]
..sleeping 1878ms.
```

