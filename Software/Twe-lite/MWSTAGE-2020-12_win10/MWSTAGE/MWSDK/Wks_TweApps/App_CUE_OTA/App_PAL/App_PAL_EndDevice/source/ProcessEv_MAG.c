/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"

#ifdef USE_CUE
#include "App_CUE.h"
#include "SPI.h"
#include "MC3630.h"
#else
#include "EndDevice.h"
#endif

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();

#define E_APPCONF_OPT_WAKE_PERIODIC 0x00000001UL
#define IS_APPCONF_OPT_WAKE_PERIODIC() ((sAppData.sFlash.sData.u32param & E_APPCONF_OPT_WAKE_PERIODIC) != 0)
#define E_APPCONF_OPT_EVENT_INTEGRATION 0x00000002UL
#define IS_APPCONF_OPT_EVENT_INTEGRATION() ((sAppData.sFlash.sData.u32param & E_APPCONF_OPT_EVENT_INTEGRATION) != 0)

#define END_INPUT 0
#define END_ADC 1
#define END_TX 2
static uint8 u8sns_cmplt = 0;
static uint8 DI_Bitmap = 0;
static bool_t bInverse = FALSE;

/*
 * 最初に遷移してくる状態
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	//	この状態から始まったときモジュールIDなどを表示する
	if (eEvent == E_EVENT_START_UP) {
		//	初回起動(リセット)かスリープからの復帰かで表示するメッセージを変える
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 起床メッセージ
			vSerInitMessage();

			// 開始する
			// start up message
			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

#ifdef USE_CUE
			// SPIの初期化
			vSPIInit( SPI_MODE3, SLAVE_ENABLE1, 1);
			vMC3630_Sleep();
#endif

			if( sAppData.u8LID&0x80 ){
				sAppData.sFlash.sData.u32param |= 0x00000010;
			}
		}
		V_FLUSH();

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// 定期送信するときはカウントが0の時と割り込み起床したときだけ送信する
		if( !IS_APPCONF_OPT_WAKE_PERIODIC() ){
			if( sAppData.u32SleepCount != 0 && sAppData.bWakeupByButton == FALSE ){
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				return;
			}
		}else{
			if( sAppData.bColdStart==FALSE && sAppData.bWakeupByButton == FALSE ){
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				return;
			}
		}

		// ADC の開始
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		
		// 入力待ち状態へ遷移
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_INPUT);
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_INPUT, tsEvent *pEv, teEvent eEvent, uint32 u32evarg){
	static bool_t bTimeout = FALSE;
	if( eEvent == E_EVENT_NEW_STATE ){
		DI_Bitmap = bPortRead(SNS_EN) ? 0x01 : 0x00;
#ifdef USE_CUE
		DI_Bitmap |= bPortRead(INPUT_PC) ? 0x02 : 0x00;
#else
		DI_Bitmap |= bPortRead(SNS_INT) ? 0x02 : 0x00;
#endif
		if(DI_Bitmap != 0){
			bInverse = TRUE;
		}else{
			bInverse = FALSE;
		}
		bTimeout = FALSE;
	}

	if( u8sns_cmplt == END_ADC ){
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}

	if( ToCoNet_Event_u32TickFrNewState(pEv) > 100 && bTimeout == FALSE ){
		bTimeout = TRUE;
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}

	// タイムアウトの場合はスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_INPUT)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/*	パケットを送信する状態	*/
PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
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

		uint8*	q;
		bool_t bOk = FALSE;
		/*	DIの入力状態を取得	*/
		uint8	au8Data[64];
		q = au8Data;

		uint8 u8Event = (sAppData.u8LID&0x80) ? 1:0;

		if( sPALData.u8EEPROMStatus != 0 ){
			S_OCTET(4);		// データ数
			S_OCTET(0x32);
			S_OCTET(0x00);
			S_OCTET( sPALData.u8EEPROMStatus );
		}else{
			S_OCTET(3);		// データ数
		}

		if(IS_APPCONF_OPT_EVENTMODE_MAG()){
			au8Data[0] += 2;

			S_OCTET(0x34);					// Transmission factor
			S_OCTET(0x82);
			if(!sAppData.bWakeupByButton){
				S_OCTET(0x35);
				S_OCTET(0x00);
			}else{
				S_OCTET(0x00);
				S_OCTET(0x01);
			}

			S_OCTET(0x05);					// Event
			S_OCTET(0x00);
			uint8 Event = DI_Bitmap;
			if( IS_APPCONF_OPT_EVENT_INTEGRATION() ){
				Event = DI_Bitmap ? 1:0;
			}
			S_OCTET( Event );
		}

		S_OCTET(0x30);					// 電源電圧
		S_OCTET(0x01);
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// AI1(ADC1)
		S_OCTET(0x02);
		S_BE_WORD(sAppData.u16Adc[0]);

		S_OCTET(0x00);			// HALL IC
		S_OCTET(0x00);			// 予約領域
		S_OCTET(DI_Bitmap | (sAppData.bWakeupByButton?0x00:0x80) );

		if(u8Event){
			S_OCTET(0x05);					// Acceleration Event
			S_OCTET(0x00);
			uint8 Event = DI_Bitmap;
			if( IS_APPCONF_OPT_EVENT_INTEGRATION() ){
				Event = DI_Bitmap ? 1:0;
			}
			S_OCTET( Event );
		}

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

/*	送信完了状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_ORDER_KICK  && u32evarg == END_TX ) { // 送信完了イベントが来たのでスリープする
		V_PRINTF(LB"ColdStart:%s", sAppData.bColdStart?"TRUE":"FALSE");
		V_FLUSH();
		if(sAppData.bColdStart){
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED); // 点滅させる
		}else{
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
		}
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

/*	スリープをする状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		// スリープ時間を計算する
		uint32 u32Sleep = 60000;	// 60 * 1000ms
		if( sAppData.u32SleepCount == 0 && sAppData.u8Sleep_sec ){
			u32Sleep = sAppData.u8Sleep_sec*1000;
		}

		V_PRINTF(LB"Sleeping... %d", u32Sleep);
		V_FLUSH();

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)
		(void)u32AHI_DioInterruptStatus(); // clear interrupt register

		vAHI_DioWakeEnable( u32DioPortWakeUp, 0); // also use as DIO WAKE SOURCE
		if(bInverse){
			vAHI_DioWakeEdge( u32DioPortWakeUp, 0 ); // 割り込みエッジ（立上がりに設定）
		}else{
			vAHI_DioWakeEdge( 0, u32DioPortWakeUp ); // 割り込みエッジ（立下がりに設定）
		}

		// wake up using wakeup timer as well.
		vSleep( u32Sleep, TRUE, FALSE ); // PERIODIC RAM OFF SLEEP USING WK0
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_INPUT),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_BLINK_LED),
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

		break;

	default:
		break;
	}

	return u8handled;
}

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
			u8sns_cmplt = END_ADC;
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
	ToCoNet_Event_Process(E_ORDER_KICK, END_TX, vProcessEvCore);
}
/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, // cbAppToCoNet_vMain,
	NULL, // cbAppToCoNet_vNwkEvent,
	NULL, // cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppMAG() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.u16Adc[0] = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);
}
