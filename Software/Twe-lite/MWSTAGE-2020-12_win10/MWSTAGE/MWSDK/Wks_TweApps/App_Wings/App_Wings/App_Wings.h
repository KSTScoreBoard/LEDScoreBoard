/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
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

#include "twecommon.h"
#include "tweserial.h"
#include "tweserial_jen.h"
#include "tweprintf.h"
#include "twesettings.h"
#include "tweutils.h"
#include "twesercmd_gen.h"
#include "tweinteractive.h"
#include "twesysutils.h"

//#include "modbus_ascii.h"

/** @ingroup MASTER
 * 使用する無線チャネル数の最大値 (複数設定すると Channel Agility を利用する)
 */
#define MAX_CHANNELS 3

/** @ingroup MASTER
 * ネットワークのモード列挙体 (ショートアドレス管理か中継ネットワーク層を使用したものか)
 */
typedef enum {
	E_NWKMODE_MAC_DIRECT,//!< ネットワークは構成せず、ショートアドレスでの通信を行う
	E_NWKMODE_LAYERTREE  //!< 中継ネットワークでの通信を行う(これは使用しない)
} teNwkMode;

/** @ingroup MASTER
 * IO 設定要求
 */
typedef struct {
	uint8 u8IOports;          //!< 出力IOの状態 (1=Lo, 0=Hi)
	uint8 u8IOports_use_mask; //!< 設定を行うポートなら TRUE
	uint16 au16PWM_Duty[4];      //!< PWM DUTY 比 (0～1024)
	uint8 au16PWM_use_mask[4];   //!< 設定を行うPWMポートなら TRUE
} tsIOSetReq;

typedef struct{
	uint8 u8color;
	uint8 u8brightness;
	uint8 u8pal;
	uint8 u8edge;
	uint8 u8brinkfreq;
	uint8 u8brinkduty;
	uint8 u8offcycle;
} tsLEDSettings;


/** @ingroup MASTER
 * アプリケーションの情報
 */
typedef struct {
	// ToCoNet
	uint32 u32ToCoNetVersion; //!< ToCoNet のバージョン番号を保持
	uint8 u8AppIdentifier; //!< AppID から自動決定

	// メインアプリケーション処理部
	uint8 u8Hnd_vProcessEvCore; //!< vProcessEvCore のハンドル

	// DEBUG
	uint8 u8DebugLevel; //!< デバッグ出力のレベル

	// 再送の標準的な回数
	uint8 u8StandardTxRetry;

	// Network mode
	teNwkMode eNwkMode; //!< ネットワークモデル(未使用：将来のための拡張用)
	uint8 u8AppLogicalId; //!< ネットワーク時の抽象アドレス 0:親機 1~:子機, 0xFF:通信しない

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk; //!< ネットワークコンテキスト(未使用)
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig; //!< LayerTree の設定情報(未使用)

	// 中継
	uint8 u8max_hops; //!< 最大中継ホップ数 (1-3)

	// 設定
	uint32 u32appid;	//!< アプリケーションID
	uint32 u32chmask;	//!< チャネルマスク
	uint8 u8pow;		//!< 出力(0-3)
	uint8 u8retry;		//!< 再送回数
	uint32 u32opt;		//!< オプションビット
	uint32 u32enckey;	//!< 暗号化キー
	uint8 u8layer;		//!< レイヤ
	uint8 u8sublayer;	//!< サブレイヤ(将来的な実装)
	uint32 u32AddrHigherLayer;	//!< 接続先の上位レイヤ(0だったら動的ルーティング)
	uint32 u32baud;		//!< ボーレート
	uint8 u8parity;		//!< シリアルのパリティなど

	tsLEDSettings sLEDSettings[8];

	// config mode
	uint8 u8Mode; //!< 動作モード(IO M1,M2,M3 から設定される)

	// Counter
	uint32 u32CtTimer0; //!< 64fps カウンタ。スリープ後も維持
	uint16 u16CtTimer0; //!< 64fps カウンタ。起動時に 0 クリアする

	uint8 u8UartReqNum; //!< UART の要求番号

	uint16 u16TxFrame; //!< 送信フレーム数
	uint8 u8SerMsg_RequestNumber; //!< シリアルメッセージの要求番号
} tsAppData;

/**
 * アプリケーションごとの振る舞いを決める関数テーブル
 */
typedef struct _sCbHandler {
	uint8 (*pf_cbToCoNet_u8HwInt)(uint32 u32DeviceId, uint32 u32ItemBitmap);
	void (*pf_cbToCoNet_vHwEvent)(uint32 u32DeviceId, uint32 u32ItemBitmap);
	void (*pf_cbToCoNet_vMain)();
	void (*pf_cbToCoNet_vNwkEvent)(teEvent eEvent, uint32 u32arg);
	void (*pf_cbToCoNet_vRxEvent)(tsRxDataApp *pRx);
	void (*pf_cbToCoNet_vTxEvent)(uint8 u8CbId, uint8 bStatus);
} tsCbHandler;

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

