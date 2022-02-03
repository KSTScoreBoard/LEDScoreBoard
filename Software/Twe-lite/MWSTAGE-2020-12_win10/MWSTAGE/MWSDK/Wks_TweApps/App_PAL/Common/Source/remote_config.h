/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef REMOTE_CONFIG_H_
#define REMOTE_CONFIG_H_

/*
[概要]
  設定サーバの設定内容を子機(EndDevice_Input)に無線経由で反映します。
  子機にシリアル接続を行うことが煩雑な場合に利用します。

[手順]
1. 設定サーバを起動します。自動的にインタラクティブモードになり、子機に反映したい設定
  を行います。
  ※ セーブは任意です。未セーブ情報も反映されます。

2. 子機のM2ピンをLoに落としたまま親機の至近距離で電源投入します。
 - 親機では、以下の様な表示が行われます。
  !INF REQUEST CONF FR 81000096
  >>> TxCmp Ok(tick=51212,req=#6) <<<
  !INF ACK CONF FR 81000096

  また ToCoStick 上の赤色と黄色のLEDが１秒間両方点灯します。
  （失敗した場合は赤色のみの点灯となります）

 - 子機は自動的に再始動します。
  設定に失敗した時は、子機はインタラクティブモードを維持します。

[その他]
 - 設定を有効にするのは親機と子機の距離をごく近くにし LQI 値を 150 以上になった場合のみです。
   他の設定サーバとの混信を避ける目的もあります。

   独自の運用をしたい場合は Samp_Monitor/Common/Source_User/config.h 中の以下を編集します。
    #define APP_ID_CNFMST       0x67726405 <== 設定時に使用されるアプリケーションID
    #define CHANNEL_CNFMST		25         <== 設定時に使用されるチャネル

[関連ファイル]
   EndDevice_Input の vProcessEv_Config.c → 子機側の状態マシン
   EndDevice_Input の vProcessEv_ConfigMaster.c → 設定親機側の状態マシン
 */

/*
 * パケット定義:
 *
 *   OCTET    : パケットバージョン (1)
 *
 *     [以下パケットバージョン 1]
 *     OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
 *     OCTET(4) : アプリケーションバージョン
 *
 *       [パケット種別 = 要求]
 *       データなし
 *
 *       [パケット種別 = 応答]
 *       OCTET    : 設定有効化 LQI
 *       OCTET    : データ形式 (0: ベタ転送, 1pkt)
 *       OCTET    : データサイズ
 *       OCTET(N) : 設定データ
 *
 *       [パケット種別 = ACK]
 *       OCTET    : SUCCESS(0) FAIL(1)
 */

#define RMTCNF_PKTCMD 3 //!< パケットのコマンド

#define RMTCNF_MINLQI 100 //!< 有効なLQI値

#define RMTCNF_PRTCL_VERSION 0x11 //!< パケットバージョン

// パケット種別
#define RMTCNF_PKTTYPE_REQUEST 0
#define RMTCNF_PKTTYPE_DATA 1
#define RMTCNF_PKTTYPE_ACK 2

// パケットデータ形式
#define RMTCNF_DATATYPE_RAW_SINGLE_PKT 0

// Ack データ
#define RMTCNF_ACK_SUCCESS 0
#define RMTCNF_ACK_ERROR 1

#endif /* REMOTE_CONFIG_H_ */
