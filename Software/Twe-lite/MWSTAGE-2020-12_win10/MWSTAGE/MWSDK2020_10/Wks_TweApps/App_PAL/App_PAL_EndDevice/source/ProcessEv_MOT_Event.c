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
static void vStoreSensorValue();
static bool_t bSendData( int16 ai16accel[3][32], uint8 u8startAddr, uint8 u8Sample );

#define ABS(c) (c<0?(-1*c):c)

static uint8 u8sns_cmplt = 0;
tsObjData_MC3630 sObjMC3630;

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_ALL_CMP = 1
};

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bFirst = TRUE;
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting Event");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		if( bFirst ){
			bFirst = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED);
		}else{
			if( !sAppData.bWakeupByButton ){
				V_PRINTF(LB "*** Wake by Timer. Sleep...");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			}else{
				// ADC の取得
				vADC_WaitInit();
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

				// センサ値を読み込んで送信する
				// RUNNING 状態
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}
		}

	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/* 電源投入時はLEDを点滅させる */
PRSEV_HANDLER_DEF(E_STATE_APP_BLINK_LED, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		V_PRINTF(LB "*** Blink LED ");
		sAppData.u8LedState = 0x02;
	}

	// タイムアウトの場合はスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 750 && sObjMC3630.bFinished) {
		LED_OFF();
		sAppData.u8LedState = 0x00;
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP && sObjMC3630.bFinished) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint8 count = 0;
	if (eEvent == E_EVENT_NEW_STATE) {
		// ネットワークの初期化
		if (!sAppData.pContextNwk) {
			// 初回のみ
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
				return;
			}
		} else {
			// 一度初期化したら RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);
		}

		if( sObjMC3630.u8Event != ACCEL_EVENT_NOTANALYZED ){
			if( sObjMC3630.u8FIFOSample > 16 ){
				count = 2;
				if ( bSendData( sObjMC3630.ai16Result, 0, 16 ) ) {
					ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
				} else {
					//ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
				}
				V_PRINTF(" FR=%04X", sAppData.u16frame_count);
				if ( bSendData( sObjMC3630.ai16Result, 16, sObjMC3630.u8FIFOSample-16 ) ) {
					ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
				} else {
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
				}
					V_PRINTF(" FR=%04X", sAppData.u16frame_count);
			}else{
				count = 1;
				if ( bSendData( sObjMC3630.ai16Result, 0, sObjMC3630.u8FIFOSample ) ) {
					ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
				} else {
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
				}
				V_PRINTF(" FR=%04X", sAppData.u16frame_count);
			}
		}
//	}else{
//		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		count--;
		if( count == 0) ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		if (sObjMC3630.bFinished) {
			V_PRINTF(LB"! Sleeping...");
			V_FLUSH();

			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bPause(sAppData.pContextNwk);
			}

			vMC3630_ClearInterrupReg();

			pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持しない

			vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

			(void)u32AHI_DioInterruptStatus(); // clear interrupt register
			vAHI_DioWakeEnable( u32DioPortWakeUp, 0); // ENABLE DIO WAKE SOURCE
			vAHI_DioWakeEdge(0, u32DioPortWakeUp ); // 割り込みエッジ(立上がりに設定)

			// 周期スリープに入る
			vSleep(10000, FALSE, FALSE);
		}
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_BLINK_LED),
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
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			u8sns_cmplt |= E_SNS_ADC_CMP_MASK;
			vStoreSensorValue();
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
	V_PRINTF(LB"! Tx Cmp = %d", bStatus);
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
void vInitAppMOT_Event() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;

	extern void vInitAppAccelEvent();
	vInitAppAccelEvent();
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.u16Adc[0] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);
}

static bool_t bSendData( int16 ai16accel[3][32], uint8 u8startAddr, uint8 u8Sample )
{
		uint8 i;
		uint8	au8Data[92];
		uint8*	q = au8Data;

		if( sPALData.u8EEPROMStatus != 0 ){
			S_OCTET(3+u8Sample);		// データ数
			S_OCTET(0x32);
			S_OCTET(0x00);
			S_OCTET( sPALData.u8EEPROMStatus );
		}else{
			S_OCTET(2+u8Sample);		// データ数
		}

		S_OCTET(0x30);					// 電源電圧
		S_OCTET(0x01)
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// AI1(ADC1)
		S_OCTET(0x02)
		S_BE_WORD(sAppData.u16Adc[0]);


		S_OCTET(0x04);					// Acceleration
		S_OCTET( sObjMC3630.u8Interrupt );		// 拡張用1バイト
		S_OCTET( u8Sample );				// サンプル数
		S_OCTET( sObjMC3630.u8SampleFreq-MC3630_SAMPLING25HZ );		// サンプリング周波数
		S_OCTET( 12 );			// 送信するデータの分解能

		V_PRINTF( LB"sample = %d", u8Sample );
		V_PRINTF( LB"freq = %d", sObjMC3630.u8SampleFreq-MC3630_SAMPLING25HZ );


		uint16 u16x[2], u16y[2], u16z[2];
		for( i=u8startAddr; i<u8Sample+u8startAddr; i+=2 ){
			u16x[0] = (ai16accel[MC3630_X][i]>>3)&0x0FFF;
			u16x[1] = (ai16accel[MC3630_X][i+1]>>3)&0x0FFF;
			u16y[0] = (ai16accel[MC3630_Y][i]>>3)&0x0FFF;
			u16y[1] = (ai16accel[MC3630_Y][i+1]>>3)&0x0FFF;
			u16z[0] = (ai16accel[MC3630_Z][i]>>3)&0x0FFF;
			u16z[1] = (ai16accel[MC3630_Z][i+1]>>3)&0x0FFF;

			// 12bitに変換する
			S_OCTET((u16x[0]&0x0FF0)>>4);
			S_OCTET( ((u16x[0]&0x0F)<<4) | ((u16y[0]&0x0F00)>>8) );
			S_OCTET(u16y[0]&0x00FF);
			S_OCTET((u16z[0]&0x0FF0)>>4);
			S_OCTET( ((u16z[0]&0x0F)<<4) | ((u16x[1]&0x0F00)>>8) );
			S_OCTET(u16x[1]&0x00FF);
			S_OCTET((u16y[1]&0x0FF0)>>4);
			S_OCTET( ((u16y[1]&0x0F)<<4) | ((u16z[1]&0x0F00)>>8) );
			S_OCTET(u16z[1]&0x00FF);
		}

		sAppData.u16frame_count++;

		return bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
}