#define E_APPCONF_OPT_PWM_INVERT					0x00000001UL //!< PWMをDUTYを反転する。 @ingroup FLASH
#define E_APPCONF_OPT_DO_INVERT						0x00000002UL //!< DIO出力を反転する @ingroup FLASH
#define E_APPCONF_OPT_ROUTING_HOP2					0x00000010UL //!< 中継段数を２にする。 @ingroup FLASH
#define E_APPCONF_OPT_ROUTING_HOP3					0x00000020UL //!< 中継段数な３にする。 @ingroup FLASH
#define E_APPCONF_OPT_UART_BIN						0x00000100UL //!< バイナリ書式に変更する。 @ingroup FLASH
#define E_APPCONF_OPT_UART_FORCE_SETTINGS			0x00000200UL //!< UARTの設定を強制する @ingroup FLASH
#define E_APPCONF_OPT_REGULAR_PACKET_NO_DISP		0x00000400UL //!< 標準アプリの定期送信パケットを出力しない @ingroup FLASH
#define E_APPCONF_OPT_SECURE						0x00001000UL //!< 暗号化通信を有効にする。 @ingroup FLASH
#define E_APPCONF_OPT_RCV_NOSECURE					0x00002000UL //!< 暗号化通信しているときに平文も受信する。 @ingroup FLASH
#define E_APPCONF_OPT_SHORTTIMEOUT					0x10000000UL //!< 重複チェッカーのタイムアウトを短くする。

#define IS_APPCONF_OPT_PWM_INVERT() (sAppData.u32opt & E_APPCONF_OPT_PWM_INVERT) //!< E_APPCONF_OPT_PWM_INVERT判定 @ingroup FLASH
#define IS_APPCONF_OPT_DO_INVERT() (sAppData.u32opt & E_APPCONF_OPT_DO_INVERT) //!< E_APPCONF_OPT_DO_INVERT判定 @ingroup FLASH
#define IS_APPCONF_OPT_ROUTING_HOP2() (sAppData.u32opt & E_APPCONF_OPT_ROUTING_HOP2) //!< E_APPCONF_OPT_ROUTING_HOP2判定 @ingroup FLASH
#define IS_APPCONF_OPT_ROUTING_HOP3() (sAppData.u32opt & E_APPCONF_OPT_ROUTING_HOP3) //!< E_APPCONF_OPT_ROUTING_HOP3判定 @ingroup FLASH
#define IS_APPCONF_OPT_UART_BIN() (sAppData.u32opt & E_APPCONF_OPT_UART_BIN) //!< E_APPCONF_OPT_UART_BIN判定 @ingroup FLASH
#define IS_APPCONF_OPT_UART_FORCE_SETTINGS() (sAppData.u32opt & E_APPCONF_OPT_UART_FORCE_SETTINGS) //!< E_APPCONF_OPT_UART_FORCE_SETTINGS判定 @ingroup FLASH
#define IS_APPCONF_OPT_REGULAR_PACKET_NO_DISP() (sAppData.u32opt & E_APPCONF_OPT_REGULAR_PACKET_NO_DISP) //!< E_APPCONF_OPT_REGULAR_PACKET_NO_DISP判定 @ingroup FLASH
#define IS_APPCONF_OPT_SECURE() (sAppData.u32opt & E_APPCONF_OPT_SECURE) //!< E_APPCONF_OPT_SECURE判定 @ingroup FLASH
#define IS_APPCONF_OPT_RCV_NOSECURE() (sAppData.u32opt & E_APPCONF_OPT_RCV_NOSECURE) //!< E_APPCONF_OPT_RCV_NOSECURE判定 @ingroup FLASH
#define IS_APPCONF_OPT_SHORTTIMEOUT() (sAppData.u32opt & E_APPCONF_OPT_SHORTTIMEOUT) //!< E_APPCONF_OPT_SHORTTIMEOUT判定 @ingroup FLASH

/** PWM値の反転を考慮した値を設定する */
#define _PWM(c) (IS_APPCONF_OPT_PWM_INVERT() ? (1024-c) : c)

/** DO値をセットする */
#define vDoSetLo(c) (IS_APPCONF_OPT_DO_INVERT() ? vPortSetHi(c) : vPortSetLo(c))
#define vDoSetHi(c) (IS_APPCONF_OPT_DO_INVERT() ? vPortSetLo(c) : vPortSetHi(c))
#define vDoSet_TrueAsLo(c,f) vPortSet_TrueAsLo((c), (IS_APPCONF_OPT_DO_INVERT() ? ((f) == FALSE) : (f)))

#define IS_PARENT() (sAppData.u8layer == 0)
#define IS_ROUTER() (sAppData.u8layer != 0)

void vSerInitMessage();
void vInitAppParent();
void vInitAppRouter();

extern tsCbHandler *psCbHandler;
extern void (* pvProcessSerialCmd)(TWESERCMD_tsSerCmd_Context*);
extern void *pvProcessEv;
extern tsAppData sAppData;

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
