/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bWaiting = FALSE;

	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {

			// 起動時の待ち処理
			// (Resume 時に MAC 層の初期化が行われるため電流が流れる。その前にスリープ)
			if (sAppData.sFlash.sData.u8wait) {
				if (bWaiting == FALSE) {
					// Warm start message
					V_PRINTF( LB LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
					V_FLUSH();

					bWaiting = TRUE;
					ToCoNet_vSleep( E_AHI_WAKE_TIMER_1, sAppData.sFlash.sData.u8wait, FALSE, FALSE);
					return;
				} else {
					V_PRINTF( LB"End Sensor Wait Duration." );
					bWaiting = FALSE;
				}
			}else{
				V_PRINTF( LB LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
			}

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

			// 起動時の待ち処理
			//   いったん Init と Start を行っておく
			if (sAppData.sFlash.sData.u8wait && bWaiting == FALSE) {
				bWaiting = TRUE;
				ToCoNet_vSleep(E_AHI_WAKE_TIMER_1, sAppData.sFlash.sData.u8wait, FALSE, FALSE);
				return;
			}

			// RUNNING状態へ遷移
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// ADC の開始
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}
	if (eEvent == E_ORDER_KICK) {
		bool_t bOk = FALSE;
		if( IS_APPCONF_OPT_APP_TWELITE() ){		//超簡単!TWEアプリあてに送信する場合
			uint8	au8Data[7];
			uint8* q = au8Data;

			// DIO の設定
			uint8 DI_Bitmap = (sAppData.sSns.u16Adc1 > 800 ? 0x08:0x00)
							+ (sAppData.sSns.u16Adc1 > 600 ? 0x04:0x00)
							+ (sAppData.sSns.u16Adc1 > 400 ? 0x02:0x00)
							+ (sAppData.sSns.u16Adc1 > 200 ? 0x01:0x00);
			S_OCTET(DI_Bitmap);
			S_OCTET(0x0F);

			// PWM(AI)の設定
			uint16 u16v = sAppData.sSns.u16Adc1 >> 2;
			uint8 u8MSB = (u16v>>2)&0xff;
			// 下2bitを u8LSBs に詰める
			uint8 u8LSBs = u16v|0x03;
			S_OCTET(u8MSB);

			u16v = sAppData.sSns.u16Adc3 >> 2;
			u8MSB = (u16v>>2)&0xff;
			u8LSBs |= (u16v|0x03)<<2;
			S_OCTET(u8MSB);

			u16v = sAppData.sSns.u16Adc2 >> 2;
			u8MSB = (u16v>>2)&0xff;
			u8LSBs |= (u16v|0x03)<<4;
			S_OCTET(u8MSB);

			u16v = sAppData.sSns.u16Adc4 >> 2;
			u8MSB = (u16v>>2)&0xff;
			u8LSBs |= (u16v|0x03)<<6;
			S_OCTET(u8MSB);
			S_OCTET(u8LSBs);

			bOk = bTransmitToAppTwelite( au8Data, q-au8Data );
		}else{
			uint8 au8Data[9];
			uint8 *q =  au8Data;

			S_OCTET(sAppData.sSns.u8Batt);			// 電源電圧
			if( sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD && (sAppData.sFlash.sData.i16param&0x0001) ){
				sAppData.sSns.u16Adc1 |= 0x8000; 		// ADCを4ポート使用してますよビット
				S_BE_WORD(sAppData.sSns.u16Adc1);
				S_BE_WORD(sAppData.sSns.u16Adc3);
				S_BE_WORD(sAppData.sSns.u16Adc2);
				S_BE_WORD(sAppData.sSns.u16Adc4);
			}else{
				S_BE_WORD(sAppData.sSns.u16Adc1);
				S_BE_WORD(sAppData.sSns.u16Adc2);
				S_BE_WORD(sAppData.sSns.u16PC1);
				S_BE_WORD(sAppData.sSns.u16PC2);
			}

			//	LM61を使う場合
			uint16 bias=0;
			if( sAppData.sFlash.sData.u8mode == 0x11 ){
				bias = sAppData.sFlash.sData.i16param;

				S_BE_WORD( bias );		//	バイアス
				//V_PRINTF(LB"%d", bias);
			}

			bOk = bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
		}

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

		// 周期スリープに入る
		//  - 初回は５秒あけて、次回以降はスリープ復帰を基点に５秒
		vSleep(sAppData.sFlash.sData.u32Slp, sAppData.u16frame_count == 1 ? FALSE : TRUE, FALSE);
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
void vInitAppStandard() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}


/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
#ifndef SWING
	if( !(sAppData.sFlash.sData.i16param&0x0001) ){
		// パルス数の読み込み
		bAHI_Read16BitCounter(E_AHI_PC_0, &sAppData.sSns.u16PC1); // 16bitの場合
		// パルス数のクリア
		bAHI_Clear16BitPulseCounter(E_AHI_PC_0); // 16bitの場合

		// パルス数の読み込み
		bAHI_Read16BitCounter(E_AHI_PC_1, &sAppData.sSns.u16PC2); // 16bitの場合
		// パルス数のクリア
		bAHI_Clear16BitPulseCounter(E_AHI_PC_1); // 16bitの場合
	}else{
		sAppData.sSns.u16PC1 = 0;
		sAppData.sSns.u16PC2 = 0;
	}
#else
	sAppData.sSns.u16PC1 = 0;
	sAppData.sSns.u16PC2 = 0;
#endif

	// センサー値の保管
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[u8ADCPort[0]];
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[u8ADCPort[1]];
	if( sAppData.sFlash.sData.i16param&0x0001 ){
		sAppData.sSns.u16Adc3 = sAppData.sObjADC.ai16Result[u8ADCPort[2]];
		sAppData.sSns.u16Adc4 = sAppData.sObjADC.ai16Result[u8ADCPort[3]];
	}else{
		sAppData.sSns.u16Adc3 = 0;
		sAppData.sSns.u16Adc4 = 0;
	}

	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}
