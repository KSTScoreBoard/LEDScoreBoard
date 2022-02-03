/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#ifndef  MASTER_H_INCLUDED
#define  MASTER_H_INCLUDED

#ifndef ENDDEVICE
#define ENDDEVICE
#endif

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include <jendefs.h>


/// TOCONET 関連の定義
#define TOCONET_DEBUG_LEVEL 0

// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_NWK_LAYERTREE_MININODES

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"
#include "appdata.h"
#include "sercmd_gen.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/**
 * 無線状態管理用の状態定義
 */
typedef enum {
	E_IO_TIMER_NWK_IDLE = 0,                  //!< E_IO_TIMER_NWK_IDLE
	E_IO_TIMER_NWK_FIRE_REQUEST,              //!< E_IO_TIMER_NWK_FIRE_REQUEST
	E_IO_TIMER_NWK_NWK_START,                 //!< E_IO_TIMER_NWK_NWK_START
	E_IO_TIMER_NWK_COMPLETE_MASK = 0x10,       //!< E_IO_TIMER_NWK_COMPLETE_MASK
	E_IO_TIMER_NWK_COMPLETE_TX_SUCCESS  = 0x11,//!< E_IO_TIMER_NWK_COMPLETE_TX_SUCCESS
	E_IO_TIMER_NWK_COMPLETE_TX_FAIL  = 0x12,   //!< E_IO_TIMER_NWK_COMPLETE_TX_FAIL
} teIOTimerNwk;

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

/**
 * 接続したPALのデータを保持する
 */
typedef struct {
	uint8 u8EEPROMStatus;		// EEPROMのステータス
	uint8 u8FormatVersion;		// データ書式のバージョン
	uint8 u8PALModel;			// 接続したPALのモデル
	uint8 u8PALVersion;			// 接続したPALの基板バージョン
	uint32 u32SerialID;			// シリアルID
	uint8 u8OptionLength;		// オプションの長さ
	uint8 au8Option[64];		// オプション
} tsPALData;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vInitAppNOC();
void vInitAppMAG();
void vInitAppENV();
void vInitAppMOT();
void vInitAppMOT_Event();
void vInitAppLED();
void vInitAppConfig();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
extern tsPALData sPALData;
extern tsFILE sSerStream;

extern tsCbHandler *psCbHandler;
extern tsCbHandler *psCbHandler_Sub;
extern void *pvProcessEv;
extern void *pvProcessEv_Sub;
extern void (*pf_cbProcessSerialCmd)(tsSerCmd_Context *);

#if defined __cplusplus
}
#endif

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
