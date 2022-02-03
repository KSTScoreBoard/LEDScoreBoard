/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();

#define END_INPUT 0
#define END_ADC 1
#define END_TX 2

uint8 DI_Bitmap = 0;

/*
 * 最初に遷移してくる状態
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	//	この状態から始まったときモジュールIDなどを表示する
	if (eEvent == E_EVENT_START_UP) {
		// 起床メッセージ
		vSerInitMessage();

		if( IS_ENABLE_WDT() ){
			// WDTにパルスを送る
			vPortSetHi(3);
		}

		//	初回起動(リセット)かスリープからの復帰かで表示するメッセージを変える
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");

			// RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);

			if(sAppData.bWakeupByButton){
				// 入力待ち状態へ遷移
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_INPUT);
			}else{
				if( IS_TIMER_MODE() ){
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_INPUT);
				}else{
					vPortSetSns(FALSE);
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				}
			}
		} else {
			// 開始する
			// start up message
			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			// ネットワークの初期化
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);

			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			}

			// 最初にパケットを送りたくないのでチャタリング対策状態へ遷移後、割り込みがあるまでスリープ
			if( IS_SWING_MODE() ){		//	スリープしない場合
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_INPUT);
			}else{
				vPortSetSns(FALSE);
				ToCoNet_Event_SetState(pEv, E_STATE_APP_CHAT_SLEEP);
			}
		}

	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_INPUT, tsEvent *pEv, teEvent eEvent, uint32 u32evarg){
	bool_t bBtnSet = FALSE;
	if( IS_INPUT_TIMER() ){
		//V_PRINTF(LB"! TIMER");
		if ( eEvent == E_ORDER_KICK && u32evarg == END_INPUT ) { // キックされたのでDIの状態が確定した
			V_PRINTF(LB"! TIMER: %d, %d", ToCoNet_Event_u32TickFrNewState(pEv), u32evarg);

			if( (((sAppData.sFlash.sData.i16param&0x00FF) == 0x00) && DI_Bitmap ) ||	// どこかのピンがLoだったら
				( IS_INVERSE_INT() && (DI_Bitmap&u32InputMask) != u32InputMask ) ||	// どこかのピンがHiだったら
				( IS_DBLEDGE_INT() ) ||							// 両方のエッジ
				( IS_SWING_MODE() )) {							// SWING
				bBtnSet = TRUE;
			}
		}
	}else{
		DI_Bitmap = bPortRead(DIO_BUTTON) ? 0x01 : 0x00;
		if( IS_MULTI_INPUT() ){
			DI_Bitmap |= bPortRead(PORT_INPUT2) ? 0x02 : 0x00;
			DI_Bitmap |= bPortRead(PORT_INPUT3) ? 0x04 : 0x00;
			DI_Bitmap |= bPortRead(PORT_SDA) ? 0x08 : 0x00;
		}
		bBtnSet = TRUE;
	}

	if( bBtnSet ){
		if( IS_SWING_MODE() ){
			vPortDisablePullup(DIO_BUTTON);
		}
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}

	// タイムアウトの場合はノイズだと思ってスリープする
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_INPUT)");
		//V_FLUSH();
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/*	パケットを送信する状態	*/
PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// ADC の開始
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}
	if (eEvent == E_ORDER_KICK && u32evarg == END_ADC ) {
		uint8*	q;
		bool_t bOk = FALSE;
		if( IS_APPCONF_OPT_APP_TWELITE() ){		//超簡単!TWEアプリあてに送信する場合
			uint8	au8Data[7];
			q = au8Data;

			// DIO の設定
			S_OCTET(DI_Bitmap);
			S_OCTET(0x0F);

			// PWM(AI)の設定
			uint8 u8MSB = (sAppData.sSns.u16Adc1 >> 2) & 0xFF;
			S_OCTET(u8MSB);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);

			// 下2bitを u8LSBs に詰める
			uint8 u8LSBs = sAppData.sSns.u16Adc1|0x03;
			S_OCTET(u8LSBs);

			bOk = bTransmitToAppTwelite( au8Data, q-au8Data );
		}else{									// 無線タグアプリ宛に送信する場合
			uint16 u16RandNum = ToCoNet_u16GetRand();
			/*	DIの入力状態を取得	*/
			uint8	au8Data[7];
			q = au8Data;
			S_OCTET(sAppData.sSns.u8Batt);
			S_BE_WORD(sAppData.sSns.u16Adc1);
			if( IS_SWING_MODE() ){
				S_BE_WORD(u16RandNum);
			}else{
				S_BE_WORD(sAppData.sSns.u16Adc2);
			}

			//	立ち上がりで起動 or 立ち下がりで起動
			uint16 u16mode = sAppData.sFlash.sData.i16param&0xFF;
			if( !IS_SWING_MODE() ){
				u16mode += sAppData.bWakeupByButton?0x00:0x80;
			}

			S_OCTET(u16mode);

			/*	DIの入力状態を取得	*/
			S_OCTET( DI_Bitmap );

			bOk = bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
		}

		sAppData.u16frame_count++;

		if ( bOk ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
			V_PRINTF(LB"TxOk");
			if( IS_SWING_MODE() ){
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF);
			}else{
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
			}
		} else {
			V_PRINTF(LB"TxFl");
			if( IS_SWING_MODE() ){
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_POWEROFF);
			}else{
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
			}
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}
}

