/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"
#include "Interactive.h"
#include "config.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif


#define HALLIC ((1UL<<SNS_EN)|(1UL<<INPUT_PC))
#define Is_IntWakeUp() ( sAppData.u32WakeupDIOStatus&HALLIC && sAppData.u32DIO_startup&HALLIC )

#define BUF_SIZE 16

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
uint64 u64TimeSub( uint8 minuend, uint8 subtrahend );
bool_t bTimeCount();

static uint64 u64Time = 0ULL;
static uint8 u8CountMax = 5;
static uint16 WAIT_MIN = 150;
static uint16 WAIT_MAX = 750;
static uint64 u64TimeMax = 0xFFFFFFFFFFFFFFFFULL;

static uint64 au64TimeBuf[BUF_SIZE];
static uint8 u8BufIndex = 0;

PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		if( sAppData.sFlash.sData.u16RcClock == 0 ){
			V_PRINTF(LB"! Calib Start." );
			sAppData.sFlash.sData.u16RcClock = ToCoNet_u16RcCalib(0);
			u64TimeMax =  (0x1FFFFFFFFFFULL >> 5) * (uint64)sAppData.sFlash.sData.u16RcClock / 10000ULL;
			//V_FLUSH();
		}

		if( Is_IntWakeUp() ){
			V_PRINTF(LB LB"! Count Start." );
			V_FLUSH();
			ToCoNet_Event_SetState(pEv, E_STATE_OTA_COUNT);
		}
	}
}

PRSEV_HANDLER_DEF(E_STATE_OTA_COUNT, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint8 u8Countnum = 0;
	if( eEvent == E_EVENT_NEW_STATE ){
		au64TimeBuf[u8BufIndex] = u64GetTimer_ms();
		if( u8Countnum <= u8CountMax-1 ){
			u8Countnum++;
		}
		V_PRINTF(LB"! Index:%2d, Time:%d", u8BufIndex, (uint32)au64TimeBuf[u8BufIndex] );
		V_FLUSH();

		if( u8Countnum >= u8CountMax ){
			if(bTimeCount()){
				ToCoNet_Event_SetState(pEv, E_STATE_OTA_SUCCESS);
				return;
			}
		}
		u8BufIndex++;
		if( u8BufIndex >= BUF_SIZE ) u8BufIndex = 0;
	}

}

PRSEV_HANDLER_DEF(E_STATE_OTA_SUCCESS, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if( eEvent == E_EVENT_NEW_STATE ){
		V_PRINTF(LB"! Checked OTA Command. %d", (uint32)u64Time);
		V_PRINTF(LB"! Reset this system...");
		V_FLUSH();

		vAHI_SwReset();
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_OTA_COUNT),
	PRSEV_HANDLER_TBL_DEF(E_STATE_OTA_SUCCESS),
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
#if 0
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}
#endif

/**
 * ハードイベント（遅延割り込み処理）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
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
	NULL, //cbAppToCoNet_vMain,
	NULL, //cbAppToCoNet_vNwkEvent,
	NULL, //cbAppToCoNet_vRxEvent,
	NULL  //cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppOTA() {
	psCbHandler_OTA = &sCbHandler;
	pvProcessEv_OTA = vProcessEvCore;
}

void vInitOTAParam( uint8 u8CountNum, uint16 u16TimeOutMs_min, uint16 u16TimeOutMs_max )
{
	if( u8CountNum > 0 ){
		u8CountMax = u8CountNum;
	}
	if( u16TimeOutMs_min > 0 && u16TimeOutMs_max > u16TimeOutMs_min ){
		WAIT_MAX = u16TimeOutMs_max;
		WAIT_MIN = u16TimeOutMs_min;
	}

	A_PRINTF(LB"Set OTA PARAM %d, %d, %d", u8CountMax, WAIT_MIN, WAIT_MAX);
}

uint64 u64TimeSub( uint8 minuend, uint8 subtrahend )
{
	uint64 diff;
	if( au64TimeBuf[minuend] < au64TimeBuf[subtrahend] ){
		diff = u64TimeMax + au64TimeBuf[minuend] - au64TimeBuf[subtrahend];
	}else{
		diff = au64TimeBuf[minuend] - au64TimeBuf[subtrahend];
	}

	return diff;
}

bool_t bTimeCount()
{
	uint8 u8CountWhole = 1;
	uint8 i;

	// 差がWAIT_MAX*(u8CountMax-1)以下の個数を数える 
	for( i=1; i<BUF_SIZE; i++  ){
		uint8 u8BeforeIndex;
		if( i > u8BufIndex ){
			u8BeforeIndex = BUF_SIZE + u8BufIndex - i;
		}else{
			u8BeforeIndex = u8BufIndex - i;
		}

		uint64 sub = u64TimeSub(u8BufIndex, u8BeforeIndex);
		V_PRINTF(LB"Whole %2d,%2d %d", u8BufIndex, u8BeforeIndex, (uint32)sub);

		if( sub < WAIT_MAX*( u8CountMax-1 ) ){
			u8CountWhole++;
		}else{
			break;
		}
	}

	V_PRINTF(LB"! COUNT Whole:%d", u8CountWhole);
	V_FLUSH();

	// 下限時間の評価を行う
	uint8 u8Count = 1;
	if( u8CountWhole >= u8CountMax ){
		uint8 u8IndexNow = u8BufIndex;
		for( i=1; i<u8CountWhole; i++  ){
			uint8 u8BeforeIndex;
			if( i > u8BufIndex ){
				u8BeforeIndex = BUF_SIZE + u8BufIndex - i;
			}else{
				u8BeforeIndex = u8BufIndex - i;
			}

			uint64 diff = u64TimeSub( u8IndexNow, u8BeforeIndex );
			V_PRINTF(LB"Min %2d,%2d %d", u8IndexNow, u8BeforeIndex, (uint32)diff );
			V_FLUSH();
			if( diff >= WAIT_MIN ){
				u8Count++;
				if( u8Count >= u8CountMax ){
					return TRUE;
				}else{
					u8IndexNow = u8BeforeIndex;
				}
			}

		}
		V_PRINTF(LB"! After COUNT:%d", u8Count);
		V_FLUSH();
	}

	return FALSE;
}