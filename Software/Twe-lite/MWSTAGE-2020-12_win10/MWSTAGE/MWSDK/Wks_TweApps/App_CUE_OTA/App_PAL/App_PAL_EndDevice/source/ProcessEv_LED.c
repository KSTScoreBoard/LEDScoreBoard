/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "utils.h"
#include "ccitt8.h"
#include "Interactive.h"
#include "sensor_driver.h"
#include "MC3630.h"
#include "accel_event.h"
#include "PCA9632.h"

#include "sprintf.h"
#include "flash.h"

#define RED 0
#define GREEN 1
#define BLUE 2
#define WHITE 3

#define END_INPUT 0
#define END_ADC 1
#define END_TX 2
#define END_RX 3

#define E_OPT_DICEMODE 0x00000001UL
#define IS_OPT_DICEMODE() ( (sAppData.sFlash.sData.u32param & E_OPT_DICEMODE) != 0 )

#define E_OPT_DISABLE_ACCELEROMETER 0x00000002UL
#define IS_OPT_DISABLE_ACCELEROMETER() ( (sAppData.sFlash.sData.u32param & E_OPT_DISABLE_ACCELEROMETER) != 0 )

#define ABS(c) (c<0?(-1*c):c)

typedef struct{
	bool_t bChanged;
	uint8 u8LEDState;		// 0b44332211
	uint8 u8LEDDuty[4];
	uint8 u8BlinkCycle;
	uint8 u8BlinkDuty;
	uint16 u16Offtime;
} tsLEDParam;
tsLEDParam sLEDParam;
tsLEDParam asLEDEventParam[32];

uint8 au8BlinkCycle[16] = { 0, 0x17, 0x0B, 0x05, 0x17, 0x17, 0x2E, 0x2E, 0x45, 0x45, 0x0C, 0x0C, 0x08, 0x08, 0x2E, 0x45 };
uint8 au8BlinkDuty[16] =  { 0, 0x7F, 0x7F, 0x7F, 0x06, 0x0D, 0x03, 0x07, 0x02, 0x05, 0x0C, 0x1A, 0x12, 0x27, 0x7F, 0x7F };

static void vLEDEventParam_Init();
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessMC3630(teEvent eEvent);
static bool_t bSendData( int16 ai16accel[3][32], uint8 u8startAddr, uint8 u8Sample );
void vSetLED();

static uint8 u8sns_cmplt = 0;
static tsSnsObj sSnsObj;
static tsObjData_MC3630 sObjMC3630;
static uint8 u8Event_before = 0; 
static bool_t bShortSleep = FALSE;

uint8 au8Color[9][4] = {
//	 R, G, B, W
	{1, 1, 1, 0},
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{1, 1, 0, 0},
	{1, 0, 1, 0},
	{0, 1, 1, 0},
	{0, 0, 0, 1},
	{1, 1, 1, 1}
};

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_MC3630_CMP = 2,
	E_SNS_ALL_CMP = 3
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

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

			// LEDドライバーの初期化
			bPCA9632_Reset();
			bPCA9632_Init();

			bPCA9632_Duty(PCA9632_RED, 63);
			bPCA9632_Duty(PCA9632_GREEN, 63);
			bPCA9632_Duty(PCA9632_BLUE, 63);
			bPCA9632_Duty(PCA9632_WHITE, 63);
			bPCA9632_LEDStatus(0x00);

			vLEDEventParam_Init();

			// LEDを有効にする
			vPortSetLo(SNS_EN);
		}

		// RC クロックのキャリブレーションを行う
		if(sAppData.sFlash.sData.u16RcClock == 0){
			sAppData.sFlash.sData.u16RcClock = ToCoNet_u16RcCalib(0);
		}

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;
		vMC3630_Init( &sObjMC3630, &sSnsObj );
		if( !IS_OPT_DISABLE_ACCELEROMETER() ){
			if( IS_OPT_DICEMODE() ){
				sObjMC3630.u8Event = u8Event_before;
			}
			V_PRINTF(LB "! event = %d", sObjMC3630.u8Event );
		}else{
			vMC3630_Sleep();
		}

		// ショートスリープだったらLEDをいったん消す
		if( !sAppData.bWakeupByButton && bShortSleep ){
			bShortSleep = FALSE;
			bPCA9632_LEDStatus(0);
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}

		sToCoNet_AppContext.bRxOnIdle = TRUE;
		ToCoNet_vMacStart();

		if( !IS_OPT_DISABLE_ACCELEROMETER() ){
			sObjMC3630.u8Interrupt = u8MC3630_ReadInterrupt();
			V_PRINTF(LB"Int:%02X", sObjMC3630.u8Interrupt);
			bool_t bSnsInt = bPortRead( SNS_INT );

			// 割り込みピンがLoまたはイベントレジスタのフラグが立っていれば割り込み起床として扱う
			if( bSnsInt || (sObjMC3630.u8Interrupt&0x44) ){
				sAppData.bWakeupByButton = TRUE;
			}
		}

		if( bFirst ){
			bFirst = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED);
		}else{
			if( !sAppData.bWakeupByButton ){
				//V_PRINTF(LB "*** Wake by Timer.");
				//bPCA9632_LEDStatus(0x00);
				if( sAppData.u32SleepCount == 0 ){
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_ADC); // 親機に問い合わせる
				}else{
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
					return;
				}
			}else{
				if( IS_OPT_DISABLE_ACCELEROMETER() ){
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_ADC); // 親機に問い合わせる
				}else{
					vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
					if (bSnsObj_isComplete(&sSnsObj)) {
						// 即座に完了した時はセンサーが接続されていない、通信エラー等
						u8sns_cmplt |= E_SNS_MC3630_CMP;
						V_PRINTF(LB "*** MC3630 comm err?");
						ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
						return;
					}

					// センサ値を読み込んで送信する
					// RUNNING 状態
					ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
				}
			}

			// ADC の取得
			vADC_WaitInit();
			vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		}

	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/* 電源投入時はLEDを点滅させる */