/*	送信完了状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_ORDER_KICK  && u32evarg == END_TX ) { // 送信完了イベントが来たのでスリープする
		if( IS_DBLEDGE_INT() ){
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // チャタリング無視
		}else{
			ToCoNet_Event_SetState(pEv, E_STATE_APP_CHAT_SLEEP); // スリープ状態へ遷移
		}
	}
}

/*
 * 送信後のチャタリング対策を行う状態
 */
PRSEV_HANDLER_DEF(E_STATE_APP_CHAT_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	/*	遷移してきたとき一旦眠る	*/
	if(eEvent == E_EVENT_NEW_STATE){
		V_PRINTF(LB"Safe Chattering...");
		V_FLUSH();
		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		ToCoNet_Nwk_bPause(sAppData.pContextNwk);

		//	割り込み禁止でスリープ
		pEv->bKeepStateOnSetAll = TRUE;		//	この状態から起床
		vAHI_DioWakeEnable(0, u32InputMask|u32InputSubMask); // DISABLE DIO WAKE SOURCE

		if( IS_ENABLE_WDT() ){
			// WDTにパルスを送る
			vPortSetLo(3);
		}

		ToCoNet_vSleep(E_AHI_WAKE_TIMER_1, 200UL, FALSE, FALSE);
	/*	起床後すぐこの状態になったときずっと眠る状態へ	*/
	}else if(eEvent == E_EVENT_START_UP){
		pEv->bKeepStateOnSetAll = FALSE;
		vPortSetSns(FALSE);
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/*	スリープをする状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"Sleeping...");
		V_FLUSH();

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 20 || !IS_ENABLE_WDT() ) {
		// print message.
		vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

		// set UART Rx port as interrupt source
		vAHI_DioSetDirection( u32InputMask|u32InputSubMask, 0); // set as input

		(void)u32AHI_DioInterruptStatus(); // clear interrupt register


		if( IS_DBLEDGE_INT() ){
			vAHI_DioWakeEnable( u32InputMask|u32InputSubMask, 0); // also use as DIO WAKE SOURCE
			vAHI_DioWakeEdge( u32InputSubMask, u32InputMask ); // 割り込みエッジ（立上がりに設定）
		}else if( IS_INVERSE_INT() ){
			vAHI_DioWakeEnable( u32InputMask, 0 ); // also use as DIO WAKE SOURCE
			vAHI_DioWakeEdge( u32InputMask, 0 ); // 割り込みエッジ（立上がりに設定）
		} else {
			vAHI_DioWakeEnable( u32InputMask, 0 ); // also use as DIO WAKE SOURCE
			vAHI_DioWakeEdge( 0, u32InputMask ); // 割り込みエッジ（立下りに設定）
		}

		uint32 u32Sleep = 0;

		if( IS_ENABLE_WDT() ){
			// WDTにパルスを送る
			vPortSetLo(3);
			u32Sleep = sAppData.sFlash.sData.u32Slp;
		}else if( IS_TIMER_MODE() ){
			u32Sleep = sAppData.sFlash.sData.u32Slp;
		}

		// wake up using wakeup timer as well.
		ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32Sleep, TRUE, FALSE ); // PERIODIC RAM OFF SLEEP USING WK0
	}
}

/*	電池切れを待つ	*/
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_POWEROFF, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"Complete!!");
		V_FLUSH();
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
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_SLEEP),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_CHAT_SLEEP),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_POWEROFF),
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
		if( sAppData.sFlash.sData.i16param&0x200 ){
			// ボタンハンドラの駆動
			if (sAppData.pr_BTM_handler) {
				// ハンドラを稼働させる
				(*sAppData.pr_BTM_handler)(1000/sToCoNet_AppContext.u16TickHz);
			}
		}
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
		// ボタンの判定を行う。
		if( IS_INPUT_TIMER() ){
			uint32 bmPorts, bmChanged;
			if (bBTM_GetState(&bmPorts, &bmChanged)) {
				DI_Bitmap = (bmPorts&(1UL<<PORT_INPUT1)) ? 0x01 : 0x00;
				DI_Bitmap |= (bmPorts&(1UL<<PORT_INPUT2)) ? 0x02 : 0x00;
				DI_Bitmap |= (bmPorts&(1UL<<PORT_INPUT3)) ? 0x04 : 0x00;
				DI_Bitmap |= (bmPorts&(1UL<<PORT_SDA)) ? 0x08 : 0x00;
				ToCoNet_Event_Process(E_ORDER_KICK, END_INPUT, vProcessEvCore);
			}
		}
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
			ToCoNet_Event_Process(E_ORDER_KICK, END_ADC, vProcessEvCore);
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
void vInitAppButton() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
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
