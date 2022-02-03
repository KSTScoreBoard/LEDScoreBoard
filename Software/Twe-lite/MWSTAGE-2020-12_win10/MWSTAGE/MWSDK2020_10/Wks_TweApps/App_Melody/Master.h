/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

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
#include "adc.h"

/** @ingroup MASTER
 * 使用する無線チャネル数の最大値 (複数設定すると Channel Agility を利用する)
 */
#define MAX_CHANNELS 3

/** @ingroup MASTER
 * ネットワークのモード列挙体 (ショートアドレス管理か中継ネットワーク層を使用したものか)
 */
typedef enum {
	E_NWKMODE_MAC_DIRECT,//!< ネットワークは構成せず、ショートアドレスでの通信を行う
	E_NWKMODE_LAYERTREE  //!< 中継ネットワークでの通信を行う
} teNwkMode;

/** @ingroup MASTER
 * IO の状態
 */
typedef struct {
	uint32 u32BtmBitmap; //!< (0xFFFFFFFF: 未確定)
	uint32 u32BtmUsed; //!< 利用対象ピンかどうか (0xFFFFFFFF: 未確定)
	uint32 u32BtmChanged; //!< (0xFFFFFFFF: 未確定)

	uint16 au16InputADC[4]; //!< (0xFFFF: 未確定) 入力ポートの ADC 値 [mV]
	uint16 au16OutputDAC[4]; //!< (0xFFFF: 未確定) 送られてきた ADC 値 [mV]
	uint16 u16Volt; //!< 12bits, 0xFFFF: 未確定
	int16 i16Temp; //!< 12bits

	uint8 au8Input[4]; //!< 入力ポート (0: Hi, 1: Lo, 0xFF: 未確定)
	uint8 au8Output[4]; //!< 出力ポート (0: Hi, 1:Lo, 0xFF: 未確定)

	uint16 au16OutputPWMDuty[4]; //!< 無線経由で送られた DUTY 比 (0xFFFF: 未設定、無効)
	uint16 au16InputPWMDuty[4]; //!< 入力された AD に基づく DUTY 比の計算値 (0xFFFF: 未設定、無効)

	uint8 u8Volt; //!< i16Volt から変換

	uint32 u32TxLastTick; //!< 最後に送った時刻
	uint16 au16InputADC_LastTx[4]; //!< 最後に送信したデータ
	uint32 u32RxLastTick; //!< 最後に受信した時刻

	uint16 au16InputADC_History[4][4]; //!< ADCデータ履歴
	uint16 u16Volt_LastTx; //!< 最後に送信した電圧
	uint16 au16Volt_History[32]; //!< ADCデータ電圧履歴
	uint8 u8HistIdx; //!< 履歴情報のインデックス
	int16 i16TxCbId; //!< 送信時のID
} tsIOData;

#define HIST_VOLT_SCALE 5 //!< 電圧履歴数のスケーラ (2^HIST_VOLT_SCALE)  @ingroup MASTER
#define HIST_VOLT_COUNT (1UL << HIST_VOLT_SCALE) //!< 電圧履歴数 @ingroup MASTER


/** @ingroup MASTER
 * IO 設定要求
 */
