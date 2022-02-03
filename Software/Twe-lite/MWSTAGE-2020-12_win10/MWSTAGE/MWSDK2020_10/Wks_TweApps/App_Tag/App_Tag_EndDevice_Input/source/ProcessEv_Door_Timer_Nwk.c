/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

/**
 * FIREイベントを受けとたら、動作開始
 * @param E_STATE_IDLE
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);
		} else {
			V_PRINTF(LB "* N/W start request");
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			// ネットワークの初期化
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);

			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
				sAppData.u8NwkStat = E_IO_TIMER_NWK_NWK_START; // mininode の初期化は関数終了をもって完了する
			}
		}
	}

	if (sAppData.u8NwkStat == E_IO_TIMER_NWK_FIRE_REQUEST) { // FIRE
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}
}

/**
 * 送信要求と送信完了待ちを行う
 * - 完了待ちは、TICKTIMER 毎に本関数が呼び出され、フラグがセットされたら完了とする
 *
 * @param E_STATE_APP_WAIT_TX
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		V_PRINTF(LB"** RF transmit request - ");

		uint8 au8Data[5];
		uint8 *q =  au8Data;

		S_OCTET(sAppData.bDI1_Now_Opened); // 開閉状態
		S_BE_DWORD(sAppData.u32DI1_Dur_Opened_ms); // 連続開時間

		sAppData.u16frame_count++;

		if( bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data ) ){
			V_PRINTF(LB "Tx Start...");
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		}else{
			sAppData.u8NwkStat = E_IO_TIMER_NWK_COMPLETE_TX_FAIL;
			V_PRINTF(LB "Tx Fail.");
		}

//		if (ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx)) {
//			V_PRINTF(LB "Tx Start...");
			//extern void ToCoNet_Tx_vProcessQueue();
			//ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
//		} else {
//			V_PRINTF(LB "Tx Fail.");
//			sAppData.u8NwkStat = E_IO_TIMER_NWK_COMPLETE_TX_FAIL;
//		}

		V_PRINTF(" FR=%04X ", sAppData.u16frame_count);
	}

	// 送信完了
	if (sAppData.u8NwkStat & E_IO_TIMER_NWK_COMPLETE_MASK) {
		V_PRINTF(LB" %s", sAppData.u8NwkStat == E_IO_TIMER_NWK_COMPLETE_TX_SUCCESS ? "Complete" : "Fail");

		ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		ToCoNet_Event_SetState(pEv, E_STATE_IDLE);
	}
}


/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
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

/**
 * 初期化２
 */
void vInitAppDoorTimerSub() {
	pvProcessEv2 = vProcessEvCore;
}
