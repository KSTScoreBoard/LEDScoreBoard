/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "sensor_driver.h"
#include "SHTC3.h"
#include "LTR308ALS.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessENV(teEvent eEvent);
static uint8 u8sns_cmplt = 0;

static tsSnsObj sSnsObj[2];
static tsObjData_SHTC3 sObjSHTC3;
static tsObjData_LTR308ALS sObjLTR308ALS;

static uint8 au8ErrCount[3];
static bool_t abErr[2];

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_SHTC3_CMP = 2,
	E_SNS_LTR308ALS_CMP = 4,
	E_SNS_ALL_CMP = 7
};

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);
		}
		V_FLUSH();

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// 定期送信するときはカウントが0の時と割り込み起床したときだけ送信する
		if( sAppData.u32SleepCount != 0 && sAppData.bWakeupByButton == FALSE ){
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			return;
		}

		// 連続10回読み込みエラーが起こってたらリセットする。
		int8 i=0;
		for( i=0;i<2;i++ ){
			if( au8ErrCount[i] >= 10 ){
				vAHI_SwReset();
			}
		}

		

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		vLTR308ALS_Init(&sObjLTR308ALS, &sSnsObj[1]);
		vSnsObj_Process(&sSnsObj[1], E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj[1])) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_LTR308ALS_CMP;
			V_PRINTF(LB "*** LTR308ALS comm err?");
			V_FLUSH();
			abErr[1] = TRUE;
			//ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}else{
			abErr[1] = FALSE;
		}

		// SHTC3
		vSHTC3_Init(&sObjSHTC3, &sSnsObj[0]);
		vSnsObj_Process(&sSnsObj[0], E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj[0])) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_SHTC3_CMP;
			V_PRINTF(LB "*** SHTC3 comm err?");
			abErr[0] = TRUE;
			//ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}else{
			abErr[0] = FALSE;
		}

		// ADC の取得
		//vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

		// RUNNING 状態
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bTimeout = FALSE;
	if (eEvent == E_EVENT_NEW_STATE) {
		bTimeout = FALSE;
	}

	// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		vProcessENV(E_EVENT_START_UP);
	}

	// ２回スリープすると完了
	if (u8sns_cmplt != E_SNS_ALL_CMP && (u8sns_cmplt & E_SNS_ADC_CMP_MASK)) {
		// ADC 完了後、この状態が来たらスリープする
		pEv->bKeepStateOnSetAll = TRUE; // スリープ復帰の状態を維持

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)
		vAHI_DioWakeEnable(0, 0);

		// スリープを行うが、WAKE_TIMER_0 は定周期スリープのためにカウントを続けているため
		// 空いている WAKE_TIMER_1 を利用する
		ToCoNet_vSleep(E_AHI_WAKE_TIMER_1, 50, FALSE, FALSE); // PERIODIC RAM OFF SLEEP USING WK1
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	if( ToCoNet_Event_u32TickFrNewState(pEv) > 100 && bTimeout == FALSE ){
		bTimeout = TRUE;
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
		//ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
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

		uint8 au8Data[64];
		uint8 *q =  au8Data;

		if( sPALData.u8EEPROMStatus != 0 ){
			S_OCTET(6);		// データ数
			S_OCTET(0x32);
			S_OCTET(0x00);
			S_OCTET( sPALData.u8EEPROMStatus );
		}else{
			S_OCTET(5);		// データ数
		}

		S_OCTET(0x30);					// 電圧
		S_OCTET(0x01);					// 電源電圧
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// 電圧
		S_OCTET(0x02);					// ADC1(AI1)
		S_BE_WORD(sAppData.u16Adc[0]);

		S_OCTET(0x01);			// Temp
		S_OCTET(0x00);
		S_BE_WORD(sObjSHTC3.ai16Result[SHTC3_IDX_TEMP]);

		if(sObjSHTC3.ai16Result[SHTC3_IDX_TEMP] == SHTC3_DATA_ERROR || sObjSHTC3.ai16Result[SHTC3_IDX_TEMP] == SHTC3_DATA_NOTYET  ){
			au8ErrCount[0]++;
		}else{
			au8ErrCount[0] = 0;
		}

		S_OCTET(0x02);			// Hum
		S_OCTET(0x00);
		S_BE_WORD(sObjSHTC3.ai16Result[SHTC3_IDX_HUMID]);

		if(sObjSHTC3.ai16Result[SHTC3_IDX_HUMID] == SHTC3_DATA_ERROR || sObjSHTC3.ai16Result[SHTC3_IDX_HUMID] == SHTC3_DATA_NOTYET  ){
			au8ErrCount[1]++;
		}else{
			au8ErrCount[1] = 0;
		}

		S_OCTET(0x03);			// Lux
		S_OCTET(0x00);
		S_BE_DWORD(sObjLTR308ALS.u32Result);

		if(sObjLTR308ALS.u32Result == LTR308ALS_DATA_ERROR || sObjLTR308ALS.u32Result == LTR308ALS_DATA_NOTYET  ){
			au8ErrCount[1]++;
		}else{
			au8ErrCount[1] = 0;
		}

		sAppData.u16frame_count++;
		if ( bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data ) ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
			V_PRINTF(LB"TxOk");
		} else {
			V_PRINTF(LB"TxFl");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		if(sAppData.bColdStart){
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED); // 点滅させる
		}else{
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/* 電源投入時はLEDを点滅させる */
PRSEV_HANDLER_DEF(E_STATE_APP_BLINK_LED, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		sAppData.u8LedState = 0x02;
	}

	// タイムアウトの場合はスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 750) {
		sAppData.u8LedState = 0;
		LED_OFF();
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		// スリープ時間を計算する
		uint32 u32Sleep = 60000;	// 60 * 1000ms
		if( sAppData.u32SleepCount == 0 && sAppData.u8Sleep_sec ){
			u32Sleep = sAppData.u8Sleep_sec*1000;
		}

		V_PRINTF(LB"Sleeping... %d", u32Sleep);
		V_FLUSH();

		pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEnable( u32DioPortWakeUp, 0 ); // also use as DIO WAKE SOURCE
		vAHI_DioWakeEdge( 0, u32DioPortWakeUp ); // 割り込みエッジ（立下りに設定）

		// 周期スリープに入る
		vSleep( u32Sleep, sAppData.u16frame_count == 1 ? FALSE : TRUE, FALSE);
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
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_BLINK_LED),
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
		vProcessENV(E_EVENT_TICK_TIMER);
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
void vInitAppENV() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}

static void vProcessENV(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj[0]) && bSnsObj_isComplete(&sSnsObj[1])) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj[0], eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj[0]) && !(u8sns_cmplt&E_SNS_SHTC3_CMP) ){
		u8sns_cmplt |= E_SNS_SHTC3_CMP;

		V_PRINTF(LB"!SHTC3: %d.%02dC %d.%02d%%",
			sObjSHTC3.ai16Result[SHTC3_IDX_TEMP] / 100, sObjSHTC3.ai16Result[SHTC3_IDX_TEMP] % 100,
			sObjSHTC3.ai16Result[SHTC3_IDX_HUMID] / 100, sObjSHTC3.ai16Result[SHTC3_IDX_HUMID] % 100
		);
		V_FLUSH();

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}

	vSnsObj_Process(&sSnsObj[1], eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj[1]) && !(u8sns_cmplt&E_SNS_LTR308ALS_CMP) ) {
		u8sns_cmplt |= E_SNS_LTR308ALS_CMP;

		V_PRINTF(LB"!LTR308ALS: %d Lux", sObjLTR308ALS.u32Result);
		V_FLUSH();

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.u16Adc[0] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);
}
