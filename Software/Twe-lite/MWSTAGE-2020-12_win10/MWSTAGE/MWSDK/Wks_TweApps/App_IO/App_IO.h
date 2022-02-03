/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE *
 * AGREEMENT).                                                    */

/** @file
 * アプリケーションのメイン処理
 *
 * @defgroup MASTER アプリケーションのメイン処理
 */

#ifndef  MASTER_H_INCLUDED
#define  MASTER_H_INCLUDED

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "ToCoNet.h"
#include "flash.h"
#include "btnMgr.h"

#include "Interactive.h"
#include "common.h"

/** @ingroup MASTER
 * ネットワークのモード列挙体 (ショートアドレス管理か中継ネットワーク層を使用したものか)
 */
typedef enum {
	E_NWKMODE_MAC_DIRECT,//!< ネットワークは構成せず、ショートアドレスでの通信を行う
	E_NWKMODE_LAYERTREE  //!< 中継ネットワークでの通信を行う(これは使用しない)
} teNwkMode;

/** @ingroup MASTER
 * IO の状態
 */
typedef struct {
	uint32 u32BtmBitmap; //!< 現在のビットの状況 (0xFFFFFFFF: 未確定)
	uint32 u32BtmUsed; //!< 利用対象ピンかどうか (0xFFFFFFFF: 未確定)
	uint32 u32BtmChanged; //!< 変更があったポートまたは割り込み対象ピン (0xFFFFFFFF: 未確定)

	uint8 au8Input[MAX_IO_TBL]; //!< 入力ポート (0: Hi, 1: Lo, 0xFF: 未確定)
	uint8 au8Output[MAX_IO_TBL]; //!< 出力ポート (0: Hi, 1:Lo, 0xFF: 未確定)

	uint32 u32TxLastTick; //!< 最後に送った時刻
	uint32 u32RxLastTick; //!< 最後に受信した時刻

	int16 i16TxCbId; //!< 送信時のID
} tsIOData;

/** @ingroup MASTER
 * IO 設定要求
 */
typedef struct {
	uint8 u16IOports;          //!< 出力IOの状態 (1=Lo, 0=Hi)
	uint8 u16IOports_use_mask; //!< 設定を行うポートなら TRU
} tsIOSetReq;

/** @ingroup MASTER
 * アプリケーションの情報
 */
typedef struct {
	// ToCoNet
	uint32 u32ToCoNetVersion; //!< ToCoNet のバージョン番号を保持
	uint16 u16ToCoNetTickDelta_ms; //!< ToCoNet の Tick 周期 [ms]
	uint8 u8AppIdentifier; //!< AppID から自動決定

	// メインアプリケーション処理部
	void *prPrsEv; //!< vProcessEvCoreSlpまたはvProcessEvCorePwrなどの処理部へのポインタ

	// DEBUG
	uint8 u8DebugLevel; //!< デバッグ出力のレベル

	// Wakeup
	bool_t bWakeupByButton; //!< TRUE なら起床時に DI 割り込みにより起床した
	uint32 u32SleepDur; //!< スリープ間隔 [ms]

	// mode3 fps
	uint8 u8FpsBitMask; //!< mode=3 連続送信時の秒間送信タイミングを判定するためのビットマスク (64fps のカウンタと AND を取って判定)

	// Network mode
	teNwkMode eNwkMode; //!< ネットワークモデル(未使用：将来のための拡張用)
	uint8 u8AppLogicalId; //!< ネットワーク時の抽象アドレス 0:親機 1~:子機, 0xFF:通信しない

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk; //!< ネットワークコンテキスト(未使用)
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig; //!< LayerTree の設定情報(未使用)

	// Flash Information
	tsFlash sFlash; //!< フラッシュからの読み込みデータ
	tsFlashApp sConfig_UnSaved; //!< フラッシュへの設定データ (0xFF, 0xFFFFFFFF は未設定)
	int8 bFlashLoaded; //!< フラッシュからの読み込みが正しく行われた場合は TRUE

	uint32 u32DIO_startup; //!< 電源投入時のIO状態

	// config mode
	uint8 u8Mode; //!< 動作モード(IO M1,M2,M3 から設定される)
	uint8 u8ChCfg; //!< チャネル設定(EI1,EI2 から設定される)
	uint8 u8IoTbl; //!< IO割り当てテーブル番号

	// button manager
	tsBTM_Config sBTM_Config; //!< ボタン入力（連照により状態確定する）管理構造体
	PR_BTM_HANDLER pr_BTM_handler; //!< ボタン入力用のイベントハンドラ (TickTimer 起点で呼び出す)
	uint32 u32BTM_Tick_LastChange; //!< ボタン入力で最後に変化が有ったタイムスタンプ (操作の無効期間を作る様な場合に使用する)

	uint16 au16HoldBtn[MAX_IO_TBL]; //!< ボタンの入力を一定時間維持する
	uint32 u32BtnMask_Special; //!< ボタンの入力に対する特殊設定に対応するマスク

	// latest state
	tsIOData sIOData_now; //!< 現時点での IO 情報
	tsIOData sIOData_reserve; //!< 保存された状態(0がデフォルトとする)
	uint8 u8IOFixState; //!< IOの読み取り確定ビット

	// Counter
	uint32 u32CtTimer0; //!< 64fps カウンタ。スリープ後も維持
	uint16 u16CtTimer0; //!< 64fps カウンタ。起動時に 0 クリアする
	uint16 u16CtRndCt; //!< 起動時の送信タイミングにランダムのブレを作る

	uint8 u8UartReqNum; //!< UART の要求番号

	uint16 u16TxFrame; //!< 送信フレーム数
	uint8 u8SerMsg_RequestNumber; //!< シリアルメッセージの要求番号

	bool_t bCustomDefaults; //!< カスタムデフォルトがロードされたかどうか

	uint8 u8RxSetting; //!< bit0: 起動時 bit1: 常時

	uint8 u8StandardTxRetry; //!< デフォルトの再送設定
	uint8 u8StandardTxAckRetry; //!< デフォルトの再送設定(ACKモード時)
} tsAppData;

/****************************************************************************
 * 内部処理
 ***************************************************************************/
#define E_IO_FIX_STATE_NOT_READY 0x0
#define E_IO_FIX_STATE_READY 0x1

#define RX_STATE_BOOT_ON 0x01
#define RX_STATE_CONTINUOUS 0x02

#define HZ_LOW_LATENCY 1000 //!< 低レイテンシー時の制御周期 [Hz]

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
