/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// 詳細は remote_config.h を参照

#include <jendefs.h>

#include "utils.h"

#include "Interactive.h"
#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "config.h"
#include "remote_config.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static bool_t bTranmitRequest();
static bool_t bTranmitAck(bool_t bStat);

static uint8 u8RemoteConf = 0;
static uint8 u8Flags = 0;

/* *********************************/
/* * 状態マシン                    */
/* *********************************/

/*
 * 設定処理用の状態マシン
 *
 * 設定マスター：
 *   - 起床後は、デバイス側からの要求待ちを行う。
 *
 * 設定対象：
 *   - 起床後 100ms は受信回路をオープン。設定要求リクエストを行う。
 */

/**
 * アイドル状態。
 *
 * @param E_STATE_IDLE
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		Interactive_vSetMode(TRUE,0);
		vSerInitMessage();

		if( IS_APPCONF_OPT_DISABLE_OTA() ){
			vfPrintf(&sSerStream, LB LB "*** Entering Config Mode ***");
			//ToCoNet_Event_SetState(pEv, E_STATE_APP_INTERACTIVE);
			return;
		}		
		vfPrintf(&sSerStream, LB LB "*** OTA START ***");
		// vfPrintf(&sSerStream, "%d", sizeof(tsFlashApp)); // 1.4.1 で 60 バイト

		sAppData.sFlash.sData.u16RcClock = ToCoNet_u16RcCalib(0);

		// LED
		sAppData.u8LedState = 1; // ON

		// Mac 層の開始
		ToCoNet_vMacStart();

		ToCoNet_Event_SetState(pEv, E_STATE_WAIT_COMMAND);
	}

	if (ToCoNet_Event_u32TickFrNewState(pEv) > 1000) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_INTERACTIVE);
	}
}

/**
 * 設定パケットを要求する。
 * 100ms 経過したら受信回路をクローズする。
 *
 * @param E_STATE_WAIT_COMMAND
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_WAIT_COMMAND, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// リクエストパケットを送信する
		if (!bTranmitRequest()) {
			u8Flags = 0x00;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_FAILED); // 失敗時は FAILED 状態に遷移
		}
		else{
			u8Flags = 0x01;
		}
	}

	// 60ms 経過後はRx回路を停止させる（通常の入力コマンドインタフェースに移行
	if (PRSEV_u32TickFrNewState(pEv) > 60 || (u8Flags&0x02) != 0 ) {
		// 通常モードへ戻る
		if(( u8Flags&0x02) != 0 ){
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}else{
			//V_PRINTF( LB"!TIME OUT RX" );
			V_PRINTF( LB"!INF:OTA TIME OUT" );
			ToCoNet_Event_SetState(pEv, E_STATE_APP_FAILED);	// 失敗時は FAILED 状態に遷移
		}
	}
}

/**
 * 通常のインタラクティブモード
 *
 * @param E_STATE_RUNNING
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// RX 停止
		sToCoNet_AppContext.bRxOnIdle = FALSE;
		ToCoNet_vRfConfig();
	}

	if (u8RemoteConf) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
	}
	else
	if(PRSEV_u32TickFrNewState(pEv) > 50){					// タイムアウト
		V_PRINTF( LB"!TIME OUT ACK" );
		ToCoNet_Event_SetState(pEv, E_STATE_APP_FAILED);	// 失敗時は FAILED 状態に遷移
	}
}

/**
 * 通信エラーがあったときの処理
 *
 * @param E_STATE_FAILED
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_FAILED, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		if( bPortRead(INPUT_SET) ){	//	インタラクティブモード
			V_PRINTF(LB LB "*** Entering Config Mode ***");
		}
		else{							//	スリープ
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}
	}

	if(PRSEV_u32TickFrNewState(pEv) > 1000){					// タイムアウト
		ToCoNet_Event_SetState(pEv, E_STATE_APP_INTERACTIVE);
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_INTERACTIVE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		sAppData.u8LedState = 0;		// LED消灯
		Config_vUpdateScreen();
	}
}

/**
 * 通常のインタラクティブモード
 *
 * @param E_STATE_RUNNING
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		sAppData.u8LedState = 0;		// LED消灯
		if( u8Flags == 7 ){
			Config_vUpdateScreen();
			V_PRINTF("!INF:CONFIG DATA WAS UPDATED AS ABOVE. RESTART THE SYSTEM. %02X", u8Flags);
		}else if( u8Flags&0x02 ){
			V_PRINTF(LB"!ACK WAS NOT RETURNED. PLEASE RESTART. %02X", u8Flags);
			V_FLUSH();
		}else{
			V_PRINTF(LB"!INF:OTA SKIPPED. START NORMALLY. %02X", u8Flags);
			V_FLUSH();
			vSleep(100, FALSE, TRUE);
		}
	}

	//	設定親機がいない、設定が完了した以外はLEDを点灯させて永久にスリープ
	if( u8Flags != 1 && u8Flags != 7 ){
		sAppData.u8LedState = 1;
		LED_ON();
		V_PRINTF("!INF: ETERNAL SLEEP %02X", u8Flags);
		V_FLUSH();
		vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE
		ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, 0, FALSE, FALSE);
	}

	if (PRSEV_u32TickFrNewState(pEv) > 100 && u8RemoteConf != 0xFF) {
		// スリープ遷移
		u8RemoteConf = 0xFF;
		vSleep(100, FALSE, TRUE);
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_WAIT_COMMAND),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_FAILED),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_INTERACTIVE),
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

/* *********************************/
/* * ToCoNet コールバック          */
/* *********************************/

