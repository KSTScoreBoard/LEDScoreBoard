/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"
#include "sprintf.h"
#include "ccitt8.h"

#include "Interactive.h"
#include "EndDevice_Input.h"
#include "ProcessEv_MultiSensor.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessMultiSensor(teEvent eEvent);

static void vSensorInfoReg();

static uint32 u32sns_cmplt = 0;
static tsSnsObjAll asSnsObjAll[MAX_SNS];

static uint32 u32SnsMap;
static uint32 u32SnsAllCmp;
static uint8 u8SnsNum;

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_PRIMARY_CMP = 2,
	E_SNS_SECONDARY_CMP = 4,
	E_SNS_ALL_CMP = 7
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
			V_FLUSH();
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

			uint8 i = 0;
			while( i < sizeof(sAppData.sFlash.sData.uParam.au8Param) ){		// 最大16桁の16進数
				if( sAppData.sFlash.sData.uParam.au8Param[i] == '\0' ){
					break;
				}
				i++;
			}
			// 8桁以上あったら8桁まで使用する
			if( i > 8 ){
				i = 8;
			}

			// 使用するセンサの読み込み
			u32SnsMap = u32string2hex(sAppData.sFlash.sData.uParam.au8Param, i);

			// すべて完了したときのステータス
			u32SnsAllCmp = (u32SnsMap<<1) + 1;

			vSensorInfoReg();
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u32sns_cmplt = 0;

		uint8 i;
		V_PRINTF( LB );
		for( i=0; i<MAX_SNS; i++){
			if( asSnsObjAll[i].bSnsEnable ){
				// センサに初期設定が必要であれば設定を行う
				switch(asSnsObjAll[i].u8SnsName){
					case PKT_ID_SHT21:
						vSHT21_Init( (tsObjData_SHT21*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_ADT7410:
						vADT7410_Init( (tsObjData_ADT7410*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_MPL115A2:
						vMPL115A2_Init( (tsObjData_MPL115A2*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_LIS3DH:
						vLIS3DH_Init( (tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_ADXL345:
						vADXL345_Init( (tsObjData_ADXL345*)(asSnsObjAll[i].tsObjData), &(asSnsObjAll[i].sSnsObj) );
						if( bFirst ){
							bADXL345reset();
							bADXL345_Setting( 0, sAppData.sFlash.sData.uParam.sADXL345Param, FALSE );
						}
						break;
					case PKT_ID_TSL2561:
						vTSL2561_Init( (tsObjData_TSL2561*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_L3GD20:
						vL3GD20_Init( &sObjL3GD20, &(asSnsObjAll[i].sSnsObj), 2 );
						break;
					case PKT_ID_S1105902:
						vS1105902_Init( (tsObjData_S1105902*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_BME280:
						vBME280_Init( (tsObjData_BME280*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						if( bFirst ){
							bBME280_Setting();
						}
						break;
					case PKT_ID_SHT31:
						vSHT31_Init( (tsObjData_SHT31*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					case PKT_ID_SHTC3:
						vSHTC3_Init( (tsObjData_SHTC3*)asSnsObjAll[i].tsObjData, &(asSnsObjAll[i].sSnsObj) );
						break;
					default:
						break;
				}

				vSnsObj_Process(&asSnsObjAll[i].sSnsObj, E_ORDER_KICK);
				if (bSnsObj_isComplete(&(asSnsObjAll[i].sSnsObj))) {
					// 即座に完了した時はセンサーが接続されていない、通信エラー等
					u32sns_cmplt |= (1UL<<(i+1));
					V_PRINTF(LB "*** 0x%02X comm err?" , asSnsObjAll[i].u8SnsName );
					V_FLUSH();
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
					return;
				}
			}
		}

		bFirst = FALSE;

		// ADC の取得
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

//		for( i=0; i<MAX_SNS; i++){
//			V_PRINTF( "%d ", asSnsObjAll[i].bSnsEnable );
//		}

		// RUNNING 状態
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	//V_PRINTF( LB"RUNNING : %08X/%08X",u32sns_cmplt, u32SnsAllCmp );
	//V_FLUSH();
	// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		V_FLUSH();
		vProcessMultiSensor(E_EVENT_START_UP);
	}

	// ２回スリープすると完了
	if (u32sns_cmplt != u32SnsAllCmp && (u32sns_cmplt & E_SNS_ADC_CMP_MASK)) {
		// ADC 完了後、この状態が来たらスリープする
		pEv->bKeepStateOnSetAll = TRUE; // スリープ復帰の状態を維持

		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)
		vAHI_DioWakeEnable(0, 0);

		// スリープを行うが、WAKE_TIMER_0 は定周期スリープのためにカウントを続けているため
		// 空いている WAKE_TIMER_1 を利用する
		ToCoNet_vSleep(E_AHI_WAKE_TIMER_1, 20, FALSE, FALSE); // PERIODIC RAM OFF SLEEP USING WK1
	}

	// 送信処理に移行
	if (u32sns_cmplt == u32SnsAllCmp) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) {
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

		uint8	au8Data[100];
		uint8*	q = au8Data;
		S_OCTET(sAppData.sSns.u8Batt);
		S_BE_WORD(sAppData.sSns.u16Adc1);
		S_BE_WORD(sAppData.sSns.u16Adc2);
		S_OCTET(u8SnsNum);

		uint8 i;
		for( i=0; i<MAX_SNS; i++){
			if( asSnsObjAll[i].bSnsEnable ){
				S_OCTET(asSnsObjAll[i].u8SnsName);
				// センサに初期設定が必要であれば設定を行う
				switch(asSnsObjAll[i].u8SnsName){
					case PKT_ID_SHT21:
						S_BE_WORD(((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_TEMP]);
						S_BE_WORD(((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_HUMID]);
						break;
					case PKT_ID_ADT7410:
						S_BE_WORD(((tsObjData_ADT7410*)asSnsObjAll[i].tsObjData)->i16Result);
						break;
					case PKT_ID_MPL115A2:
						S_BE_WORD(((tsObjData_MPL115A2*)asSnsObjAll[i].tsObjData)->i16Result);
						break;
					case PKT_ID_LIS3DH:
						S_BE_WORD(((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_X]);
						S_BE_WORD(((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_Y]);
						S_BE_WORD(((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_Z]);
						break;
					case PKT_ID_ADXL345:
						S_OCTET(0x00);
						S_BE_WORD(((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_X]);
						S_BE_WORD(((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_Y]);
						S_BE_WORD(((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_Z]);
						break;
					case PKT_ID_TSL2561:
						S_BE_DWORD(((tsObjData_TSL2561*)asSnsObjAll[i].tsObjData)->u32Result );
						break;
					case PKT_ID_L3GD20:
						S_BE_WORD(((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_X]);
						S_BE_WORD(((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_Y]);
						S_BE_WORD(((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_Z]);
						break;
					case PKT_ID_S1105902:
						S_BE_WORD(((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_R]);
						S_BE_WORD(((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_G]);
						S_BE_WORD(((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_B]);
						S_BE_WORD(((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_I]);
						break;
					case PKT_ID_BME280:
						S_BE_WORD(((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->i16Temp);
						S_BE_WORD(((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->u16Hum);
						S_BE_WORD(((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->u16Pres );
						break;
					case PKT_ID_SHT31:
						S_BE_WORD(((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_TEMP]);
						S_BE_WORD(((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_HUMID]);
						break;
					case PKT_ID_SHTC3:
						S_BE_WORD(((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_TEMP]);
						S_BE_WORD(((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_HUMID]);
						break;
					default:
						break;
				}
			}
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
		V_PRINTF(LB"! Sleeping...");
		V_FLUSH();

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE
		ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, sAppData.sFlash.sData.u32Slp, sAppData.u16frame_count == 1 ? FALSE : TRUE, FALSE);
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
		vProcessMultiSensor(E_EVENT_TICK_TIMER);
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			u32sns_cmplt |= E_SNS_ADC_CMP_MASK;
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
void vInitAppMultiSensor() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

static void vProcessMultiSensor( teEvent eEvent) {
	uint8 i = 0;
	uint8 u8count = 0;
	for( i=0; i<MAX_SNS; i++ ){
		if (bSnsObj_isComplete(&asSnsObjAll[i].sSnsObj) && asSnsObjAll[i].bSnsEnable ){
			u8count++;
		}
	}
	if( u8count == u8SnsNum ){
		return;
	}

	for( i=0; i<MAX_SNS; i++ ){
		// イベントの処理
		if(asSnsObjAll[i].bSnsEnable){
			vSnsObj_Process(&(asSnsObjAll[i].sSnsObj) , eEvent); // ポーリングの時間待ち
			if (bSnsObj_isComplete(&asSnsObjAll[i].sSnsObj) ) {
				if( !( u32sns_cmplt&( 1UL<<(i+1) ) ) ){
					switch( asSnsObjAll[i].u8SnsName ){
					case PKT_ID_SHT21:
						V_PRINTF(LB"!SHT21: %d.%02dC %d.%02d%%",
								((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_TEMP] / 100,
								((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_TEMP] % 100,
								((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_HUMID] / 100,
								((tsObjData_SHT21*)asSnsObjAll[i].tsObjData)->ai16Result[SHT21_IDX_HUMID] % 100
						);
						break;
					case PKT_ID_ADT7410:
						V_PRINTF(LB"!ADT7410: %d.%02dC",
								((tsObjData_ADT7410*)asSnsObjAll[i].tsObjData)->i16Result / 100,
								((tsObjData_ADT7410*)asSnsObjAll[i].tsObjData)->i16Result % 100
						);
						break;
					case PKT_ID_MPL115A2:
						V_PRINTF(LB"!MPL115A2: %dhPa", ((tsObjData_MPL115A2*)asSnsObjAll[i].tsObjData)->i16Result );
						break;
					case PKT_ID_LIS3DH:
						V_PRINTF(LB"!LIS3DH: X : %d, Y : %d, Z : %d",
								((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_X],
								((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_Y],
								((tsObjData_LIS3DH*)asSnsObjAll[i].tsObjData)->ai16Result[LIS3DH_IDX_Z]
						);
						break;
					case PKT_ID_ADXL345:
						V_PRINTF(LB"!ADXL345: X : %d, Y : %d, Z : %d",
								((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_X],
								((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_Y],
								((tsObjData_ADXL345*)asSnsObjAll[i].tsObjData)->ai16Result[ADXL345_IDX_Z]
						);
						break;
					case PKT_ID_TSL2561:
						V_PRINTF(LB"!TSL2561: %dC", ((tsObjData_TSL2561*)asSnsObjAll[i].tsObjData)->u32Result );
						break;
					case PKT_ID_L3GD20:
						V_PRINTF(LB"!L3GD20: X : %d, Y : %d, Z : %d",
								((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_X],
								((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_Y],
								((tsObjData_L3GD20*)asSnsObjAll[i].tsObjData)->ai16Result[L3GD20_IDX_Z]
						);
						break;
					case PKT_ID_S1105902:
						V_PRINTF(LB"!S1105902: R : %d, G : %d, B : %d, I : %d",
								((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_R],
								((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_G],
								((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_B],
								((tsObjData_S1105902*)asSnsObjAll[i].tsObjData)->au16Result[S1105902_IDX_I]
						);
						break;
					case PKT_ID_BME280:
						V_PRINTF(LB"!BME280: %d.%02dC %d.%02d%% %dhPa",
								((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->i16Temp/100,
								((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->i16Temp%100,
								((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->u16Hum/100,
								((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->u16Hum%100,
								((tsObjData_BME280*)asSnsObjAll[i].tsObjData)->u16Pres );
						break;
					case PKT_ID_SHT31:
						V_PRINTF(LB"!SHT31: %d.%02dC %d.%02d%%",
								((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_TEMP] / 100,
								((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_TEMP] % 100,
								((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_HUMID] / 100,
								((tsObjData_SHT31*)asSnsObjAll[i].tsObjData)->ai16Result[SHT31_IDX_HUMID] % 100
						);
						break;
					case PKT_ID_SHTC3:
						V_PRINTF(LB"!SHTC3: %d.%02dC %d.%02d%%",
								((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_TEMP] / 100,
								((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_TEMP] % 100,
								((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_HUMID] / 100,
								((tsObjData_SHTC3*)asSnsObjAll[i].tsObjData)->ai16Result[SHTC3_IDX_HUMID] % 100
						);
						break;
					default:
						break;
					}
					V_FLUSH();
				}
				u32sns_cmplt |= ( 1UL<<(i+1) );
			}
		}
	}

	for( i=0; i<MAX_SNS; i++ ){
		if (bSnsObj_isComplete(&asSnsObjAll[i].sSnsObj) && asSnsObjAll[i].bSnsEnable ){
			u8count++;
		}
	}
	if( u8count == u8SnsNum ){
		// 完了時の処理
		if (u32sns_cmplt == u32SnsAllCmp) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[u8ADCPort[0]];
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[u8ADCPort[1]];
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}

static void vSensorInfoReg()
{
	uint8 i;
	for( i=0; i<MAX_SNS; i++ ){
		asSnsObjAll[i].u8SnsName = 0x30+(i+1);
		asSnsObjAll[i].bSnsEnable = ( (1UL<<i)&u32SnsMap ) ? TRUE : FALSE;

		if(asSnsObjAll[i].bSnsEnable){
			// 使用するセンサの数を数える
			u8SnsNum++;

			memset( &asSnsObjAll[i].sSnsObj, 0, sizeof(asSnsObjAll[i].sSnsObj) );

			switch( i ){
			case 0:
				asSnsObjAll[i].tsObjData = (void*)&sObjSHT21;
				break;
			case 1:
				asSnsObjAll[i].tsObjData = (void*)&sObjADT7410;
				break;
			case 2:
				asSnsObjAll[i].tsObjData = (void*)&sObjMPL115A2;
				break;
			case 3:
				asSnsObjAll[i].tsObjData = (void*)&sObjLIS3DH;
				break;
			case 4:
				asSnsObjAll[i].tsObjData = (void*)&sObjADXL345;
				break;
			case 5:
				asSnsObjAll[i].tsObjData = (void*)&sObjTSL2561;
				break;
			case 6:
				asSnsObjAll[i].tsObjData = (void*)&sObjL3GD20;
				break;
			case 7:
				asSnsObjAll[i].tsObjData = (void*)&sObjS1105902;
				break;
			case 8:
				asSnsObjAll[i].tsObjData = (void*)&sObjBME280;
				break;
			case 9:
				asSnsObjAll[i].tsObjData = (void*)&sObjSHT31;
				break;
			case 10:
				asSnsObjAll[i].tsObjData = (void*)&sObjSHTC3;
				break;
			default:
				break;
			}
		}
	}
}
