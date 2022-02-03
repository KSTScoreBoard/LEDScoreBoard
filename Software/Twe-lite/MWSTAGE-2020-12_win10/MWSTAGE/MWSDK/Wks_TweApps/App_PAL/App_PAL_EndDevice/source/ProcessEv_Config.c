/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// 詳細は remote_config.h を参照

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "config.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

/* *********************************/
/* * 状態マシン                    */
/* *********************************/

/*
 * 設定処理用の状態マシン
 *
 * 設定マスター：
 *   - 起床後は、デバイス側からの要求待ちを行う。
 *
 * 設定対象：
 *   - 起床後 100ms は受信回路をオープン。設定要求リクエストを行う。
 */

/**
 * アイドル状態。
 *
 * @param E_STATE_IDLE
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		Interactive_vSetMode(TRUE,0);
		vSerInitMessage();
		vfPrintf(&sSerStream, LB LB "*** Entering Config Mode ***");

		sAppData.sFlash.sData.u16RcClock = ToCoNet_u16RcCalib(0);
	}

	if (ToCoNet_Event_u32TickFrNewState(pEv) > 1000) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_INTERACTIVE);
	}
}

/**
 * 通常のインタラクティブモード
 *
 * @param E_STATE_APP_INTREACTIVE
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */

PRSEV_HANDLER_DEF(E_STATE_APP_INTERACTIVE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		sAppData.u8LedState = 0;		// LED消灯
		Config_vUpdateScreen();
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_INTERACTIVE),
	PRSEV_HANDLER_TBL_TRM
};


/**
 * イベント処理関数
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	ToCoNet_Event_StateExec(asStateFuncTbl, pEv, eEvent, u32evarg);
}

/* *********************************/
/* * ToCoNet コールバック          */
/* *********************************/

/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}

/**
 * ハードイベント（遅延割り込み処理）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bLed = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		// LED の点滅を管理する
		if (sAppData.u8LedState == 1 || ((u32TickCount_ms >> 8) & 1)) {
			bLed = TRUE;
		}
		vPortSet_TrueAsLo(OUTPUT_LED, bLed);

		// WDTの制御
		if( (u32TickCount_ms&0x0000FFFF) == 0x8000 ){
			vPortSetHi(WDT_OUT);
		}else{
			vPortSetLo(WDT_OUT);
		}

		break;

	default:
		break;
	}
}

/**
 * 受信イベント
 * @param pRx
 */
#if 0
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {
	return;
}
#endif

/**
 * 送信完了イベント
 * - 送信失敗時は次の手続きはないので無線受信回路をクローズする
 *
 * @param u8CbId
 * @param bStatus
 */
#if 0
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	return;
}
#endif

/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	cbAppToCoNet_vMain,
	NULL, //cbAppToCoNet_vNwkEvent,
	NULL, //cbAppToCoNet_vRxEvent,
	NULL  //cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppConfig() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}