/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}

/**
 * ハードイベント（遅延割り込み処理）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bLed = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		// LED の点滅を管理する
		if (sAppData.u8LedState == 1 || ((u32TickCount_ms >> 8) & 1)) {
			bLed = TRUE;
		}
		vPortSet_TrueAsLo(OUTPUT_LED, bLed);

		// WDTの制御
		if( (u32TickCount_ms&0x0000FFFF) == 0x8000 ){
			vPortSetHi(WDT_OUT);
		}else{
			vPortSetLo(WDT_OUT);
		}


		break;

	default:
		break;
	}
}

/**
 * 受信イベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {
	int i;

	if (IS_APPCONF_OPT_VERBOSE()) {
		V_PRINTF(LB"RxPkt: Sr:%08X De:%08X Lq:%03d Ln:%02d Cm:%d Sq:%02x [",
				pRx->u32SrcAddr,
				pRx->u32DstAddr,
				pRx->u8Lqi,
				pRx->u8Len,
				pRx->u8Cmd,
				pRx->u8Seq
		);
		for (i = 0; i < 16 && i < pRx->u8Len; i++) {
			V_PRINTF("%02X", pRx->auData[i]);
		}
		if (pRx->u8Len > i) {
			V_PRINTF("..");
		}
		V_PUTCHAR(']');
	}

	uint8 *p = pRx->auData;

	if (pRx->u8Cmd == RMTCNF_PKTCMD) {
		// *   OCTET    : パケットバージョン (1)
		uint8 u8pktver = G_OCTET();
		if (u8pktver != RMTCNF_PRTCL_VERSION) {
			V_PRINTF(LB"!PRTCL_VERSION");
			V_PRINTF(" : %08X PKTVER %d", pRx->u32SrcAddr, u8pktver );
			return;
		}

		// *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
		uint8 u8pkttyp = G_OCTET();
		if (u8pkttyp != RMTCNF_PKTTYPE_DATA) {
			V_PRINTF(LB"!PKTTYPE_DAT");
			return;
		}

		// *   OCTET(4) : アプリケーションバージョン
		uint32 u32ver = G_BE_DWORD();
		if (u32ver != VERSION_U32) {
			V_PRINTF(LB"!VERSION_U32");
			V_PRINTF(" : %08X VER %d.%d.%d", pRx->u32SrcAddr, (u32ver>>16)&0xFF, (u32ver>>8)&0xFF, u32ver&0xFF );
//			return;
		}

		// *   パケット種別 = 応答
		// *   OCTET    : 設定有効化 LQI
		uint8 u8lqi = G_OCTET();
		if (pRx->u8Lqi < u8lqi) {
			V_PRINTF(LB"!LQI");
			return;
		}

		// *   OCTET    : データ形式 (0: ベタ転送, 1pkt)
		uint8 u8dattyp = G_OCTET();
		if (u8dattyp != RMTCNF_DATATYPE_RAW_SINGLE_PKT) {
			V_PRINTF(LB"!DATATYPE_RAW_SINGLE_PKT");
			return;
		}

		// *   OCTET    : データサイズ
		uint8 u8datsiz = G_OCTET();
		if (u8datsiz != sizeof(tsFlashApp)) {
			V_PRINTF(LB"!DATASIZE");
			return;
		}
		//	受信完了
		u8Flags += 0x01<<1;

		// *   OCTET(N) : 設定データ
		{
			// RX 停止(電池の無駄になるので)
			sToCoNet_AppContext.bRxOnIdle = FALSE;
			ToCoNet_vRfConfig();

			// 構造体は４バイト境界に配置しないといけないので一旦 memcpy() する。
			tsFlashApp sSave;
			memcpy(&sSave, p, u8datsiz);

			// EEPROM にセーブ
			bool_t bRet = Config_bUnSerialize(p, u8datsiz, RMTCNF_DATATYPE_RAW_SINGLE_PKT);
			sAppData.sFlash.sData.u16RcClock = ToCoNet_u16GetRcCalib();
			bRet &= Config_bSave();

			// Ack 応答
			u8Flags += bTranmitAck(bRet)<<2;
		}
	}
}

/**
 * 送信完了イベント
 * - 送信失敗時は次の手続きはないので無線受信回路をクローズする
 *
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	if (!bStatus) {
		// RX 停止(電池の無駄になるので)
		sToCoNet_AppContext.bRxOnIdle = FALSE;
		ToCoNet_vRfConfig();
	}

	// シーケンスが無事成功し完了した場合、リセットする
	if (u8CbId == RMTCNF_PKTTYPE_ACK && bStatus) {
		u8RemoteConf = 1;
	}
}

/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	cbAppToCoNet_vMain,
	NULL, //cbAppToCoNet_vNwkEvent,
	cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppCUEConfig() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
}


/* *********************************/
/* * 送信関連の手続き              */
/* *********************************/