PRSEV_HANDLER_DEF(E_STATE_APP_BLINK_LED, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		if( !IS_OPT_DISABLE_ACCELEROMETER() ){
			V_PRINTF(LB "*** MC3630 Setting...");

			sObjMC3630.u8SampleFreq = MC3630_SAMPLING100HZ;
			bool_t bOk = bMC3630_reset( sObjMC3630.u8SampleFreq, MC3630_RANGE16G, 10 );
			if(bOk == FALSE){
				V_PRINTF(LB "Access failed.");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
				return;
			}
			V_PRINTF(LB "*** MC3630 Setting Compleate!");
		}
		V_PRINTF(LB "*** Blink LED ");
		sAppData.u8LedState = 0x02;
	}

	// タイムアウトの場合はスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 750) {
		if( !IS_OPT_DISABLE_ACCELEROMETER() ){
			V_PRINTF(LB "*** MC3630 First Sleep ");
			if( IS_APPCONF_OPT_LOOSE_TH() ){
				vMC3630_StartSNIFF( 2, 1 );
			}else{
				vMC3630_StartSNIFF( 3, 1 );
			}
		}
		sAppData.u8LedState = 0;
		LED_OFF();
		bPCA9632_LEDStatus(0x00);
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

// ADC 待ち
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_ADC, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(u8sns_cmplt == E_SNS_ADC_CMP_MASK){
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint8 u8count = 1;

		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessMC3630(E_EVENT_START_UP);
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		if( IS_OPT_DICEMODE() ){
			vAccelEvent_Init( 100 );
			bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );
			uint8 u8e = u8AccelEvent_Top();
			if( u8e == 0 || u8e == 0xFF ){
				vMC3630_StartFIFO();
				V_PRINTF(LB"One more");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				return;
			}else{
				if( IS_APPCONF_OPT_LOOSE_TH() ){
					vMC3630_StartSNIFF( 2, 1 );
				}else{
					vMC3630_StartSNIFF( 3, 1 );
				}
				sObjMC3630.u8Event = u8e;
				u8Event_before = sObjMC3630.u8Event;
				sLEDParam = asLEDEventParam[sObjMC3630.u8Event];
				vSetLED();

				if(sLEDParam.u8LEDState){
					sLEDParam.u16Offtime = 1;
				}else{
					sLEDParam.u16Offtime = 0;
				}

				if(sLEDParam.u16Offtime){
					bShortSleep = TRUE;
				}else{
					bShortSleep = FALSE;
				}

				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
			}
		}else{
			switch (u8count){
				case 0:
					vMC3630_StartFIFO();

					V_PRINTF( LB"Clear" );
					vAccelEvent_Init( 100 );
					V_PRINTF( LB"Set" );
					bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );

					sLEDParam.bChanged = FALSE;

					u8count++;
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
					break;
				case 1:
					if( IS_APPCONF_OPT_LOOSE_TH() ){
						vMC3630_StartSNIFF( 2, 1 );
					}else{
						vMC3630_StartSNIFF( 3, 1 );
					}

					vAccelEvent_Init( 100 );
					bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );
					tsAccelEventData sAccelEvent = tsAccelEvent_GetEvent();
					V_PRINTF( LB"Event = %s", (sAccelEvent.eEvent == 0x01) ? "Move":((sAccelEvent.eEvent == 0x02)?"Tap":"None") );
					sObjMC3630.u8Event = sAccelEvent.eEvent<<3;
					sLEDParam = asLEDEventParam[sObjMC3630.u8Event];
					vSetLED();

					if(sLEDParam.u8LEDState){
						sLEDParam.u16Offtime = 1;
					}else{
						sLEDParam.u16Offtime = 0;
					}

					if(sLEDParam.u16Offtime){
						bShortSleep = TRUE;
					}else{
						bShortSleep = FALSE;
					}
					V_PRINTF( LB"LEDState = %02X/%02X", sLEDParam.u8LEDState, sObjMC3630.u8Event  );

					//u8count = 0;
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
					break;
			}
		}
	}

	if( sLEDParam.bChanged == TRUE ){
		vSetLED();

		if(sLEDParam.u8LEDState){
			sLEDParam.u16Offtime = 1;
		}else{
			sLEDParam.u16Offtime = 0;
		}

		if(sLEDParam.u16Offtime){
			bShortSleep = TRUE;
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
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

		if ( bSendData( sObjMC3630.ai16Result, 0, 16 ) ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		} else {
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}
		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来た
		if(sAppData.bWakeupByButton){
			sToCoNet_AppContext.bRxOnIdle = FALSE;
			ToCoNet_vRfConfig();
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
		}else{
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_RX); // 受信待ち
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/*	受信待ち状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_RX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_ORDER_KICK  && u32evarg == END_RX ) { // 受信完了イベントが来たのでスリープする
		V_PRINTF(LB"Recv OK");
		V_FLUSH();

		if( sLEDParam.bChanged == TRUE ){
			vSetLED();

			V_PRINTF(LB"Duty %d %d %d %d", sLEDParam.u8LEDDuty[0], sLEDParam.u8LEDDuty[1], sLEDParam.u8LEDDuty[2], sLEDParam.u8LEDDuty[3]);
			V_PRINTF(LB"OFFTIME %d", sLEDParam.u16Offtime);
			V_PRINTF(LB"STATE %02X", sLEDParam.u8LEDState);

			if(sLEDParam.u16Offtime>0){
#if 0
				uint32 u32offcount = (uint32)( ( (uint64)sLEDParam.u16Offtime * 1000ULL * 10000ULL) / (uint64)ToCoNet_u16RcCalib(0xFFFF) ) * 32UL;
				V_PRINTF( LB"count %d %d", u32offcount, ToCoNet_u16RcCalib(0xFFFF) );
				V_FLUSH();
				vAHI_WakeTimerEnable(E_AHI_WAKE_TIMER_1, TRUE);
				vAHI_WakeTimerStartLarge(E_AHI_WAKE_TIMER_1, (uint64)u32offcount);
			}else{
				vAHI_WakeTimerStop( E_AHI_WAKE_TIMER_1 );
#else
				if( sLEDParam.u16Offtime < sAppData.sFlash.sData.u32Slp  ){
					bShortSleep = TRUE;
				}else{
					bShortSleep = FALSE;
				}
#endif
			}
		}

		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"Rx TIME OUT");
		V_FLUSH();
		sToCoNet_AppContext.bRxOnIdle = FALSE;
		ToCoNet_vRfConfig();
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 寝る
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		vMC3630_ClearInterrupReg();

		uint32 u32Sleep = 60000;	// 60 * 1000ms
		if( sAppData.u32SleepCount == 0 && sAppData.u8Sleep_sec ){
			u32Sleep = sAppData.u8Sleep_sec*1000;
		}
		if(bShortSleep == TRUE && sLEDParam.u16Offtime){
			u32Sleep = sLEDParam.u16Offtime*1000;
		}

		V_PRINTF(LB"! Sleeping... %d", u32Sleep);
		V_FLUSH();

		if (sAppData.pContextNwk) {
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持しない

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEnable( u32DioPortWakeUp, 0); // ENABLE DIO WAKE SOURCE
		vAHI_DioWakeEdge(0, u32DioPortWakeUp ); // 割り込みエッジ(立上がりに設定)

		if( bShortSleep == TRUE && sLEDParam.u16Offtime ){
			vPortSetLo(WDT_OUT);
			// 短期スリープに入る
			ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32Sleep, FALSE, FALSE);
		}else{
			// 周期スリープに入る
			vSleep(u32Sleep, sAppData.bWakeupByButton, FALSE);
		}

	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_BLINK_LED),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_ADC),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_RX),
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
		vProcessMC3630(E_EVENT_TICK_TIMER);

		if(sAppData.u8LedState == 2){
			if(u32TickCount_ms&0x0080){
				bPCA9632_LEDStatus(0x55);
			}else{
				bPCA9632_LEDStatus(0x00);
			}
			vPortSet_TrueAsLo( OUTPUT_LED, u32TickCount_ms&0x0080 );
		}
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


/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {
	// 受信したときの処理を書く。
	uint8* p = pRx->auData;

	// App_PALの返信パケットじゃなかったら処理をやめる
	uint8 u8b = G_OCTET();
	if(u8b != 'P'+0x80){
		return;
	}

	// 親機からじゃなかったら処理をやめる
	uint8 u8id = G_OCTET();
	if( u8id != 121 ){
		return;
	}

	uint16 u16freq = G_BE_WORD();(void)u16freq;

	uint8 u8ver = G_OCTET();
	if( u8ver != 1 ){
		return;
	}

	uint8 i = 0;
	if(Interactive_bGetMode()){
		V_PRINTF(LB"RCV=");
		for (i = 5; i < pRx->u8Len ; i++) {
			V_PRINTF("%02X ", pRx->auData[i]);
		}
	}

	uint8 u8Identifier = G_OCTET();
	uint8 u8Event = G_OCTET();
	V_PRINTF( LB"IDENTIFIER=%d", u8Identifier);

	if( u8Identifier == PKT_ID_LED || (u8Identifier == 0xFF && u8Event < 32) ){
		uint16 u16RGBW = G_BE_WORD();
		uint8 u8Cycle = G_OCTET();
		uint8 u8Duty = G_OCTET();
		uint8 u8Timer = G_OCTET();

		if( u8Event == 0xFE ){
			sLEDParam.u8LEDDuty[RED] = (u16RGBW&0x000F)<<3;
			sLEDParam.u8LEDDuty[GREEN] = ((u16RGBW&0x00F0)>>4)<<3;
			sLEDParam.u8LEDDuty[BLUE] = ((u16RGBW&0x0F00)>>8)<<3;
			sLEDParam.u8LEDDuty[WHITE] = ((u16RGBW&0xF000)>>12)<<3;
			sLEDParam.u8BlinkCycle = u8Cycle;
			sLEDParam.u8BlinkDuty = u8Duty;
			sLEDParam.u16Offtime = u8Timer;
			sLEDParam.u8LEDState = 0;
			for( i=0; i<4; i++ ){
				if( sLEDParam.u8LEDDuty[i] != 0 ){
					sLEDParam.u8LEDState |= (sLEDParam.u8BlinkDuty ? 2 : 1) << (i*2);
				}
			}
			V_PRINTF( LB"TIMER=%d", sLEDParam.u16Offtime);
		}else{
			if( u8Event < 32 ){
				V_PRINTF(LB"Event=%d", u8Event);
				sLEDParam = asLEDEventParam[u8Event];
				V_PRINTF(LB"! %02X", sLEDParam.u8LEDState);
			}
		}
		if( u8Event < 32 || u8Event == 0xFE ){
			sLEDParam.bChanged = TRUE;
		}else{
			sLEDParam.bChanged = FALSE;
		}
	}else{
		sLEDParam.bChanged = FALSE;
	}

	sToCoNet_AppContext.bRxOnIdle = FALSE;
	ToCoNet_vRfConfig();

	ToCoNet_Event_Process(E_ORDER_KICK, END_RX, vProcessEvCore);
}

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
	cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppLED() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}

static void vProcessMC3630(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_MC3630_CMP;


		V_PRINTF( LB"!NUM = %d", sObjMC3630.u8FIFOSample );
		uint8 i;
		for( i=0; i< sObjMC3630.u8FIFOSample; i++){
			V_PRINTF(LB"!MC3630_%d: X : %d, Y : %d, Z : %d",
					i,
					sObjMC3630.ai16Result[MC3630_X][i],
					sObjMC3630.ai16Result[MC3630_Y][i],
					sObjMC3630.ai16Result[MC3630_Z][i]
			);
		}

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

static bool_t bSendData( int16 ai16accel[3][32], uint8 u8startAddr, uint8 u8Sample )
{
		uint8	au8Data[92];
		uint8*	q = au8Data;

		if( sPALData.u8EEPROMStatus != 0 ){
			if(sAppData.bWakeupByButton){
				S_OCTET(4);
			}else{
				S_OCTET(5);		// データ数
			}
			S_OCTET(0x32);
			S_OCTET(0x00);
			S_OCTET( sPALData.u8EEPROMStatus );
		}else{
			//S_OCTET(2+u8Sample);		// データ数
			if(sAppData.bWakeupByButton){
				S_OCTET(3);		// データ数
			}else{
				S_OCTET(4);		// データ数
			}
		}

		S_OCTET(0x30);					// 電源電圧
		S_OCTET(0x01)
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// AI1(ADC1)
		S_OCTET(0x02)
		S_BE_WORD(sAppData.u16Adc[0]);

		S_OCTET(0x05);					// Acceleration Event
		S_OCTET(0x04);
		if( IS_OPT_DISABLE_ACCELEROMETER() ){
			S_OCTET( 0 );
		}else{
			S_OCTET( sObjMC3630.u8Event );
		}

//		S_OCTET(0x06);					// LED
//		S_OCTET( sObjMC3630.u8Event );

		// タイマー起床の時だけデータを取得する
		if( !sAppData.bWakeupByButton ){
			S_OCTET(0x33);
		}

		sAppData.u16frame_count++;

		return bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
}

void vSetLED()
{
	bPCA9632_Duty(RED, sLEDParam.u8LEDDuty[RED]);
	bPCA9632_Duty(GREEN, sLEDParam.u8LEDDuty[GREEN]);
	bPCA9632_Duty(BLUE, sLEDParam.u8LEDDuty[BLUE]);
	bPCA9632_Duty(WHITE, sLEDParam.u8LEDDuty[WHITE]);

	bPCA9632_BlinkCycle(sLEDParam.u8BlinkCycle);
	bPCA9632_BlinkDuty(sLEDParam.u8BlinkDuty);
	bPCA9632_LEDStatus(sLEDParam.u8LEDState );
	sLEDParam.bChanged = FALSE;
}

void vLEDEventParam_Init()
{	
	uint8 i;
	memset( asLEDEventParam, 0, sizeof(tsLEDParam)*32);

	for(i=0; i<32; i++){
		asLEDEventParam[i].bChanged = TRUE;
	}

	for( i=0; i<sAppData.sFlash.sData.u8EventNum; i++ ){
		uint8 u8Id = u32string2hex( sAppData.sFlash.sData.au8Event+(8*i), 2 );
		uint16 u16RGBW = u32string2hex( sAppData.sFlash.sData.au8Event+(8*i+2), 4 );
		uint8 u8Blink = u32string2hex( sAppData.sFlash.sData.au8Event+(8*i+6), 1 );
		uint8 u8Time = u32string2hex( sAppData.sFlash.sData.au8Event+(8*i+7), 1 );
		V_PRINTF(LB"ID%d = %02X", i, u8Id );
		V_PRINTF(LB"RGBW%d = %04X", i, u16RGBW );
		V_PRINTF(LB"Blink%d = %02X", i, u8Blink );
		V_PRINTF(LB"Time%d = %02X", i, u8Time );
		V_PRINTF(LB);

		if( u8Id < 32 ){
			asLEDEventParam[u8Id].u8LEDDuty[RED] = ((u16RGBW&0xF000)>>12)<<3;
			asLEDEventParam[u8Id].u8LEDDuty[GREEN] = ((u16RGBW&0x0F00)>>8)<<3;
			asLEDEventParam[u8Id].u8LEDDuty[BLUE] = ((u16RGBW&0x00F0)>>4)<<3;
			asLEDEventParam[u8Id].u8LEDDuty[WHITE] = (u16RGBW&0x000F)<<3;
			asLEDEventParam[u8Id].u8BlinkDuty = au8BlinkDuty[u8Blink];
			asLEDEventParam[u8Id].u8BlinkCycle = au8BlinkCycle[u8Blink];
			asLEDEventParam[u8Id].u16Offtime = u8Time;
		
			uint8 j;
			for( j=0; j<4; j++ ){
				if( asLEDEventParam[u8Id].u8LEDDuty[j] != 0 ){
					asLEDEventParam[u8Id].u8LEDState |= (asLEDEventParam[u8Id].u8BlinkCycle ? 2 : 1) << (j*2);
				}
			}
		}
	}
}