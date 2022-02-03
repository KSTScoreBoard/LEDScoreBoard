/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "Interactive.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	//static bool_t bWaiting = FALSE;

	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {

			// 起動時の待ち処理
			V_PRINTF( LB LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");

			// RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);

			// RUNNING状態へ遷移
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		} else {
			// 開始する
			// start up message
			// 起床メッセージ
			vSerInitMessage();
			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);
			V_FLUSH();

			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			// ネットワークの初期化
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);

			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			}

			// RUNNING状態へ遷移
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bTimeout = FALSE;
	if (eEvent == E_EVENT_NEW_STATE) {
		bTimeout = FALSE;
		// ADC の開始
		//vADC_WaitInit();
		sAppData.sObjADC.u8SourceMask = TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2 | TEH_ADC_SRC_ADC_3 | TEH_ADC_SRC_ADC_4;
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}


	if( ToCoNet_Event_u32TickFrNewState(pEv) > 100 && bTimeout == FALSE ){
		bTimeout = TRUE;
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}

	if( ToCoNet_Event_u32TickFrNewState(pEv) > 200 ){
		V_PRINTF(LB"TIMEOUT[E_STATE_RUNNING]");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
	}

	if (eEvent == E_ORDER_KICK) {
		bool_t bOk = FALSE;
		uint8 au8Data[64];
		uint8 *q =  au8Data;

		if( sPALData.u8EEPROMStatus != 0 ){
			S_OCTET(7);		// データ数
			S_OCTET(0x32);
			S_OCTET(0x00);
			S_OCTET( sPALData.u8EEPROMStatus );
		}else{
			S_OCTET(6);		// データ数
		}

		uint8 u8DIO = bPortRead(12) ? 0x01:0x00;
		u8DIO |= bPortRead(13) ? 0x02:0x00;
		u8DIO |= bPortRead(11) ? 0x04:0x00;
		u8DIO |= bPortRead(16) ? 0x08:0x00;

		S_OCTET(0x30);					// 電源電圧
		S_OCTET(0x01);
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// AI1(ADC1)
		S_OCTET(0x02);
		S_BE_WORD(sAppData.u16Adc[0]);

		S_OCTET(0x30);					// AI2(ADC3)
		S_OCTET(0x03);
		S_BE_WORD(sAppData.u16Adc[2]);

		S_OCTET(0x30);					// AI3(ADC2)
		S_OCTET(0x04);
		S_BE_WORD(sAppData.u16Adc[1]);

		S_OCTET(0x30);					// AI4(ADC4)
		S_OCTET(0x05);
		S_BE_WORD(sAppData.u16Adc[3]);

		S_OCTET(0x31);					// DIO
		S_OCTET(4);						// Port数
		S_OCTET(u8DIO);

		bOk = bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );

		sAppData.u16frame_count++;

		if ( bOk ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
			V_PRINTF(LB"TxOk");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
		} else {
			V_PRINTF(LB"TxFl");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"Sleeping...");
		V_FLUSH();

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		ToCoNet_Nwk_bPause(sAppData.pContextNwk);

		// print message.
		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

		// stop interrupt source, if interrupt source is still running.
		;

		// set UART Rx port as interrupt source
		vAHI_DioSetDirection(PORT_INPUT_MASK, 0); // set as input

		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
		 vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）

		// 周期スリープに入る
		vSleep(sAppData.sFlash.sData.u32Slp*1000, sAppData.u16frame_count == 1 ? FALSE : TRUE, FALSE);
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_SLEEP),
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
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			// 全チャネルの処理が終わったら、次の処理を呼び起こす
			vStoreSensorValue();
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
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
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
}
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
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppNOC() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}


/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.u16Adc[0] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.u16Adc[1] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2];
	sAppData.u16Adc[2] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_3];
	sAppData.u16Adc[3] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_4];
	sAppData.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);
	V_PRINTF( LB"P=%d,1=%d,2=%d,3=%d,4=%d", 
				sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT],
				sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1],
				sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2],
				sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_3],
				sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_4]
				);
}