/**
 * 送信手続き
 * @param pTx
 * @return
 */
static bool_t bTransmit(tsTxDataApp *pTx) {
	// 送信元
	pTx->u32SrcAddr = ToCoNet_u32GetSerial();

	// ルータがアプリ中では受信せず、単純に中継する
	pTx->u32DstAddr = SHORTADDR_OTA;

	//pTx->u8CbId = 0; // TxEvent で通知される番号、送信先には通知されない
	//pTx->u8Seq = 0; // シーケンス番号(送信先に通知される)

	pTx->u8Cmd = RMTCNF_PKTCMD; // 0..7 の値を取る。パケットの種別を分けたい時に使用する

	pTx->bAckReq = TRUE; // Ack
	pTx->u8Retry = 0x01; // アプリケーション再送１回

	if (ToCoNet_bMacTxReq(pTx)) {
		ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		return TRUE;
	} else {
		return FALSE;
	}
}

#define TXINIT(c) \
		tsTxDataApp c; \
		memset(&sTx, 0, sizeof(c)); \
		uint8 *q =  c.auData; \

/**
 * 設定パケット要求
 * @return
 */
static bool_t bTranmitRequest() {
	TXINIT(sTx);

	// ペイロードの準備
	//  *   OCTET    : パケットバージョン (1)
	S_OCTET(RMTCNF_PRTCL_VERSION);
	//  *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
	S_OCTET(RMTCNF_PKTTYPE_REQUEST);
	//  *   OCTET(4) : アプリケーションバージョン
	S_BE_DWORD(VERSION_U32);
	//  *   パケット種別 = 要求
	//  *   データなし
	;

	sTx.u8Len = q - sTx.auData;
	sTx.u8CbId = RMTCNF_PKTTYPE_REQUEST;

	// 送信
	return bTransmit(&sTx);
}

/**
 * 設定完了
 * @param bStat
 * @return
 */
static bool_t bTranmitAck(bool_t bStat) {
	TXINIT(sTx);

	// *   OCTET    : パケットバージョン (1)
	S_OCTET(RMTCNF_PRTCL_VERSION);
	// *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
	S_OCTET(RMTCNF_PKTTYPE_ACK);
	// *   OCTET(4) : アプリケーションバージョン
	S_BE_DWORD(VERSION_U32);
	// *   パケット種別 = ACK
	// *   OCTET    : SUCCESS(0) FAIL(1)
	S_OCTET(bStat);

	sTx.u8Len = q - sTx.auData;
	sTx.u8CbId = RMTCNF_PKTTYPE_ACK;

	// 送信
	return bTransmit(&sTx);
}