typedef struct {
	uint8 u8IOports;          //!< 出力IOの状態 (1=Lo, 0=Hi)
	uint8 u8IOports_use_mask; //!< 設定を行うポートなら TRUE
	uint16 au16PWM_Duty[4];      //!< PWM DUTY 比 (0～1024)
	uint8 au16PWM_use_mask[4];   //!< 設定を行うPWMポートなら TRUE
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

	// button manager
	tsBTM_Config sBTM_Config; //!< ボタン入力（連照により状態確定する）管理構造体
	PR_BTM_HANDLER pr_BTM_handler; //!< ボタン入力用のイベントハンドラ (TickTimer 起点で呼び出す)
	uint32 u32BTM_Tick_LastChange; //!< ボタン入力で最後に変化が有ったタイムスタンプ (操作の無効期間を作る様な場合に使用する)

	// ADC
	tsObjData_ADC sObjADC; //!< ADC管理構造体（データ部）
	tsSnsObj sADC; //!< ADC管理構造体（制御部）
	bool_t bUpdatedAdc; //!< TRUE:ADCのアップデートが有った。アップデート検出後、FALSE に戻す事。
	uint8 u8AdcState; //!< ADCの状態 (0xFF:初期化前, 0x0:ADC開始要求, 0x1:AD中, 0x2:AD完了)

	// latest state
	tsIOData sIOData_now; //!< 現時点での IO 情報
	tsIOData sIOData_reserve; //!< 保存された状態(0がデフォルトとする)
	uint8 u8IOFixState; //!< IOの読み取り確定ビット
	uint32 u32AdcLastTick; //!< 最後に ADC を開始した時刻

	// Counter
	uint32 u32CtTimer0; //!< 64fps カウンタ。スリープ後も維持
	uint16 u16CtTimer0; //!< 64fps カウンタ。起動時に 0 クリアする
	uint16 u16CtRndCt; //!< 起動時の送信タイミングにランダムのブレを作る

	uint8 u8UartReqNum; //!< UART の要求番号

	uint16 u16TxFrame; //!< 送信フレーム数
	uint8 u8SerMsg_RequestNumber; //!< シリアルメッセージの要求番号
} tsAppData;

/****************************************************************************
 * フラッシュ設定情報
 ***************************************************************************/

#define FL_MASTER_u32(c) sAppData.sFlash.sData.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u32(c) sAppData.sConfig_UnSaved.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u32(c) (sAppData.sConfig_UnSaved.u32##c != 0xFFFFFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u16(c) sAppData.sFlash.sData.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u16(c) sAppData.sConfig_UnSaved.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u16(c) (sAppData.sConfig_UnSaved.u16##c != 0xFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u8(c) sAppData.sFlash.sData.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u8(c) sAppData.sConfig_UnSaved.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u8(c) (sAppData.sConfig_UnSaved.u8##c != 0xFF) //!< 構造体要素アクセス用のマクロ @ingroup FLASH

/** @ingroup FLASH
 * フラッシュ設定内容の列挙体
 */
enum {
	E_APPCONF_APPID,     //!<
	E_APPCONF_CHMASK,    //!<
	E_APPCONF_ID,        //!<
	E_APPCONF_ROLE,      //!<
	E_APPCONF_LAYER ,    //!<
	E_APPCONF_SLEEP4,    //!<
	E_APPCONF_SLEEP7,    //!<
	E_APPCONF_FPS,       //!<
	E_APPCONF_PWM_HZ,    //!<
	E_APPCONF_SYS_HZ,    //!<
	E_APPCONF_BAUD_SAFE, //!<
	E_APPCONF_BAUD_PARITY,
	E_APPCONF_TEST
};

/** @ingroup FLASH
 * フラッシュ設定で ROLE に対する要素名の列挙体
 * (未使用、将来のための拡張のための定義)
 */
enum {
	E_APPCONF_ROLE_MAC_NODE = 0,  //!< MAC直接のノード（親子関係は無し）
	E_APPCONF_ROLE_NWK_MASK = 0x10, //!< NETWORKモードマスク
	E_APPCONF_ROLE_PARENT,          //!< NETWORKモードの親
	E_APPCONF_ROLE_ROUTER,        //!< NETWORKモードの子
	E_APPCONF_ROLE_ENDDEVICE,     //!< NETWORKモードの子（未使用、スリープ対応）
	E_APPCONF_ROLE_SILENT = 0x7F, //!< 何もしない（設定のみ)
};

/** サイレントモードの判定マクロ  @ingroup FLASH */
#define IS_APPCONF_ROLE_SILENT_MODE() (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_SILENT)

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
