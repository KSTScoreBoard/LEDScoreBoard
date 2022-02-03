/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"
#include "ccitt8.h"
#include "Interactive.h"
#include "sensor_driver.h"
#include "MC3630.h"

#include "flash.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "accel_event.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessMC3630(teEvent eEvent);
static bool_t bSendData( int16 ai16accel[3][32], uint8 u8startAddr, uint8 u8Sample );
static bool_t bTap = TRUE;

#define ABS(c) (c<0?(-1*c):c)

static uint8 u8sns_cmplt = 0;
static tsSnsObj sSnsObj;
static tsObjData_MC3630 sObjMC3630;

static uint8 DI_Bitmap = 0;
static bool_t bInverse = FALSE;
static bool_t bRegular = TRUE;

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
		vMC3630_Init( &sObjMC3630, &sSnsObj );
		sObjMC3630.u8SampleFreq = MC3630_SAMPLING100HZ;

		if( bFirst ){
			bFirst = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_BLINK_LED);
		}else
		if( sAppData.bWakeupByButton ){
			sObjMC3630.u8Interrupt = u8MC3630_ReadInterrupt();
			vMC3630_ClearInterrupReg();
			if( sObjMC3630.u8Interrupt ){
				V_PRINTF(LB"Int:%02X", sObjMC3630.u8Interrupt);
				if( sObjMC3630.u8Interrupt ){
					if(bTap) bRegular = FALSE;
					vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
					if (bSnsObj_isComplete(&sSnsObj)) {
						// 即座に完了した時はセンサーが接続されていない、通信エラー等
						u8sns_cmplt |= E_SNS_MC3630_CMP;
						V_PRINTF(LB "*** MC3630 comm err?");
						ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
						return;
					}

					// ADC の取得
					vADC_WaitInit();
					vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);	

					// センサ値を読み込んで送信する
					// RUNNING 状態
					ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
				}else{
					V_PRINTF(LB"*** No Send...");
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				}
			}else{
				V_PRINTF(LB "*** MC3630 Start Measurement ");
				bTap = FALSE;
				bRegular = FALSE;
				bMC3630_reset( sObjMC3630.u8SampleFreq, MC3630_RANGE16G, 10 );
				vMC3630_StartFIFO();
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			}
		}else{
			V_PRINTF(LB "*** MC3630 Start Measurement2 ");
			bTap = FALSE;
			bRegular = TRUE;
			bMC3630_reset( sObjMC3630.u8SampleFreq, MC3630_RANGE16G, 10 );
			vMC3630_StartFIFO();
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}
	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/* 電源投入時はLEDを点滅させる */
