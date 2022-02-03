/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// 詳細は remote_config.h を参照
#include <jendefs.h>
#include <AppHardwareApi.h>

#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"
#include "ToCoNet_event.h"
#include "utils.h"

#include <serial.h>
#include <string.h>
#include <fprintf.h>
#include <sprintf.h>

#include "common.h"
#include "flash.h"
#include "config.h"
#include "ccitt8.h"

#include "Pairing.h"

#define PAIR_MINLQI 150 //!< 有効なLQI値

#define PAIR_PKTCMD 3 //!< パケットのコマンド
#define PAIR_PRTCL_VERSION 1 //!< パケットバージョン

// パケット種別
#define PAIR_PKTTYPE_REQUEST 0		//!< リクエストパケット (子から親へ)
#define PAIR_PKTTYPE_DATA 1			//!< データパケット (親から子へ)
#define PAIR_PKTTYPE_ACK 2			//!< 完了パケット (子から親へ)

#define IS_NOUSE_LED(c) ( c == 0xFF )	//!< LEDを使用しない設定か否かを判断する

#define IS_PARENT() ( psPairingConf.u8PairingMode == 0x01 )		//!< 親機モードであるか判断する
#define IS_CHILD() ( psPairingConf.u8PairingMode == 0x02 )		//!< 子機モードであるか判断する

#define V_PRINTF(...) if( !psPairingConf.bDisableUART ) vfPrintf( &psPairingConf.sSerStream,__VA_ARGS__)	//!< VERBOSE モード時の printf 出力

/** @ingroup MASTER
 * アプリケーション内で使用する状態名
 */
typedef enum
{
	E_STATE_APP_BASE = ToCoNet_STATE_APP_BASE,	//!< ToCoNet 組み込み状態と重複させないため
	E_STATE_APP_ONEMORE_TX,						//!< 子機モードの時に複数回送信するためのモード
	E_STATE_APP_LISTEN,							//!< 待機状態
	E_STATE_APP_COMP,							//!< ペアリング終了状態
	E_STATE_APP_WAIT_ACK,						//!< ACK待ち状態

} teStateApp;

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static bool_t bTransmit(tsTxDataApp *pTx);
static bool_t bTranmitRequest();
static bool_t bTranmitRespond( uint32 u32AddrDst );
static bool_t bTranmitAck( uint32 u32AddrDst );

/**
 * アプリケーションごとの振る舞いを記述するための関数テーブル
 */
tsCbHandlerPair *psCbHandlerPair;
void *pvProcessEvPair;

static tsPairingConf psPairingConf;		//!< ペアリングの設定パラメタ構造体

static uint8 u8Port = 0xFF;				//!< ペアリングしているときのステータスを見るためのDIOポート番号 0x80,0x81:DO0,1を使用する。 0x00-0x14:DIO0-18(20)を使用する。 0xFF:使用しない
static bool_t bDOFlag = FALSE;			//!< DOを使用するかどうかのフラグ
static uint8 u8RemoteConf = 0xFF;		//!< どんなパケットを送信したかを判別するためのフラグ
static uint8 u8Flags = 0xFF;			//!< 何を受信したかを判別するためのフラグ
static uint32 u32RcvAddr = 0xFFFFFFFF;	//!< 受信した相手のアドレス

static tsPairingInfo sPairingInfo;		//!< ペアリング結果を保存する構造体

