/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#ifndef  MASTER_H_INCLUDED
#define  MASTER_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

#ifndef ENDDEVICE_INPUT
#define ENDDEVICE_INPUT
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include <jendefs.h>

/// TOCONET 関連の定義
#define TOCONET_DEBUG_LEVEL 0

// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_NWK_LAYERTREE_MININODES
//#define ToCoNet_USE_MOD_RAND_MT
//#define ToCoNet_USE_MOD_RAND_XOR_SHIFT

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

#include "appdata.h"

#include "sercmd_gen.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
/**
 * スイッチ閉⇒開検出後に、周期的にチェックする間隔
 */
static const uint16 u16_IO_Timer_mini_sleep_dur = 1000;

/**
 * ブザーを鳴らす時間
 */
static const uint16 u16_IO_Timer_buzz_dur = 100;

/**
 * ボタンモード向け定義
 */
#define E_INVERSE_INT 0x0001
#define E_DBLEDGE_INT 0x0002
#define E_SWING_MODE  0x0004
#define E_MULTI_INPUT 0x0008
#define E_ENABLE_WDT  0x0100
#define E_INPUT_TIMER 0x0200
#define E_TIMER_MODE  0x0400

#define IS_INVERSE_INT() ((sAppData.sFlash.sData.i16param & E_INVERSE_INT) != 0)
#define IS_DBLEDGE_INT() ((sAppData.sFlash.sData.i16param & E_DBLEDGE_INT) != 0)
#define IS_SWING_MODE()  ((sAppData.sFlash.sData.i16param & E_SWING_MODE) != 0)
#define IS_MULTI_INPUT() ((sAppData.sFlash.sData.i16param & E_MULTI_INPUT) != 0)
#define IS_ENABLE_WDT() ((sAppData.sFlash.sData.i16param & E_ENABLE_WDT) != 0)
#define IS_INPUT_TIMER() ((sAppData.sFlash.sData.i16param & E_INPUT_TIMER) != 0)
#define IS_TIMER_MODE() ((sAppData.sFlash.sData.i16param & E_TIMER_MODE) != 0)

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

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
extern void *pvProcessEv1, *pvProcessEv2;
extern void (*pf_cbProcessSerialCmd)(tsSerCmd_Context *);

void vInitAppStandard();
void vInitAppButton();
void vInitAppSHT21();
void vInitAppSHT31();
void vInitAppSHTC3();
void vInitAppBME280();
void vInitAppS1105902();
void vInitAppADT7410();
void vInitAppMPL115A2();
void vInitAppLIS3DH();
void vInitAppL3GD20();
void vInitAppADXL345();
void vInitAppADXL345_LowEnergy();
void vInitAppADXL345_AirVolume();
void vInitAppADXL345_FIFO();
void vInitAppTSL2561();
void vInitAppMAX31855();
void vInitAppDoorTimer();
void vInitAppUart();
void vInitAppConfig();
void vInitAppConfigMaster();
void vInitAppMultiSensor();

void vPortSetSns(bool_t bActive);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
extern tsFILE sSerStream;
extern tsCbHandler *psCbHandler;
extern uint8 u8ConfPort;

extern uint32 u32InputMask;
extern uint32 u32InputSubMask;

#if defined __cplusplus
}
#endif

extern uint8 u8ADCPort[4];

#endif  /* MASTER_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
