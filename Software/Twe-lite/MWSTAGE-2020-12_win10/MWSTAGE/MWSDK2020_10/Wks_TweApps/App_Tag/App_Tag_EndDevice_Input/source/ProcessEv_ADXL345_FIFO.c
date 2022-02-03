/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "ccitt8.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

#include "sensor_driver.h"
#include "ADXL345.h"
#include "SMBus.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessADXL345_FIFO(teEvent eEvent);
static uint8 u8sns_cmplt = 0;

static uint8 u8GetCount = 0;

static bool_t bPoweroff = FALSE;

static tsSnsObj sSnsObj;
static tsObjData_ADXL345 sObjADXL345;

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_ADXL345_CMP = 2,
	E_SNS_ALL_CMP = 3
};

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bFirst = TRUE;
	static bool_t bOk = TRUE;
	static bool_t bFIFO_Measuring = FALSE;
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
			// ADXL345 の初期化
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

/*
		// SWがLowになっていたらスリープする
		if( bPortRead(DIO_BUTTON) ){
			bPoweroff = TRUE;
			bFIFO_Measuring = FALSE;
			if(bFirst == FALSE){
				bADXL345_EndMeasuring();
			}
			bFirst = TRUE;
			V_PRINTF(LB "*** POWEROFF");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			return;
		}else{
			V_PRINTF(LB "*** POWERON");
			bPoweroff = FALSE;
		}
*/

		vADXL345_FIFO_Init( &sObjADXL345, &sSnsObj );
		if( bFirst ){
			V_PRINTF(LB "*** ADXL345 FIFO Setting...");
			bOk &= bADXL345reset();
			bOk &= bADXL345_FIFO_Setting( (uint16)(sAppData.sFlash.sData.i16param&0xFFFF), sAppData.sFlash.sData.uParam.sADXL345Param );
		}
		vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj) || !bOk ) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_ADXL345_CMP;
			V_PRINTF(LB "*** ADXL345 comm err?");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}

		if( sAppData.bWakeupByButton || bFirst ){
			V_PRINTF(LB"Interrupt:0x%02X", sObjADXL345.u8Interrupt);
			if( (sAppData.sFlash.sData.i16param&0xF) && !bFirst ){
				if( sObjADXL345.u8Interrupt&0x74 ){
					if( bFIFO_Measuring && sAppData.sFlash.sData.u8wait == 0 ){
						bFIFO_Measuring = FALSE;
						V_PRINTF(LB"Disable");
						bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
					}else{
						bFIFO_Measuring = TRUE;
						V_PRINTF(LB"Enable");
						bADXL345_EnableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
					}
				}else if( (sObjADXL345.u8Interrupt&0x08) && sAppData.sFlash.sData.u8wait == 0 ){
					bFIFO_Measuring = FALSE;
					V_PRINTF(LB"Disable");
					bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
				}
				if(bFIFO_Measuring && (sObjADXL345.u8Interrupt&0x02) ){
					bFirst = FALSE;
					// ADC の取得
					vADC_WaitInit();
					vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

					// RUNNING 状態
					ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
				}else{
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				}
			}else{
				bFirst = FALSE;
				// ADC の取得
				vADC_WaitInit();
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

				// RUNNING 状態
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}
		}else{
			bADXL345_StartMeasuring(FALSE);
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}

	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessADXL345_FIFO(E_EVENT_START_UP);
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static int32 ave[3] = {0,0,0};
	static int16 min[3] = {17000,17000,17000};
	static int16 max[3] = {-17000,-17000,-17000};
	static uint16 u16Sample = 0;

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

		int16 ai16ResultAccel[3][10];		// 3軸データを10サンプルまで
		uint8 u8Sample = sObjADXL345.u8FIFOSample;
		uint8 i = 0;
		if( sObjADXL345.u8FIFOSample > 10 ){
			u8Sample = 10;
			i = sObjADXL345.u8FIFOSample-10;
		}

		uint8 j;
		for( j=0; j<u8Sample; j++){
			ai16ResultAccel[0][j] = sObjADXL345.ai16ResultX[i+j];
			ai16ResultAccel[1][j] = sObjADXL345.ai16ResultY[i+j];
			ai16ResultAccel[2][j] = sObjADXL345.ai16ResultZ[i+j];

			uint8 k;
			for(k=0; k<3; k++){
				ave[k] += ai16ResultAccel[k][j];
				if( min[k] > ai16ResultAccel[k][j] ){
					min[k] = ai16ResultAccel[k][j];
				}
				if( max[k] < ai16ResultAccel[k][j] ){
					max[k] = ai16ResultAccel[k][j];
				}
			}
		}
		u16Sample += u8Sample;

		if( (sAppData.sFlash.sData.i16param&2048) == 0 ||			// min,max以外は送る
			 sAppData.sFlash.sData.u8wait == 0 ||					// 送信回数が設定されなければ送る
			 u8GetCount >= sAppData.sFlash.sData.u8wait-1 ){		// 最後の一回は送る
			if( sAppData.bWakeupByButton && (sObjADXL345.u8Interrupt&0x02) ){
				u8GetCount++;		// 送信回数をインクリメント

				uint8	au8Data[91];
				uint8*	q = au8Data;
				S_OCTET(sAppData.sSns.u8Batt);
				S_BE_WORD(sAppData.sSns.u16Adc1);
				S_BE_WORD(sAppData.sSns.u16Adc2);

				if( sAppData.sFlash.sData.i16param&2048 ){
					ave[0] /= u16Sample;
					ave[1] /= u16Sample;
					ave[2] /= u16Sample;
					V_PRINTF( LB"min=%d", min[0] );
					V_PRINTF( LB"ave=%d", ave[0] );
					V_PRINTF( LB"max=%d"LB, max[0] );

					S_BE_WORD(min[0]);
					S_BE_WORD(ave[0]);
					S_BE_WORD(max[0]);
					S_OCTET( 0xF9 );

					S_BE_WORD(u16Sample);

					S_BE_WORD(min[1]);
					S_BE_WORD(ave[1]);
					S_BE_WORD(max[1]);

					S_BE_WORD(min[2]);
					S_BE_WORD(ave[2]);
					S_BE_WORD(max[2]);

					max[0] = -32768;
					max[1] = -32768;
					max[2] = -32768;
					min[0] = 32767;
					min[1] = 32767;
					min[2] = 32767;
					ave[0] = 0;
					ave[1] = 0;
					ave[2] = 0;
				}else{
					S_BE_WORD(ai16ResultAccel[0][0]);
					S_BE_WORD(ai16ResultAccel[1][0]);
					S_BE_WORD(ai16ResultAccel[2][0]);

					S_OCTET( 0xFA );
					S_OCTET( u16Sample );
					for( j=1; j<u8Sample; j++ ){
						S_BE_WORD(ai16ResultAccel[0][j]);
						S_BE_WORD(ai16ResultAccel[1][j]);
						S_BE_WORD(ai16ResultAccel[2][j]);
					}
				}
				u16Sample = 0;

				sAppData.u16frame_count++;

				if ( bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data ) ) {
				} else {
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
				}
			}else{
				V_PRINTF(LB"First");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
			}
		}else{
			if(u16Sample > 0){
				u8GetCount++;		// 送信回数をインクリメント
			}
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		uint16 u16Rate = (1000/u16ADXL345_GetSamplingFrequency())*10;
		uint32 u32Sleep = sAppData.sFlash.sData.u32Slp-(u16Rate*sAppData.sFlash.sData.u8wait);

		if(bPoweroff){
			vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // ENABLE DIO WAKE SOURCE
			(void)u32AHI_DioInterruptStatus(); // clear interrupt register
			vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ(立上がりに設定)
			u32Sleep = 0;

			bADXL345_EndMeasuring();
			// 起床中にサンプリングしていても良いように読み込んでFIFOの中身をクリアする
			bADXL345FIFOreadResult( sObjADXL345.ai16ResultX, sObjADXL345.ai16ResultY, sObjADXL345.ai16ResultZ );
			// ダメ押しでレジスタに残っているデータも読み込む
			uint8 au8data[6];
			bool_t bOk = TRUE;
			GetAxis(bOk, au8data);
		}else{
			vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // ENABLE DIO WAKE SOURCE
			(void)u32AHI_DioInterruptStatus(); // clear interrupt register
			vAHI_DioWakeEdge( (1UL<<PORT_INPUT2)|(1UL<<PORT_INPUT3), (1UL<<DIO_BUTTON)); // 割り込みエッジ(立上がりに設定)


			if( sAppData.sFlash.sData.u8wait == 0 || u8GetCount < sAppData.sFlash.sData.u8wait  ){
				u32Sleep = 0;
			}else{
				if( (sAppData.sFlash.sData.i16param&0x000F) == 0  ){
					bADXL345_EndMeasuring();

					// 起床中にサンプリングしていても良いように読み込んでFIFOの中身をクリアする
					bADXL345FIFOreadResult( sObjADXL345.ai16ResultX, sObjADXL345.ai16ResultY, sObjADXL345.ai16ResultZ );
					// ダメ押しでレジスタに残っているデータも読み込む
					uint8 au8data[6];
					bool_t bOk = TRUE;
					GetAxis(bOk, au8data);
				}else{
					bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0xFFFF) );
					u32Sleep = 0;
				}
				u8GetCount = 0;
			}
		}

		V_PRINTF(LB"! Sleeping... : %dms, 0x%02X"LB, u32Sleep, sObjADXL345.u8Interrupt );
		V_FLUSH();

		u8Read_Interrupt();
		ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, u32Sleep, u32Sleep>0 ? TRUE:FALSE, FALSE);
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
		vProcessADXL345_FIFO(E_EVENT_TICK_TIMER);
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
void vInitAppADXL345_FIFO() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

static void vProcessADXL345_FIFO(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_ADXL345_CMP;


		V_PRINTF( LB"!NUM = %d", sObjADXL345.u8FIFOSample );
#ifdef DEBUG
		uint8 i;
		for(i=0; i<sObjADXL345.u8FIFOSample; i++ ){
			V_PRINTF(LB"!ADXL345_%d: X : %d, Y : %d, Z : %d",
					i,
					sObjADXL345.ai16ResultX[i],
					sObjADXL345.ai16ResultY[i],
					sObjADXL345.ai16ResultZ[i]
			);
		}
#else
	V_PRINTF(LB"!ADXL345_%d: X : %d, Y : %d, Z : %d",
			0,
			sObjADXL345.ai16ResultX[0],
			sObjADXL345.ai16ResultY[0],
			sObjADXL345.ai16ResultZ[0]
	);
#endif

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
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2];
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}