PRSEV_HANDLER_DEF(E_STATE_APP_BLINK_LED, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(eEvent == E_EVENT_NEW_STATE){
		V_PRINTF(LB "*** MC3630 Setting...");

		bool_t bOk = TRUE;

		sObjMC3630.u8SampleFreq = MC3630_SAMPLING100HZ;
		bOk = bMC3630_reset( sObjMC3630.u8SampleFreq, MC3630_RANGE16G, 10 );
		if(bOk == FALSE){
			V_PRINTF(LB "Access failed.");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}
		V_PRINTF(LB "*** MC3630 Setting Compleate!");
		V_PRINTF(LB "*** Blink LED ");
		sAppData.u8LedState = 0x02;
	}

	// タイムアウトの場合はスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 750) {
		V_PRINTF(LB "*** MC3630 First Sleep ");

		DI_Bitmap = bPortRead(SNS_EN) ? 0x01 : 0x00;
		DI_Bitmap |= bPortRead(INPUT_PC) ? 0x02 : 0x00;
		if(DI_Bitmap != 0){
			bInverse = TRUE;
		}else{
			bInverse = FALSE;
		}
		V_PRINTF( LB"HALL : %d, %c", DI_Bitmap, bInverse?'t':'f' );

		//vMC3630_Sleep();
		if( IS_APPCONF_OPT_LOOSE_TH() ){
			vMC3630_StartSNIFF( 2, 1 );
		}else{
			vMC3630_StartSNIFF( 3, 1 );
		}

		sAppData.u8LedState = 0;
		LED_OFF();
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessMC3630(E_EVENT_START_UP);
	}

	// 短期間スリープからの起床をしたので、センサーの値をとる
	if (eEvent == E_EVENT_NEW_STATE) {
		vProcessMC3630(E_EVENT_START_UP);		
		DI_Bitmap = bPortRead(SNS_EN) ? 0x01 : 0x00;
		DI_Bitmap |= bPortRead(INPUT_PC) ? 0x02 : 0x00;
		if(DI_Bitmap != 0){
			bInverse = TRUE;
		}else{
			bInverse = FALSE;
		}
		V_PRINTF( LB"HALL : %d, %c", DI_Bitmap, bInverse?'t':'f' );
		V_FLUSH();
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		if( IS_APPCONF_OPT_LOOSE_TH() ){
			vMC3630_StartSNIFF( 2, 1 );
		}else{
			vMC3630_StartSNIFF( 3, 1 );
		}
		vAccelEvent_Init(100);
		bAccelEvent_SetData( sObjMC3630.ai16Result[MC3630_X], sObjMC3630.ai16Result[MC3630_Y], sObjMC3630.ai16Result[MC3630_Z], sObjMC3630.u8FIFOSample );
		sObjMC3630.u8Event = u8AccelEvent_Top();
		if( sObjMC3630.u8Event > 6 || sObjMC3630.u8Event == 0 ) sObjMC3630.u8Event = 0xFF;

		if( bTap ){
			tsAccelEventData sAccelEvent = tsAccelEvent_GetEvent();
			V_PRINTF( LB"Event:%s", (sAccelEvent.eEvent==1)? "Move":"Tap"  );
			sObjMC3630.u8Event = sAccelEvent.eEvent<<3;
		}

		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		V_FLUSH();
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

		if ( bSendData( sObjMC3630.ai16Result, 0, sObjMC3630.u8FIFOSample ) ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		} else {
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}
		bTap = TRUE;
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
	static uint32 u32Sleep = 0;	// 60 * 1000ms
	if (eEvent == E_EVENT_NEW_STATE) {
		// スリープ時間を計算する
		u32Sleep = 60000;	// 60 * 1000ms
		if( sAppData.u32SleepCount == 0 && sAppData.u8Sleep_sec ){
			u32Sleep = sAppData.u8Sleep_sec*1000;
		}
		if( sAppData.bColdStart ){
			u32Sleep = 500;
		}

		V_PRINTF(LB"! Sleeping... %d", u32Sleep );
		V_FLUSH();

		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if (sAppData.pContextNwk) {
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		pEv->bKeepStateOnSetAll = FALSE; // スリープ復帰の状態を維持しない

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)
		(void)u32AHI_DioInterruptStatus(); // clear interrupt register

		if( bTap == FALSE &&  bRegular == FALSE ){
			vAHI_DioWakeEnable( (1UL<<SNS_INT), ((1UL<<SNS_EN)|(1UL<<INPUT_PC)) ); // ホールICで起床した場合は、ホールICの割り込みを禁止する。
			vAHI_DioWakeEdge( 0, (1UL<<SNS_INT) ); // 割り込みエッジ（立下がりに設定）
		}else{
			vAHI_DioWakeEnable( u32DioPortWakeUp, 0); // ENABLE DIO WAKE SOURCE
			if(bInverse){
				vAHI_DioWakeEdge( ((1UL<<SNS_EN)|(1UL<<INPUT_PC)), (1UL<<SNS_INT) ); // 割り込みエッジ（立下がりに設定）
			}else{
				vAHI_DioWakeEdge( 0, u32DioPortWakeUp ); // 割り込みエッジ（立上がりに設定）
			}
		}

		vMC3630_ClearInterrupReg();

		vSleep(u32Sleep, sAppData.u16frame_count == 1 ? FALSE : TRUE, FALSE);
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
		vProcessMC3630(E_EVENT_TICK_TIMER);
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
void vInitAppCUE() {
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


		//V_PRINTF( LB"!NUM = %d", sObjMC3630.u8FIFOSample );
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
		uint8	au8Data[100];
		uint8*	q = au8Data;

		S_OCTET(5+u8Sample);		// データ数

		S_OCTET(0x34);					// Transmission factor
		S_OCTET(0x81);
		if(bRegular){
			S_OCTET(0x35);
			S_OCTET(0x00);
		}else{
			if(bTap){
				S_OCTET(0x04);
				S_OCTET(0x02);
			}else{
				S_OCTET(0x00);
				S_OCTET(0x01);
			}
		}

		S_OCTET(0x05);
		S_OCTET(0x04);
		S_OCTET(sObjMC3630.u8Event);

		S_OCTET(0x30);					// 電源電圧
		S_OCTET(0x01)
		S_OCTET(sAppData.u8Batt);

		S_OCTET(0x30);					// AI1(ADC1)
		S_OCTET(0x02)
		S_BE_WORD(sAppData.u16Adc[0]);

		S_OCTET(0x00);			// HALL IC
		S_OCTET(0x00);			// 予約領域
		S_OCTET(DI_Bitmap | (bRegular?0x80:0x00) );

		if( u8Sample ){
			S_OCTET(0x04);					// Acceleration
			S_OCTET( sObjMC3630.u8Interrupt );		// 拡張用1バイト
			S_OCTET( u8Sample );				// サンプル数
			S_OCTET( sObjMC3630.u8SampleFreq-MC3630_SAMPLING25HZ );		// サンプリング周波数
			S_OCTET( 12 );			// 送信するデータの分解能

			V_PRINTF( LB"sample = %d", u8Sample );
			V_PRINTF( LB"freq = %d", sObjMC3630.u8SampleFreq-MC3630_SAMPLING25HZ );
			V_FLUSH();

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

		}

		sAppData.u16frame_count++;

		V_PRINTF(LB"Byte:%d", q-au8Data );
		V_FLUSH();

		return bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
}