static bool_t bLEDBlink = TRUE;			//!< 点滅フラグ
static uint8 au8Data[64];				//!< 汎用データ領域

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
		// 初期化
		memset( &sPairingInfo, 0x00, sizeof(sPairingInfo) );
		sPairingInfo.au8Data = au8Data;

		// LEDを使用しなければ点滅しない
		if( IS_NOUSE_LED(u8Port) ){
			bLEDBlink = FALSE;
		}else{
			//	出力ポートの初期化
			if( bDOFlag ){
				vAHI_DoSetDataOut( u8Port+1, 0 );
			}else{
				vPortSetHi( u8Port );
				vPortAsOutput( u8Port );
			}
		}
		V_PRINTF( LB LB "*** Entering Pairing Mode ***");

		// MAC start
		ToCoNet_vMacStart();

		if( IS_PARENT() ){		// 親モードの場合、待ち受け状態にする。
			V_PRINTF( LB"!PARENT MODE");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN );	// ペアリング要求待ち（マスターとして振る舞う）
		}else{
			if( IS_CHILD() ){
				V_PRINTF( LB"!CHILD MODE");
			}
			ToCoNet_Event_SetState(pEv, E_STATE_WAIT_COMMAND);	// 設定パケット要求状態へ
		}
	}
}

/**
 * 設定パケットを要求する。
 *
 * @param E_STATE_WAIT_COMMAND
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_WAIT_COMMAND, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint8 u8Count = 0;				//!< 送信回数
	if (eEvent == E_EVENT_NEW_STATE) {
		// リクエストパケットを送信する
		if (!bTranmitRequest()) {
			V_PRINTF( LB"!CHANGE LESTEN MODE" );
			ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN); // 失敗時はペアリング要求待ち（マスターとして振る舞う）
		}
	}

	// 60ms 経過後はペアリング受信待ち
	if (PRSEV_u32TickFrNewState(pEv) > 60 || u8Flags == 1 ) {
		if( u8Flags == 1 ){		//	成功したのでAckを返す状態へ
			u8Flags = 0xFF;
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}else{
			if( u8Count == psPairingConf.u8Retry ){		//	規定回数リトライ下かどうかの判断
				if( IS_CHILD() ){						//	子機モードならこれ以上何もしない
					ToCoNet_Event_SetState(pEv, E_STATE_APP_COMP );
				}else{									//違うならペアリング要求待ち
					V_PRINTF( LB"!TIME OUT RX" );
					V_PRINTF( LB"!CHANGE LESTEN MODE" );
					u8Flags = 0xFF;
					ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN);	// 失敗時はペアリング要求待ち（マスターとして振る舞う）
				}
			}else{		//	規定回数以下ならもう一度
				u8Count++;
				ToCoNet_Event_SetState(pEv, E_STATE_APP_ONEMORE_TX );	// もう一度送信
			}
		}
	}
}

/**
 * 複数回送信するために便宜的に遷移してくる状態
 *
 * @param E_STATE_APP_ONEMORE_TX
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_ONEMORE_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		V_PRINTF( LB"!ONE MORE TX" );
		ToCoNet_Event_SetState(pEv, E_STATE_WAIT_COMMAND);		//	設定パケットを要求する状態に戻る
	}
}

/**
 * Ackを送信する状態
 *
 * @param E_STATE_RUNNING
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Ackを送信する
		if (!bTranmitAck( u32RcvAddr )) {
			V_PRINTF( LB"!CHANGE LESTEN MODE" );
			ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN); // 失敗時はペアリング要求待ち
		}
	}
	if ( u8RemoteConf == 2 ) {		//	送信できた！
		ToCoNet_Event_SetState(pEv, E_STATE_APP_COMP);		// 保存状態へ
	}
	else
	if(PRSEV_u32TickFrNewState(pEv) > 50 ){					// タイムアウト
		V_PRINTF( LB"!TIME OUT ACK" );
		V_PRINTF( LB"!CHANGE LESTEN MODE" );
		ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN);	// 失敗時はペアリング要求待ち
	}
}

/**
 * ペアリング要求待ち
 *
 * @param E_STATE_FAILED
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_LISTEN, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if(  u8Flags == 0 ){
		// 設定パケット送信
		if ( !bTranmitRespond( u32RcvAddr ) ) {
			V_PRINTF( LB"!TX FAILURE" );
			ToCoNet_Event_SetState(pEv, E_STATE_APP_LISTEN); // 失敗時はペアリング要求待ち
		}else{		//	成功した場合Ack返送待ち
			u8Flags = 0xFF;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_ACK );
		}
	}
	else	// タイムアウトなら、終了状態へ
	if ( PRSEV_u32TickFrNewState(pEv) > psPairingConf.u32ListenWait_ms ) {
			ToCoNet_Event_SetState( pEv, E_STATE_APP_COMP );
	}
}

/**
 * Ack待ちをする状態
 * @param E_STATE_APP_WAIT_ACK
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_ACK, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if( u8Flags == 2 ){
		//	Ackが帰ってきたら終了状態へ
		ToCoNet_Event_SetState(pEv, E_STATE_APP_COMP );
	}
	else	//	タイムアウトならペアリング要求待ち状態へ
	if ( PRSEV_u32TickFrNewState(pEv) > 60 ) {
		V_PRINTF( LB"!ONE MORE TIME" );
		u32RcvAddr = 0xFFFF;
		ToCoNet_Event_SetState( pEv, E_STATE_APP_LISTEN );
	}
}

/**
 * ペアリング完了時、設定を書込みを行い、これ以上何もしない
 *
 * @param E_STATE_APP_COMP
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
PRSEV_HANDLER_DEF(E_STATE_APP_COMP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		bLEDBlink = FALSE;
		if( u8Flags != 2 && u8RemoteConf != 2 ){	//	Ack送信完了もAck受信もしていない場合失敗
			V_PRINTF( LB"!INF: PAIRING FAILURE");
			sPairingInfo.u32DstAddr = 0xFFFF;
			sPairingInfo.u32PairKey = 0x00000000;
			sPairingInfo.bStatus = FALSE;		// 失敗
			//	LED点灯
			if( !IS_NOUSE_LED(u8Port) ){
				//	出力ポートの初期化
				if( bDOFlag ){
					vAHI_DoSetDataOut( 0, u8Port+1 );
				}else{
					vPortSetLo( u8Port );
				}
			}
		}else{		//	成功した場合
			sPairingInfo.u32DstAddr = u32RcvAddr;
			sPairingInfo.bStatus = TRUE;		// 成功
			if( u8Flags == 2 ){		//	最後にACKを受信した場合、ペアキーを自分のアドレスにする
				sPairingInfo.u32PairKey = ToCoNet_u32GetSerial();
			}else{					//	最後にACKを送信した場合、ペアキーを相手のアドレスにする
				sPairingInfo.u32PairKey = u32RcvAddr;
			}
			//	LEDの消灯
			if( !IS_NOUSE_LED(u8Port) ){
				//	出力ポートの初期化
				if( bDOFlag ){
					vAHI_DoSetDataOut( u8Port+1, 0 );
				}else{
					vPortSetHi( u8Port );
				}
			}
			V_PRINTF( LB"!INF: PAIRING SUCCESS -> %08X.", sPairingInfo.u32PairKey);
		}
		//	コールバック関数で設定の保存等を行う。
		psPairingConf.pf_cbSavePair( &sPairingInfo );
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_WAIT_COMMAND),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_ONEMORE_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_LISTEN),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_ACK),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_COMP),
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
 * ハードイベント（遅延割り込み処理）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bStateLED = FALSE;
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		if(bLEDBlink){	//点滅フラグが立っているとき、128msごとにLEDを点滅させる
			if( (u32TickCount_ms >> 7)&1  ){
				bStateLED = TRUE;
			}
			if( bDOFlag ){
				vAHI_DoSetDataOut( bStateLED ? 0 : u8Port+1, bStateLED ? u8Port+1 : 0 );
			}else{
				vPortSet_TrueAsLo(u8Port, bStateLED);
			}
		}else{

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
	uint8 *p = pRx->auData;

	if (pRx->u8Cmd == PAIR_PKTCMD) {
		// *   OCTET    : パケットバージョン (1)
		uint8 u8pktver = G_OCTET();
		if (u8pktver != PAIR_PRTCL_VERSION) {
			V_PRINTF( LB"!PRTCL_VERSION");
			return;
		}

		// *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
		uint8 u8pkttyp = G_OCTET();
		if ( u8pkttyp > 2 ) {
			V_PRINTF( LB"!PKTTYPE_DAT");
			return;
		}

		// *   OCTET(4) : アプリケーションバージョン
		uint32 u32ver = G_BE_DWORD();
		if (u32ver != psPairingConf.u32AppVer ) {
			V_PRINTF( LB"!VERSION_U32");
			return;
		}

		// *   LQIの確認
		if (pRx->u8Lqi < psPairingConf.u8ThLQI ) {
			V_PRINTF( LB"!LQI");
			return;
		}

		uint32 u32Key = G_BE_DWORD();
		if( psPairingConf.u32PairKey != u32Key ){
			V_PRINTF( LB"!KEY");
			return;
		}

		uint8 i;
		//	Ackパケット以外はデータが送られてくる可能性があるので保存する。
		if( u8pkttyp < 2 ){
			sPairingInfo.u8DataType = G_OCTET();
			sPairingInfo.u8DataLength = G_OCTET();

			for( i=0; i<sPairingInfo.u8DataLength; i++ ){
				sPairingInfo.au8Data[i] = G_OCTET();
			}
		}

		//	受信完了
		u32RcvAddr = pRx->u32SrcAddr;
		u8Flags = u8pkttyp;
	}
}

/**
 * 送信完了イベント
 *
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent( uint8 u8CbId, uint8 bStatus ){
	// 送信したパケットの種別IDによってフラグを立てる
	if (bStatus) {
		if(u8CbId == PAIR_PKTTYPE_REQUEST){
			u8RemoteConf = 0;
		}
		else
		if(u8CbId == PAIR_PKTTYPE_DATA){
			u8RemoteConf = 1;
		}
		else
		if(u8CbId == PAIR_PKTTYPE_ACK){
			u8RemoteConf = 2;
		}
	}else{
		u8RemoteConf = 0xFF;
	}
}

/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandlerPair sCbHandlerPair = {
	NULL, //cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, //cbAppToCoNet_vMain,
	NULL, //cbAppToCoNet_vNwkEvent,
	cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppPairing( tsPairingConf *psConfig ) {
	psCbHandlerPair = &sCbHandlerPair;
	pvProcessEvPair = vProcessEvCore;
	psPairingConf = *psConfig;

	//	各要素の代入
	sToCoNet_AppContext.u32AppId = psPairingConf.u32AppID;
	sToCoNet_AppContext.u8Channel = psPairingConf.u8PairCh;
	sToCoNet_AppContext.u32ChMask = 1 << psPairingConf.u8PairCh;

	//	特に指定がなければ待ち時間は5秒間
	if( psPairingConf.u32ListenWait_ms == 0 ){
		psPairingConf.u32ListenWait_ms = 5000;
	}

	if( psPairingConf.u8LEDPort == PAIR_LED_NOUSE ){
		// LED出力を無効にする
		u8Port = 0xFF;
	}else{
		// MSBが1なら DO(PWM2,3)を使用する
		bDOFlag = (psPairingConf.u8LEDPort&0x80) != 0x00 ? TRUE : FALSE;
		// 使用するポートの代入
		u8Port = psPairingConf.u8LEDPort&0x7F;
	}
	//	受信回路を開く
	sToCoNet_AppContext.bRxOnIdle = TRUE;
}


/* *********************************/
/* * 送信関連の手続き              */
/* *********************************/

