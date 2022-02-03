/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"
#include "ccitt8.h"
#include "Interactive.h"
#include "EndDevice.h"
#include "sensor_driver.h"
#include "MC3630.h"
#include "accel_event.h"

#include "flash.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vProcessAccel_Event(teEvent eEvent);

#define ABS(c) (c<0?(-1*c):c)

static uint8 u8sns_cmplt = 0;
static uint8 u8analyze_block = 4;		// 0~4まで
static tsSnsObj sSnsObj;
extern tsObjData_MC3630 sObjMC3630;

enum {
	E_SNS_MC3630_CMP = 1,
	E_SNS_ALL_CMP = 1
};

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bFirst = TRUE;
	if (eEvent == E_EVENT_START_UP) {
		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;
		vMC3630_Init( &sObjMC3630, &sSnsObj );

		if( bFirst ){
			bFirst = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_SNS_INIT);
		}else{
			if( !sAppData.bWakeupByButton ){
				//V_PRINTF(LB "*** Wake by Timer. No Action...");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF); // スリープ待ち状態
			}else{
				vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
				if (bSnsObj_isComplete(&sSnsObj)) {
					// 即座に完了した時はセンサーが接続されていない、通信エラー等
					u8sns_cmplt |= E_SNS_MC3630_CMP;
					V_PRINTF(LB "*** MC3630 comm err?");
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF); // スリープ待ち状態
					return;
				}
				sObjMC3630.u8Interrupt = u8MC3630_ReadInterrupt();

				// センサ値を読み込んで送信する
				// RUNNING 状態
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}
		}
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_SNS_INIT, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		V_PRINTF(LB "*** MC3630 Setting...");

		sObjMC3630.u8SampleFreq = MC3630_SAMPLING100HZ;
		bool_t bOk = bMC3630_reset( sObjMC3630.u8SampleFreq, MC3630_RANGE16G, 30 );
		if(bOk == FALSE){
			V_PRINTF(LB "Access failed.");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF); // スリープ状態へ遷移
			return;
		}else{
			vMC3630_StartSNIFF( 2, 1 );
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF); // スリープ状態へ遷移
		}
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint8 u8count = 0;

		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessAccel_Event(E_EVENT_START_UP);
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		V_PRINTF(LB"count = %d", u8count);
		if( u8count == 0 ){
			vMC3630_StartFIFO();
			vAccelEvent_Init( 100 );
			bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );

			u8count++;
			sObjMC3630.u8Event = ACCEL_EVENT_NOTANALYZED;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF);
		}else if( u8count == u8analyze_block-1 ){
			vMC3630_StartSNIFF(2, 1);

			bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );
			tsAccelEventData sAccelEvent = tsAccelEvent_GetEvent();
			sObjMC3630.u8Event = sAccelEvent.eEvent;
			V_PRINTF( LB"Event = %s", (sAccelEvent.eEvent == 1) ? "Move":((sAccelEvent.eEvent == 2)?"Tap":"None") );
			V_PRINTF( LB"length = %d", sAccelEvent.u8PeakLength  );

			u8count = 0;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF);
		}else{
			bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );
			u8count++;
			sObjMC3630.u8Event = ACCEL_EVENT_NOTANALYZED;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF);
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING2)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_POWEROFF, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		V_PRINTF(LB"! End...");
		V_FLUSH();

		sObjMC3630.bFinished = TRUE;

		vMC3630_ClearInterrupReg();
		pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持しない
		ToCoNet_Event_SetState(pEv, E_STATE_IDLE); // スリープ状態へ遷移
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_SNS_INIT),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_POWEROFF),
	PRSEV_HANDLER_TBL_TRM
};

/**
 * イベント処理関数
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	ToCoNet_Event_StateExec(asStateFuncTbl, pEv, eEvent, u32evarg);
}

#if 0
/**
 * ハードウェア割り込み
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
static uint8 cbAppToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	default:
		break;
	}

	return u8handled;
}
#endif

/**
 * ハードウェアイベント（遅延実行）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		vProcessAccel_Event(E_EVENT_TICK_TIMER);
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	default:
		break;
	}
}

#if 0
/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}
#endif

#if 0
/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
static void cbAppToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	default:
		break;
	}
}
#endif


#if 0
/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {

}
#endif

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
#if 0
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	V_PRINTF(LB"! Tx Cmp = %d", bStatus);
	ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
}
#endif

/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, // cbAppToCoNet_vMain,
	NULL, // cbAppToCoNet_vNwkEvent,
	NULL, // cbAppToCoNet_vRxEvent,
	NULL, //cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppAccelEvent() {
	psCbHandler_Sub = &sCbHandler;
	pvProcessEv_Sub = vProcessEvCore;
}

static void vProcessAccel_Event(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_MC3630_CMP;


		V_PRINTF( LB"!NUM = %d", sObjMC3630.u8FIFOSample );
		V_PRINTF(LB"!MC3630_%d: X : %d, Y : %d, Z : %d",
				0,
				sObjMC3630.ai16Result[MC3630_X][0],
				sObjMC3630.ai16Result[MC3630_Y][0],
				sObjMC3630.ai16Result[MC3630_Z][0]
		);

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}