/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"
#include "ccitt8.h"
#include "Interactive.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "sensor_driver.h"
#include "MC3630.h"
#include "accel_event.h"

#include "flash.h"

#define IS_COUNTUP() (sAppData.sFlash.sData.u32param&0x00000001)

//	サイコロの各々の面だったときに送信するデータマクロ
#define DICE1() \
	S_OCTET(0x01);\
	S_OCTET(0x0F);\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );

#define DICE2() \
	S_OCTET(0x08);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 25 );\
	S_OCTET( 0x00 );

#define DICE3() \
	S_OCTET(0x09);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x00 );\
	S_OCTET( 50 );\
	S_OCTET( 0x00 );

#define DICE4() \
	S_OCTET(0x0A);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x7D );\
	S_OCTET( 75 );\
	S_OCTET( 0x00 );

#define DICE5() \
	S_OCTET(0x0B);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x00 );\
	S_OCTET( 0x7D );\
	S_OCTET( 100 );\
	S_OCTET( 0x00 );

#define DICE6() \
	S_OCTET(0x0E);\
	S_OCTET(0x0F);\
	S_OCTET( 0x7D );\
	S_OCTET( 0x7D );\
	S_OCTET( 0x7D );\
	S_OCTET( 125 );\
	S_OCTET( 0x00 );

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static bool_t bSendData();
static bool_t bSendAppTag();
static bool_t bSendAppTwelite();

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
			V_PRINTF(LB "* Event Mode");

		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// 定期送信するときはカウントが0の時と割り込み起床したときだけ送信する
		if( sAppData.u32SleepCount != 0 && sAppData.bWakeupByButton == FALSE ){
			V_PRINTF(LB"*** No Send...");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			return;
		}

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		if( bFirst ){
			bFirst = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED);
		}else{
			if( !sAppData.bWakeupByButton ){
				V_PRINTF(LB "*** Wake by Timer.");
			}
			// ADC の取得
			vADC_WaitInit();
			vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

			// センサ値を読み込んで送信する
			// RUNNING 状態
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
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
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 750) {
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
		if(sObjMC3630.u8Event == 0xFF){
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}else{
			if( IS_COUNTUP() && sObjMC3630.u8Event == 0x10 ){
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			}else{
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
			}
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
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

		if( bSendData() ){
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		} else {
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}
		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
	}
	if (sObjMC3630.bFinished) {
		// スリープ時間を計算する
		uint32 u32Sleep = 60000;	// 60 * 1000ms
		if( sAppData.u32SleepCount == 0 && sAppData.u8Sleep_sec ){
			u32Sleep = sAppData.u8Sleep_sec*1000;
		}

		V_PRINTF(LB"! Sleeping... %d", u32Sleep);
		V_FLUSH();

		sObjMC3630.bFinished = FALSE;

		if (sAppData.pContextNwk) {
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		if( !sAppData.bColdStart ) vMC3630_ClearInterrupReg();
		else if( IS_APPCONF_OPT_DICEMODE() )  vMC3630_StartFIFO();

		pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持しない

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEnable( u32DioPortWakeUp, 0); // ENABLE DIO WAKE SOURCE
		vAHI_DioWakeEdge(0, u32DioPortWakeUp ); // 割り込みエッジ(立上がりに設定)

		// 周期スリープに入る
		vSleep( u32Sleep, !sAppData.bWakeupByButton, FALSE);
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

static bool_t bSendData()
{
	if(IS_APPCONF_OPT_2525AMODE()) return bSendAppTag();
	if(IS_APPCONF_OPT_APPTWELITEMODE()) return bSendAppTwelite();

	uint8	au8Data[92];
	uint8*	q = au8Data;

	if( sPALData.u8EEPROMStatus != 0 ){
		S_OCTET(5);		// データ数
		S_OCTET(0x32);
		S_OCTET(0x00);
		S_OCTET( sPALData.u8EEPROMStatus );
	}else{
		S_OCTET(4);		// データ数
	}

	S_OCTET(0x34);					// Transmission factor
	S_OCTET(0x80);
	if( sAppData.bWakeupByButton ){
		S_OCTET(0x04);
		S_OCTET(0x02);		
	}else{
		S_OCTET(0x35);
		S_OCTET(0x00)
	}

	S_OCTET(0x05);					// Acceleration Event
	S_OCTET(0x04);
	S_OCTET( sObjMC3630.u8Event );

	S_OCTET(0x30);					// 電源電圧
	S_OCTET(0x01)
	S_OCTET(sAppData.u8Batt);

	S_OCTET(0x30);					// AI1(ADC1)
	S_OCTET(0x02)
	S_BE_WORD(sAppData.u16Adc[0]);



	sAppData.u16frame_count++;

	return bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
}

static bool_t bSendAppTag()
{
	uint8	au8Data[92];
	uint8*	q = au8Data;

	S_OCTET(sAppData.u8Batt);
	S_BE_WORD(sAppData.u16Adc[0]);
	S_BE_WORD(0x0000);

	if( IS_APPCONF_OPT_DICEMODE() ){
		S_BE_WORD( sObjMC3630.u8Event );
		S_BE_WORD( 0x0000 );
		S_BE_WORD( 0x0000 );
		S_OCTET(0xFD);
	}else if( IS_APPCONF_OPT_EVENTMODE() ){
		S_BE_WORD( sObjMC3630.ai16Result[0][0] );
		S_BE_WORD( sObjMC3630.ai16Result[1][0] );
		S_BE_WORD( sObjMC3630.ai16Result[2][0] );
		if((sObjMC3630.u8Event>>3) == ACCEL_EVENT_TAP){
			S_OCTET(0x01);

		}else
		if((sObjMC3630.u8Event>>3) == ACCEL_EVENT_MOVE){
			S_OCTET(0x08);
		}else
		{
			S_OCTET(0x10);
		}
		
	}

	sAppData.u16frame_count++;
	return bTransmitToAppTag( sAppData.pContextNwk, au8Data, q-au8Data );
}

bool_t bSendAppTwelite()
{
	uint8	au8Data[92];
	uint8*	q = au8Data;

	switch( sObjMC3630.u8Event ){
		case 0:
			S_OCTET(0);
			S_OCTET(0x0F);
			S_BE_DWORD(0);
			S_OCTET(0);
			break;
		case 1:
			DICE1();
			break;
		case 2:
			if( IS_COUNTUP() ){
				S_OCTET(0x02);
				S_OCTET(0x0F);
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 25 );
				S_OCTET( 0x00 )
			}else{
				DICE2();
			}
			break;
		case 3:
			if( IS_COUNTUP() ){
				S_OCTET(0x04);
				S_OCTET(0x0F);
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 50 );
				S_OCTET( 0x00 )
			}else{
				DICE3();
			}
			break;
		case 4:
			if( IS_COUNTUP() ){
				S_OCTET(0x08);
				S_OCTET(0x0F);
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 75 );
				S_OCTET( 0x00 )
			}else{
				DICE4();
			}
			break;
		case 5:
			if( IS_COUNTUP() ){
				S_OCTET(0x00);
				S_OCTET(0x0F);
				S_OCTET( 0x7D );
				S_OCTET( 0x00 );
				S_OCTET( 0x00 );
				S_OCTET( 100 );
				S_OCTET( 0x00 )
			}else{
				DICE5();
			}
			break;
		case 6:
			if( IS_COUNTUP() ){
				S_OCTET(0x00);
				S_OCTET(0x0F);
				S_OCTET( 0x00 );
				S_OCTET( 0x7D );
				S_OCTET( 0x00 );
				S_OCTET( 125 );
				S_OCTET( 0x00 )
			}else{
				DICE6();
			}

			break;
		case 8:
			S_OCTET(0x01);
			S_OCTET(0x0F);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			break;
		case 16:
			S_OCTET(0x01);
			S_OCTET(0x0F);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);
			break;
		default:
			return FALSE;
	}
	return bTransmitToAppTwelite( au8Data, q-au8Data );
}