/**
 * 送信手続き
 * @param pTx
 * @return
 */
static bool_t bTransmit(tsTxDataApp *pTx)
{
	// 送信元
	pTx->u32SrcAddr = ToCoNet_u32GetSerial();

	pTx->u8Cmd = PAIR_PKTCMD; // 0..7 の値を取る。パケットの種別を分けたい時に使用する

	pTx->bAckReq = TRUE; // Ack
	pTx->u8Retry = 0x01; // アプリケーション再送１回

	// 送信
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
static bool_t bTranmitRequest()
{
	TXINIT(sTx);

	sTx.u32DstAddr = 0xFFFF; // ブロードキャスト

	// ペイロードの準備
	//  *   OCTET    : パケットバージョン (1)
	S_OCTET(PAIR_PRTCL_VERSION);
	//  *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
	S_OCTET(PAIR_PKTTYPE_REQUEST);
	//  *   OCTET(4) : アプリケーションバージョン
	S_BE_DWORD(psPairingConf.u32AppVer);
	//  *   パケット種別 = 要求
	//  *   OCTET(4) : 同じアプリケーションかどうかを識別するための鍵
	S_BE_DWORD(psPairingConf.u32PairKey);
	//  *   OCTET : データの識別子
	S_OCTET( psPairingConf.u8DataType );
	//  *   OCTET : データ列の長さ
	S_OCTET( psPairingConf.u8DataLength );
	//  *   OCTET(len) : データ
	uint8 i;
	for( i=0; i< psPairingConf.u8DataLength; i++ ){
		S_OCTET( psPairingConf.au8Data[i] );
	}

	sTx.u8Len = q - sTx.auData;
	sTx.u8CbId = PAIR_PKTTYPE_REQUEST;

	// 送信
	return bTransmit(&sTx);
}

/**
 * 設定パケット要求
 * @return
 */
static bool_t bTranmitRespond(uint32 u32AddrDst)
{
	TXINIT(sTx);

	sTx.u32DstAddr = u32AddrDst; // 送信先

	// ペイロードの準備
	//  *   OCTET    : パケットバージョン (1)
	S_OCTET(PAIR_PRTCL_VERSION);
	//  *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
	S_OCTET(PAIR_PKTTYPE_DATA);
	//  *   OCTET(4) : アプリケーションバージョン
	S_BE_DWORD(psPairingConf.u32AppVer);
	//  *   OCTET(4) : 同じアプリケーションかどうかを識別するための鍵
	S_BE_DWORD( psPairingConf.u32PairKey );
	//  *   OCTET : データの識別子
	S_OCTET( psPairingConf.u8DataType );
	//  *   OCTET : データ列の長さ
	S_OCTET( psPairingConf.u8DataLength );
	//  *   OCTET(len) : データ
	uint8 i;
	for( i=0; i< psPairingConf.u8DataLength; i++ ){
		S_OCTET( psPairingConf.au8Data[i] );
	}

	sTx.u8Len = q - sTx.auData;
	sTx.u8CbId =PAIR_PKTTYPE_DATA;

	// 送信
	return bTransmit(&sTx);
}

/**
 * 設定完了
 * @param bStat
 * @return
 */
static bool_t bTranmitAck( uint32 u32AddrDst )
{
	TXINIT(sTx);

	sTx.u32DstAddr = u32AddrDst; // 送信先

	// *   OCTET    : パケットバージョン (1)
	S_OCTET(PAIR_PRTCL_VERSION);
	// *   OCTET    : パケット種別 (0: 要求, 1: 応答, 2:ACK)
	S_OCTET(PAIR_PKTTYPE_ACK);
	// *   OCTET(4) : アプリケーションバージョン
	S_BE_DWORD(psPairingConf.u32AppVer);
	//  *   OCTET(4) : 同じアプリケーションかどうかを識別するための鍵
	S_BE_DWORD( psPairingConf.u32PairKey );

	sTx.u8Len = q - sTx.auData;
	sTx.u8CbId = PAIR_PKTTYPE_ACK;

	// 送信
	return bTransmit(&sTx);
}
