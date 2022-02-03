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

#include "common.h"

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
 * アプリケーションの情報
 */
typedef struct {
	// ToCoNet
	uint32 u32ToCoNetVersion; //!< ToCoNet のバージョン番号を保持
	uint16 u16ToCoNetTickDelta_ms; //!< ToCoNet の Tick 周期 [ms]
	uint8 u8AppIdentifier; //!< AppID から自動決定
	uint8 u8NumChannels; //!< 利用チャネル数

	// DEBUG
	uint8 u8DebugLevel; //!< デバッグ出力のレベル

	// Wakeup
	bool_t bWakeupByButton; //!< TRUE なら起床時に DI 割り込みにより起床した

	// Network mode
	teNwkMode eNwkMode; //!< ネットワークモデル(未使用：将来のための拡張用)
	bool_t bNwkUp; //!< ネットワークが稼働状態かどうかのフラグ
	bool_t bSilent; //!< サイレント状態なら TRUE
	uint8 u8AppLogicalId; //!< ネットワーク時の抽象アドレス 0:親機 1~:子機
	uint8 u8AppLogicalId_Pair; //!< 透過モードの相手先のアドレス
	uint16 u16ShAddr_Pair; //!< 透過モードの相手先のショートアドレス

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk; //!< ネットワークコンテキスト(未使用)
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig; //!< LayerTree の設定情報(未使用)

	// Flash Information
	tsFlash sFlash; //!< フラッシュからの読み込みデータ
	tsFlashApp sConfig_UnSaved; //!< フラッシュへの設定データ (0xFF, 0xFFFFFFFF は未設定)
	int8 bFlashLoaded; //!< フラッシュからの読み込みが正しく行われた場合は TRUE
	int8 bCustomDefaults; //!< ファームウェア末尾にあるデフォルトカスタムの呼び出しに成功したら TRUE

	// config mode
	uint8 u8Mode; //!< 動作モード(IO M1,M2,M3 から設定される)
	uint8 u8ModeEx; //!< 拡張モード設定 (EX1, EX2)

	// UART mode
	uint8 u8uart_mode; //!< UART のモード

	// button manager
	tsBTM_Config sBTM_Config; //!< ボタン入力（連照により状態確定する）管理構造体
	PR_BTM_HANDLER pr_BTM_handler; //!< ボタン入力用のイベントハンドラ (TickTimer 起点で呼び出す)
	uint32 u32BTM_Tick_LastChange; //!< ボタン入力で最後に変化が有ったタイムスタンプ (操作の無効期間を作る様な場合に使用する)
	uint8 u8PortNow; //!< 現在のボタンの状況

	// Counter
	uint32 u32CtTimer0; //!< 64fps カウンタ。スリープ後も維持
	uint16 u16CtTimer0; //!< 64fps カウンタ。起動時に 0 クリアする

	uint8 u8UartReqNum; //!< UART 送信の要求番号を管理
	uint8 u8UartSeqNext; //!< UART 送信中の SEQ# の次の値 (0..127でループする)
} tsAppData;


/**
 * 分割パケットを管理する構造体
 */
typedef struct {
	bool_t bPktStatus[SERCMD_SER_PKTNUM]; //!< パケットの受信フラグ（全部１になれば完了）
	uint8 u8PktNum; //!< 分割パケット数
	uint16 u16DataLen; //!< データ長

	uint8 u8RespID; //!< 応答を返すためのID(外部から指定される値)
	uint8 u8ReqNum; //!< 内部管理の送信ID
	uint8 u8Seq; //!< パケットのシーケンス番号（先頭）

	uint32 u32Tick; //!< タイムスタンプ

	uint8 u8IdSender; //!< 送り元簡易アドレス
	uint8 u8IdReceiver; //!< 宛先簡易アドレス
	uint32 u32SrcAddr; //!< 送り元拡張アドレス
	uint32 u32DstAddr; //!< 宛先拡張アドレス

	bool_t bWaitComplete; //!< 終了フラグ
	bool_t bSleepOnFinish; //!< 終了時にスリープする

	uint8 u8RelayPacket; //!< リピート中継済み回数
} tsSerSeq;

/****************************************************************************
 * モード情報
 ***************************************************************************/
#define UART_MODE_TRANSPARENT 0
#define UART_MODE_ASCII 1
#define UART_MODE_BINARY 2
#define UART_MODE_CHAT 3
#define UART_MODE_CHAT_NO_PROMPT 4

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
 * フラッシュ設定で ROLE に対する要素名の列挙体
 * (未使用、将来のための拡張のための定義)
 */
enum {
	E_APPCONF_ROLE_MAC_NODE = 0,  //!< MAC直接のノード（親子関係は無し）
	E_APPCONF_ROLE_MAC_NODE_REPEATER = 1, //!< リピーター定義
	E_APPCONF_ROLE_MAC_NODE_REPEATER2 = 2 , //!< リピーター定義
	E_APPCONF_ROLE_MAC_NODE_REPEATER3 = 3, //!< リピーター定義
	E_APPCONF_ROLE_MAC_NODE_MAX = 0x0F,
	E_APPCONF_ROLE_NWK_MASK = 0x10, //!< NETWORKモードマスク
	E_APPCONF_ROLE_PARENT,          //!< NETWORKモードの親
	E_APPCONF_ROLE_ROUTER,        //!< NETWORKモードの子
	E_APPCONF_ROLE_ENDDEVICE,     //!< NETWORKモードの子（スリープ対応）
	E_APPCONF_ROLE_SILENT_MASK = 0x80, //!< 無線を動作させないモード
};

/** ROLE 設定のサイレントモードの判定マクロ  @ingroup FLASH */
#define IS_APPCONF_ROLE_SILENT_MODE() (sAppData.sFlash.sData.u8role & E_APPCONF_ROLE_SILENT_MASK)

/** ROLE の主設定部分を取り出す */
#define APPCONF_ROLE() (sAppData.sFlash.sData.u8role & 0x7F)

/** ROLE 設定のリピーターの判定マクロ @ingroup FLASH */
#define IS_REPEATER() (APPCONF_ROLE()  >= E_APPCONF_ROLE_MAC_NODE_REPEATER && APPCONF_ROLE() <= E_APPCONF_ROLE_MAC_NODE_REPEATER3)

/** リピートの最大回数 @ingroup FLASH */
#define REPEATER_MAX_COUNT() (APPCONF_ROLE())

/** 専業リピーターの判定 @ingroup FLASH */
#define IS_DEDICATED_REPEATER() (IS_REPEATER() && IS_LOGICAL_ID_REPEATER(sAppData.u8AppLogicalId))
/** 子機兼リピーターの判定 @ingroup FLASH */
#define IS_CHILD_REPEATER() (IS_REPEATER() && IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId))


/** AES 利用のマクロ判定  @ingroup FLASH */
#define IS_CRYPT_MODE() (sAppData.sFlash.sData.u8Crypt)

/** コールバックID の設定 */
#define CBID_MASK_BASE 0x3F
/** コールバックID の設定 */
#define CBID_MASK_SPLIT_PKTS 0x40
/** コールバックID の設定 */
#define CBID_MASK_SILENT 0x80

/**  フラッシュ設定パリティ設定（マスク）  @ingroup FLASH */
#define APPCONF_UART_CONF_PARITY_MASK 0x3
#define APPCONF_UART_CONF_STOPBIT_MASK 0x4
#define APPCONF_UART_CONF_WORDLEN_MASK 0x8

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
