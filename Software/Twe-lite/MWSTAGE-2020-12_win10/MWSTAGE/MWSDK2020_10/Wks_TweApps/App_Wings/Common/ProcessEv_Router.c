/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <AppHardwareApi.h>

#include "ToCoNet.h"
#include "ToCoNet_event.h"

#include "App_Wings.h"
#include "App_PAL.h"

#include "common.h"

#include "utils.h"
#include "ccitt8.h"

#include "twecommon.h"
#include "tweserial.h"
#include "tweserial_jen.h"
#include "tweprintf.h"
#include "twesettings.h"
#include "tweutils.h"
#include "twesercmd_gen.h"
#include "tweinteractive.h"
#include "twesysutils.h"

#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#define ToCoNet_USE_MOD_NBSCAN_SLAVE // Neighbour scan slave module
#define ToCoNet_USE_MOD_NWK_LAYERTREE // Network definition
#define ToCoNet_USE_MOD_NWK_MESSAGE_POOL
#define ToCoNet_USE_MOD_CHANNEL_MGR
#define ToCoNet_USE_MOD_DUPCHK

#include "ToCoNet_mod_prototype.h"

static void vRepeat_Act(tsRxDataApp* psRx);
static void vRepeat_AppTwelite(tsRxDataApp* psRx);
static void vRepeat_AppIO(tsRxDataApp* psRx);
static void vRepeat_AppUart(tsRxDataApp* psRx);
static void vRepeat_AppPAL(tsRxDataApp* psRx);
static void vProcessSerialCmd(TWESERCMD_tsSerCmd_Context *pSer);

static int16 i16Transmit_AppUart_Msg(uint8 *p, uint16 u16len, tsTxDataApp *pTxTemplate,
		uint32 u32AddrSrc, uint8 u8AddrSrc, uint32 u32AddrDst, uint8 u8AddrDst,
		uint8 u8Relay, uint8 u8Req, uint8 u8RspId, uint8 u8Opt, uint8 u8Cmd);

extern TWE_tsFILE sSer;
extern TWEINTRCT_tsContext* sIntr;

extern tsTimerContext sTimerPWM; //!< タイマー管理構造体  @ingroup MASTER

tsSerSeq sSerSeqTx; //!< 分割パケット管理構造体（送信用）  @ingroup MASTER
tsSerSeq sSerSeqRx; //!< 分割パケット管理構造体（受信用）  @ingroup MASTE
uint8 au8SerBuffRx[SERCMD_MAXPAYLOAD + 32]; //!< sSerSeqRx 用に確保  @ingroup MASTER
extern TWESERCMD_tsSerCmd_Context sSerCmdOut; //!< シリアル出力

extern tsToCoNet_DupChk_Context* psDupChk;

/*
 * E_STATE_IDLE
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_START_UP) {
		// LayerNetwork で無ければ、特別な動作は不要。
		// run as default...

		// 始動メッセージの表示
		if (!(u32evarg & EVARG_START_UP_WAKEUP_MASK)) {
			vSerInitMessage();
		}

		// 重複チェッカーのタイムアウト時間を指定する。
		if(IS_APPCONF_OPT_SHORTTIMEOUT()){
			TOCONET_DUPCHK_TIMEOUT_ms = 1024;
		}else{
			TOCONET_DUPCHK_TIMEOUT_ms = 2048;
		}
		TOCONET_DUPCHK_DECLARE_CONETXT(DUPCHK,40); //!< 重複チェック
		psDupChk = ToCoNet_DupChk_psInit(DUPCHK);

		Reply_vInit();

		if (IS_APPCONF_OPT_SECURE()) {
			bool_t bRes = bRegAesKey(sAppData.u32enckey);
			S_PRINT(LB "*** Register AES key (%d) ***", bRes);
		}

		// 中継ネットを使用しない中継は3ホップまで
		sAppData.u8max_hops = 3;

		// 中継ネットの設定
		sAppData.sNwkLayerTreeConfig.u8Layer = sAppData.u8layer;
		sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ROUTER;
		sAppData.sNwkLayerTreeConfig.u8StartOpt = TOCONET_MOD_LAYERTREE_STARTOPT_NB_BEACON;
		sAppData.sNwkLayerTreeConfig.u8Second_To_Beacon = TOCONET_MOD_LAYERTREE_DEFAULT_BEACON_DUR;

		// 接続先アドレスの指定
		if( sAppData.u32AddrHigherLayer ){
			sAppData.sNwkLayerTreeConfig.u8StartOpt = TOCONET_MOD_LAYERTREE_STARTOPT_FIXED_PARENT;	// 開始時にスキャンしない
			sAppData.sNwkLayerTreeConfig.u8ResumeOpt = 0x01;			// 過去にあった接続先があると仮定する
			sAppData.sNwkLayerTreeConfig.u32AddrHigherLayer = (sAppData.u32AddrHigherLayer&0x80000000) ? sAppData.u32AddrHigherLayer:sAppData.u32AddrHigherLayer|0x80000000;
		}			

		sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);
		if (sAppData.pContextNwk) {
			ToCoNet_Nwk_bInit(sAppData.pContextNwk);
			ToCoNet_Nwk_bStart(sAppData.pContextNwk);
		}
		//ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}
	if ( eEvent == E_ORDER_KICK ) {
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if( eEvent == E_EVENT_NEW_STATE ){
		S_PRINT(LB "*** RUNNING ***");
	}	
	if( eEvent == E_EVENT_TICK_SECOND ){
		ToCoNet_DupChk_vClean( psDupChk );
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
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

	default:
		break;
	}
}

/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
}

/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
static void cbAppToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		S_PRINT( LB"[E_EVENT_TOCONET_NWK_START]");
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
		S_PRINT( LB"[E_EVENT_TOCONET_NWK_DISCONNECT]");
		break;

	case E_EVENT_TOCONET_NWK_ROUTE_PKT:
		if (u32arg) {
			tsRoutePktInfo *pInfo = (void*)u32arg;

			if (pInfo->bUpstream) {
				//sAppData.u32LedCt = 25;
			}
		}
		break;

	case E_EVENT_TOCONET_NWK_MESSAGE_POOL_REQUEST:
		//sAppData.u32LedCt = 25;
		break;

	case E_EVENT_TOCONET_NWK_MESSAGE_POOL:
		break;

	case E_EVENT_TOCONET_PANIC:
		if (u32arg) {
			tsPanicEventInfo *pInfo = (void*)u32arg;
			V_PRINT( "PANIC DETECTED!");

			pInfo->bCancelReset = TRUE;
		}
		break;
	default:
		break;
	}
}

/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *psRx) {
	int i;

	// print coming payload
	S_PRINT(
			LB "[PKT Ad:%04x,Ln:%03d,Seq:%03d,Lq:%03d,Tms:%05d %s\"",
			psRx->u32SrcAddr, psRx->u8Len, // Actual payload byte: the network layer uses additional 4 bytes.
			psRx->u8Seq, psRx->u8Lqi, psRx->u32Tick & 0xFFFF, psRx->bSecurePkt ? "Enc " : "");

	for (i = 0; i < psRx->u8Len; i++) {
		if (i < 32) {
			S_PUTCHAR((psRx->auData[i] >= 0x20 && psRx->auData[i] <= 0x7f) ?
							psRx->auData[i] : '.');
		} else {
			S_PRINT("..");
			break;
		}
	}
	S_PRINT( "\"]");

	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE() && !IS_APPCONF_OPT_RCV_NOSECURE()) {
		if (!psRx->bSecurePkt) {
			S_PRINT("Receive plaintext. Discard this packet.");
			return;
		}
	}

	if(psRx->u8Cmd == TOCONET_PACKET_CMD_APP_PAL_REPLY ){
		//S_PRINT("!Reply Data.");
		Reply_bReceiveReplyData( psRx );
		return;
	}

	uint8* p = psRx->auData;
	uint16 u16Ver = G_BE_WORD();
	if( ((u16Ver>>8)&0x7F) == 'R' || ((u16Ver>>8)&0x7F) == 'T' ){
		vRepeat_AppPAL(psRx);
	}else if( (u16Ver&0x00FF) == APP_TWELITE_PROTOCOL_VERSION ){
		vRepeat_AppTwelite(psRx);
	}else if( (u16Ver&0x00FF) == APP_IO_PROTOCOL_VERSION && psRx->u8Len == 18 ){
		vRepeat_AppIO(psRx);
	}else if( (u16Ver&0x001F) == APP_UART_PROTOCOL_VERSION ){
		vRepeat_AppUart(psRx);
	}else{
		switch (psRx->u8Cmd) {
			case TOCONET_PACKET_CMD_APP_MWX:
				vRepeat_Act(psRx);
				break;
			default:
				vRepeat_AppTwelite(psRx);
				break;
		}
	}
}

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
/*	S_PRINT( LB ">>> MacAck%s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);*/
	return;
}
/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	cbAppToCoNet_vMain,
	cbAppToCoNet_vNwkEvent,
	cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppRouter() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
	pvProcessSerialCmd = vProcessSerialCmd;
}

/***
 * MWX ライブラリのパケットを中継する
 ***/
static void vRepeat_Act(tsRxDataApp* psRx)
{
	uint8 *p = psRx->auData;

	uint8 u8ver = G_OCTET();(void)u8ver;
	uint8 u8srclid = G_OCTET();(void)u8srclid;
	uint32 u32srcaddr = G_BE_DWORD();(void)u32srcaddr;
	uint32 u32distaddr = G_BE_DWORD();(void)u32distaddr;
	uint8 u8rptflag = G_OCTET();

	if(ToCoNet_DupChk_bAdd( psDupChk, u32srcaddr, psRx->u8Seq )){
		DBGOUT(3, "Dup,");
		return;
	}

	if( u8rptflag < sAppData.u8max_hops ){
		// フラグをインクリメント
		psRx->auData[10]++;

		tsTxDataApp sTx;
		memset(&sTx, 0, sizeof(sTx));

		// Payload
		memcpy(sTx.auData, psRx->auData, psRx->u8Len);
		sTx.u8Len = psRx->u8Len;

		// コマンド設定
		sTx.u8Cmd = psRx->u8Cmd; // パケット種別

		// 送信する
		sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		sTx.u8Retry = sAppData.u8StandardTxRetry; // 再送

		// フレームカウントとコールバック識別子の指定
		sTx.u8Seq = psRx->u8Seq;
		sTx.u8CbId = sTx.u8Seq;

		// 中継時の送信パラメータ
		sTx.bAckReq = FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 20; // 再送間隔

		sTx.u16DelayMin = 30; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)
		sTx.u16DelayMax = 100; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		ToCoNet_bMacTxReq(&sTx);
	}
}

/***
 * 標準アプリを中継する
 ***/
static void vRepeat_AppTwelite(tsRxDataApp* psRx)
{
	uint8 *p = psRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier)
		return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_TWELITE_PROTOCOL_VERSION)
		return;

	uint8 u8AppLogicalId = G_OCTET();(void)u8AppLogicalId;

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();
	(void) u8AppLogicalId_Dest;

	uint16 u16TimeStamp = G_BE_WORD();(void)u16TimeStamp;

	if(ToCoNet_DupChk_bAdd( psDupChk, u32Addr, psRx->u8Seq )){
		DBGOUT(3, "Dup,");
		return;
	}

	uint8 u8TxFlag = G_OCTET();
	if (u8TxFlag < sAppData.u8max_hops) {
		*(p - 1) = *(p - 1) + 1; // 中継済みフラグのセット

		// 中継する
		tsTxDataApp sTx;
		memset(&sTx, 0, sizeof(sTx));

		// Payload
		memcpy(sTx.auData, psRx->auData, psRx->u8Len);
		sTx.u8Len = psRx->u8Len;

		// コマンド設定
		sTx.u8Cmd = psRx->u8Cmd; // パケット種別

		// 送信する
		sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		sTx.u8Retry = sAppData.u8StandardTxRetry; // 再送

		// フレームカウントとコールバック識別子の指定
		sAppData.u16TxFrame++;
		sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
		sTx.u8CbId = sTx.u8Seq;

		// 中継時の送信パラメータ
		sTx.bAckReq = FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 8; // 再送間隔

		sTx.u16DelayMin = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		ToCoNet_bMacTxReq(&sTx);
	}
}

/***
 * リモコンアプリを中継する
 ***/
static void vRepeat_AppIO(tsRxDataApp* psRx)
{
	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE()) {
		if (!psRx->bSecurePkt) {
			S_PRINT( ".. skipped plain packet.");
			return;
		}
	}

	uint8 *p = psRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier)
		return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_IO_PROTOCOL_VERSION)
		return;

	uint8 u8AppLogicalId = G_OCTET();(void)u8AppLogicalId;

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();
	(void) u8AppLogicalId_Dest;

	uint16 u16TimeStamp = G_BE_WORD();(void)u16TimeStamp;

	if(ToCoNet_DupChk_bAdd( psDupChk, u32Addr, psRx->u8Seq )){
		DBGOUT(3, "Dup,");
		return;
	}

	uint8 u8TxFlag = G_OCTET();
	if (u8TxFlag < sAppData.u8max_hops) {
		*(p - 1) = *(p - 1) + 1; // 中継済みフラグのセット

		// 中継する
		tsTxDataApp sTx;
		memset(&sTx, 0, sizeof(sTx));

		// Payload
		memcpy(sTx.auData, psRx->auData, psRx->u8Len);
		sTx.u8Len = psRx->u8Len;

		// コマンド設定
		sTx.u8Cmd = psRx->u8Cmd; // パケット種別

		// 送信する
		sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		sTx.u8Retry = sAppData.u8StandardTxRetry; // 再送回数

		// フレームカウントとコールバック識別子の指定
		sAppData.u16TxFrame++;
		sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
		sTx.u8CbId = sTx.u8Seq;

		// 中継時の送信パラメータ
		sTx.bAckReq = FALSE;
		sTx.bSecurePacket = psRx->bSecurePkt;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;

		sTx.u16RetryDur = 8; // 再送間隔
		sTx.u16DelayMin = 8; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		ToCoNet_bMacTxReq(&sTx);
	}
}

/***
 * シリアル通信アプリを中継する
 ***/
static void vRepeat_AppUart(tsRxDataApp* psRx)
{
	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE()) {
		if (!psRx->bSecurePkt) {
			S_PRINT( ".. skipped plain packet.");
			return;
		}
	}

	uint8 *p = psRx->auData;

	/* ヘッダ情報の読み取り */
	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != (sAppData.u8AppIdentifier&0xFE) ) { return; }
	uint8 u8PtclVersion = G_OCTET();
	uint8 u8RepeatFlag = u8PtclVersion >> 6;
	u8PtclVersion &= 0x1F;
	if (u8PtclVersion != APP_UART_PROTOCOL_VERSION) { return; }
	uint8 u8RespId = G_OCTET();
	uint8 u8AppLogicalId = G_OCTET(); (void)u8AppLogicalId;
	uint32 u32AddrSrc = psRx->bNwkPkt ? psRx->u32SrcAddr : G_BE_DWORD();
	uint8 u8AppLogicalId_Dest = G_OCTET();
	uint32 u32AddrDst = psRx->bNwkPkt ? psRx->u32DstAddr : G_BE_DWORD();

	/* ここから中身 */
	uint8 u8req = G_OCTET();
	uint8 u8pktsplitinfo = G_OCTET();

	uint8 u8pktnum = u8pktsplitinfo & 0xF;
	uint8 u8idx = (u8pktsplitinfo & 0xF0) >> 4;

	uint8 u8opt = G_OCTET();

	uint8 u8len = (psRx->auData + psRx->u8Len) - p;
	uint16 u16offset = u8idx * SERCMD_SER_PKTLEN;

	/* 宛先と送信元のアドレスが一致する場合は処理しない */
	if (u32AddrSrc == u32AddrDst) return;
	if (u8AppLogicalId == u8AppLogicalId_Dest && u8AppLogicalId < 0x80) return;

	bool_t bNew = FALSE;
	if (sSerSeqRx.bWaitComplete) {
		// exceptional check
		if(u32TickCount_ms - sSerSeqRx.u32Tick > 2000) {
			// time out check
			bNew = TRUE;
		}
		if (u8req != sSerSeqRx.u8ReqNum) {
			// different request number is coming.
			bNew = TRUE;
		}
		if (u32AddrSrc != sSerSeqRx.u32SrcAddr) {
			// packet comes from different nodes. (discard this one!)
			bNew = TRUE;
		}
	} else {
		// 待ち状態ではないなら新しい系列
		bNew = TRUE;
	}

	if(bNew) {
		// treat this packet as new, so clean control buffer.
		memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
	}

	if (!sSerSeqRx.bWaitComplete) {
		// 新しいパケットを受信した

		// 最初にリクエスト番号が適切かどうかチェックする。
		bool_t bDup = ToCoNet_DupChk_bAdd(psDupChk, u32AddrSrc, u8req & 0x7f);
		DBGOUT(4, "<%02X,%c>", u8req, bDup ? 'D' : '-');
		if (bDup) {
			// 重複していたら新しい人生は始めない
			bNew = FALSE;
		}

		if (bNew) {
			sSerSeqRx.bWaitComplete = TRUE;
			sSerSeqRx.u32Tick = u32TickCount_ms;
			sSerSeqRx.u32SrcAddr = u32AddrSrc;
			sSerSeqRx.u32DstAddr = u32AddrDst;
			sSerSeqRx.u8PktNum = u8pktnum;
			sSerSeqRx.u8ReqNum = u8req;
			sSerSeqRx.u8RespID = u8RespId;
			sSerSeqRx.u8IdSender = u8AppLogicalId;
			sSerSeqRx.u8IdReceiver = u8AppLogicalId_Dest;
		}
	}

	if (sSerSeqRx.bWaitComplete) {
		if (u16offset + u8len <= (sizeof(au8SerBuffRx)-16) && u8idx < sSerSeqRx.u8PktNum) {
			// check if packet size and offset information is correct,
			// then copy data to buffer and mark it as received.
			if (!sSerSeqRx.bPktStatus[u8idx]) {
				sSerSeqRx.bPktStatus[u8idx] = 1;
				memcpy (au8SerBuffRx + u16offset, p, u8len);
			}

			// the last packet indicates full data length.
			if (u8idx == sSerSeqRx.u8PktNum - 1) {
				sSerSeqRx.u16DataLen = u16offset + u8len;
			}

			// 中継パケットのフラグを格納する
			if (u8RepeatFlag > sSerSeqRx.u8RelayPacket) {
				sSerSeqRx.u8RelayPacket = u8RepeatFlag;
			}
		}

		// check completion
		int i;
		for (i = 0; i < sSerSeqRx.u8PktNum; i++) {
			if (sSerSeqRx.bPktStatus[i] == 0) break;
		}

		if (i == sSerSeqRx.u8PktNum) {
			// 分割パケットが全て届いた！
			if(u8RepeatFlag < sAppData.u8max_hops){
				i16Transmit_AppUart_Msg(
						au8SerBuffRx,
						sSerSeqRx.u16DataLen,
						NULL,
						sSerSeqRx.u32SrcAddr,
						sSerSeqRx.u8IdSender,
						sSerSeqRx.u32DstAddr,
						sSerSeqRx.u8IdReceiver,
						sSerSeqRx.u8RelayPacket + 1,
						sSerSeqRx.u8ReqNum,
						sSerSeqRx.u8RespID,
						u8opt,
						psRx->u8Cmd
						);
			}
			memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
			sSerSeqRx.bWaitComplete = FALSE;
		}
	}
}

/** @ingroup MASTER
 * シリアルメッセージの送信要求。
 * パケットを分割して送信する。
 *
 *  - Packet 構造
 *   - [1] OCTET    : 識別ヘッダ(APP ID より生成), 下１ビットが1の時は中継済み
 *   - [1] OCTET    : プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - [1] OCTET    : 応答ID(外部から指定される値、任意に使用出来るデータの一つ)
 *   - [1] OCTET    : 送信元、簡易アドレス
 *   - [4] BE_DWORD : 送信元、拡張アドレス
 *   - [1] OCTET    : 宛先、簡易アドレス
 *   - [4] BE_DWORD : 宛先、拡張アドレス
 *   - [2] BE_WORD  : 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - [1] OCTET    : パケット群の識別ID
 *   - [1] OCTET    : パケット番号 (0...total-1) / パケット数(total)
 *   - [1] OCTET    : 拡張情報
 *
 * @param p 送信データ（実際に送信したいデータ部で各種ヘッダは含まない）
 * @param u16len データ長
 * @param pTxTemplate 送信オプションを格納した構造体（送信用の構造体を流用）
 * @param u32AddrSrc 送信元、拡張アドレス
 * @param u8AddrSrc 送信元、簡易アドレス
 * @param u32AddrDst 宛先、拡張アドレス
 * @param u8AddrDst 宛先、簡易アドレス
 * @param bRelay 中継フラグ、TRUEなら中継ビットを立てた状態で送信する
 * @param u8Req 識別ID、パケット群を識別するための内部的に利用するID。重複の除去などに用いる。
 * @param u8RspId 応答ID、外部向けの識別ID。成功失敗などの応答などにも用いる。
 * @param u8Cmd データ種別ID、モードの区別で使用する。
 * @return
 */
static int16 i16Transmit_AppUart_Msg(uint8 *p, uint16 u16len, tsTxDataApp *pTxTemplate,
		uint32 u32AddrSrc, uint8 u8AddrSrc, uint32 u32AddrDst, uint8 u8AddrDst,
		uint8 u8Relay, uint8 u8Req, uint8 u8RspId, uint8 u8Opt, uint8 u8Cmd) {

	// 処理中のチェック（処理中なら送信せず失敗）
//	if (sSerSeqTx.bWaitComplete) {
//		DBGOUT(4,"<S>");
//		return -1;
//	}
	TWE_fprintf(&sSer, "Uart ");

	// パケットを分割して送信する。
	tsTxDataApp sTx;
	if (pTxTemplate == NULL) {
		memset(&sTx, 0, sizeof(sTx));
	} else {
		memcpy(&sTx, pTxTemplate, sizeof(sTx));
	}
	uint8 *q; // for S_??? macros

	// sSerSeqTx は分割パケットの管理構造体
	memset(&sSerSeqTx, 0, sizeof(sSerSeqTx)); // ゼロクリアしておく
	sSerSeqTx.u8IdSender = sAppData.u8AppLogicalId;

#if 0
	if( IS_APPCONF_OPT_FORMAT_TO_NOPROMPT() && (au8UartModeToTxCmdId[sAppData.u8uart_mode] == 1 || au8UartModeToTxCmdId[sAppData.u8uart_mode] == 3 )){
		if(IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId) && u8AddrDst == LOGICAL_ID_BROADCAST){
			sSerSeqTx.u8IdReceiver = 0x00;		// 自分が子機だったら親機にする
		}else{
			sSerSeqTx.u8IdReceiver = u8AddrDst;		// 自分が親機だったら指定のIDにする
		}
	}else{
		sSerSeqTx.u8IdReceiver = u8AddrDst;
	}
#endif
	sSerSeqTx.u8IdReceiver = u8AddrDst;

	sSerSeqTx.u8PktNum = (u16len - 1) / SERCMD_SER_PKTLEN + 1; // 1...80->1, 81...160->2, ...
	sSerSeqTx.u16DataLen = u16len;

	if (u16len > SERCMD_MAXPAYLOAD) {
		return -1; // ペイロードが大きすぎる
	}
	//TWE_fprintf(&sSer, "Uart %d/%d", u8Relay, sAppData.u8max_hops);

	static uint8 u8UartSeqNext = 0;
	sSerSeqTx.u8RespID = u8RspId;
	sSerSeqTx.u8Seq = u8UartSeqNext; // パケットのシーケンス番号（アプリでは使用しない）
	u8UartSeqNext = (sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum) & 0x3F; // 次のシーケンス番号（予め計算しておく）
	sSerSeqTx.u8ReqNum = u8Req; // パケットの要求番号（この番号で送信系列を弁別する）

	sSerSeqTx.u32Tick = u32TickCount_ms;
	if (sTx.auData[0x06] && sSerSeqTx.u8PktNum == 1) {
		// 併行送信時は bWaitComplete の条件を立てない
		sSerSeqTx.bWaitComplete = FALSE;
	} else {
		sSerSeqTx.bWaitComplete = TRUE;
	}

	// 送信後応答しない
	bool_t bNoResponse = FALSE;
	if (sTx.u8CbId & 0x80) {
		bNoResponse = TRUE;
	}

	memset(sSerSeqTx.bPktStatus, 0, sizeof(sSerSeqTx.bPktStatus));

	DBGOUT(3, "* >>> Transmit(req=%d,cb=0x02X) Tick=%d <<<" LB,
			sSerSeqTx.u8ReqNum, sTx.u8CbId ,u32TickCount_ms & 65535);

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA; // data packet.

	sTx.bSecurePacket = IS_APPCONF_OPT_SECURE() ? TRUE: FALSE;

	if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
	} else {
		// MAC 直接の送受信

		// 送信設定の微調整を行う
		if (u8Relay) {
			sTx.u8Retry = sAppData.u8StandardTxRetry;
			sTx.u16DelayMin = 20; // 中継時の遅延
			sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // application retry
		} else
		if (pTxTemplate == NULL) {
			// 簡易書式のデフォルト設定
			sTx.u8Retry = sAppData.u8StandardTxRetry;
			sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // アプリケーション再送間隔はパケット分割数に比例
		}

		// 宛先情報(指定された宛先に送る)
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		if (sTx.bAckReq) {
			sTx.u32DstAddr = u32AddrDst;
		} else {
			sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		}
	}

	int i;
	for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
		q = sTx.auData;
		sTx.u8Seq = (sSerSeqTx.u8Seq + i) & 0x3F;
		sTx.u8CbId = (sSerSeqTx.u8PktNum > 1) ? (sTx.u8Seq | 0x40) : sTx.u8Seq; // callback will reported with this ID

		if (u8Relay || bNoResponse) { // 中継パケットおよび応答なしフラグの時は完了メッセージを返さない
			sTx.u8CbId |= 0x80;
		}

		// コールバックIDと応答IDを紐づける
		//au8TxCbId_to_RespID[sTx.u8CbId] = sSerSeqTx.u8RespID;

		// 基本は中継するだけなので、受信したIDをそのまま転送する
		sTx.u8Cmd = u8Cmd;

		// ペイロードを構成
		S_OCTET(sAppData.u8AppIdentifier&0xFE);
		S_OCTET(APP_UART_PROTOCOL_VERSION + (u8Relay << 6));

		S_OCTET(sSerSeqTx.u8RespID); // 応答ID

		S_OCTET(u8AddrSrc); // 送信元アプリケーション論理アドレス
		if (!(sAppData.eNwkMode == E_NWKMODE_LAYERTREE)) { // ネットワークモードの場合は、ロングアドレスは省略
			S_BE_DWORD(u32AddrSrc);  // シリアル番号
		}

		S_OCTET(sSerSeqTx.u8IdReceiver); // 宛先

		if (!(sAppData.eNwkMode == E_NWKMODE_LAYERTREE)) { // ネットワークモードの場合は、ロングアドレスは省略
			S_BE_DWORD(u32AddrDst); //最終宛先
		}

		S_OCTET(sSerSeqTx.u8ReqNum); // request number

		uint8 u8pktinfo = (i << 4) + sSerSeqTx.u8PktNum;
		S_OCTET(u8pktinfo); //トータルパケット数とパケット番号

		S_OCTET(u8Opt); // ペイロードのオプション(コマンド、など)

		uint8 u8len_data = (u16len >= SERCMD_SER_PKTLEN) ? SERCMD_SER_PKTLEN : u16len;

		memcpy (q, p, u8len_data);
		q += u8len_data;

		sTx.u8Len = q - sTx.auData;

		if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
			ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx);
		} else {
			ToCoNet_bMacTxReq(&sTx);
		}

		p += u8len_data;
		u16len -= SERCMD_SER_PKTLEN;
	}

	return 0;
}

/***
 * 無線タグ/パル専用アプリを中継する
 ***/
static void vRepeat_AppPAL(tsRxDataApp* psRx)
{
	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE()) {
		if (!psRx->bSecurePkt) {
			S_PRINT( ".. skipped plain packet.");
			return;
		}
	}

	if(ToCoNet_DupChk_bAdd(psDupChk, psRx->u32SrcAddr, psRx->u8Seq)){
		DBGOUT(3, LB"! Dup.(App_PAL)");
		return;
	}

	// 直接受信したパケットを上位へ転送する
	//
	// 直接親機宛(TOCONET_NWK_ADDR_PARENT指定で送信)に向けたパケットはここでは処理されない。
	// 本処理はアドレス指定がTOCONET_NWK_ADDR_NEIGHBOUR_ABOVEの場合で、一端中継機が受け取り
	// その中継機のアドレス、受信時のLQIを含めて親機に伝達する方式である。
	if ((psRx->auData[0]&0x7F) == 'T') {
		tsTxDataApp sTx;
		memset(&sTx, 0, sizeof(sTx));
		uint8 *q = sTx.auData;

		uint8 u8Headder = 'R' + (psRx->auData[0]&0x80);
		//u8Headder |= psRx->auData[0]&0x80;
		S_OCTET(u8Headder); // 1バイト目に中継機フラグを立てる
		S_BE_DWORD(psRx->u32SrcAddr); // 子機のアドレスを
		S_OCTET(psRx->u8Lqi); // 受信したLQI を保存する

		memcpy(sTx.auData + 6, psRx->auData + 1, psRx->u8Len - 1); // 先頭の１バイトを除いて５バイト先にコピーする
		q += psRx->u8Len - 1;

		sTx.u8Len = q - sTx.auData;

		sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
		sTx.u32SrcAddr = ToCoNet_u32GetSerial(); // Transmit using Long address
		sTx.u8Cmd = psRx->u8Cmd; // data packet.

		sTx.u8Seq = psRx->u8Seq;
		sTx.u8CbId = psRx->u8Seq;

		sTx.u16DelayMax = 300; // 送信開始の遅延を大きめに設定する

		if (IS_APPCONF_OPT_SECURE()) {
			sTx.bSecurePacket = TRUE;
		}

		ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx);

		if(psRx->auData[0]&0x80){
			uint8* p = psRx->auData + 5;
			//p += 5;
			uint8 u8length = G_OCTET();
			uint8 i=0;
			while(i<u8length){
				uint8 sns = G_OCTET();
				switch( sns ){
					case HALLIC:
					p += 2;
					break;
				case TEMP:
					p += 3;
					break;
				case HUM:
					p += 3;
					break;
				case ILLUM:
					p += 5;
					break;

				case ACCEL:
					_C{
						uint8 u8Int = G_OCTET();(void)u8Int;
						uint8 u8Num = G_OCTET();
						uint8 u8Sampling = G_OCTET();
						u8Sampling = (u8Sampling<<5)&0xFF;		// 5bitシフトしておく
						uint8 u8Bit = G_OCTET();(void)u8Bit;

						uint8 j = 0;
						while( j < u8Num ){
							p += 9;
							j += 2;
						}
						i += (u8Num-1);
					}
					break;
				case EVENT:
					_C{
						uint8 u8Sns = G_OCTET();
						if(u8Sns&0x80){
							p+=3;
						}else{
							p++;
						}
					}
					break;
				case LED:	// ここは要修正
					p++;
					break;
				case ADC:
					_C{
						uint8 u8num = G_OCTET();
						if(u8num == 0x01 || u8num == 0x08){
							p++;
						}else{
							p+=2;
						}
					}
					break;
				case DIO:
					_C{
						uint8	u8num = G_OCTET();
						if(u8num <= 8){
							p++;
						}else if(u8num<=16){
							p+=2;
						}else{
							p+=4;
						}
					}
					break;
				case EEPROM:
					p+=2;
					break;
				case REPLY:
					_C{
						tsRxPktInfo sRxPktInfo;
						sRxPktInfo.u32addr_rcvr = TOCONET_NWK_ADDR_PARENT;
						sRxPktInfo.u8lqi_1st = psRx->u8Lqi;
						sRxPktInfo.u32addr_1st = psRx->u32SrcAddr;
						sRxPktInfo.u8id = psRx->auData[1];
						sRxPktInfo.u16fct = (psRx->auData[3]<<8)|psRx->auData[2];
						sRxPktInfo.u8pkt = psRx->auData[4];
						Reply_bSendData(sRxPktInfo);
					}
					return;

				default:
					break;
				}
				i++;
			}
		}
	}
}

/** @ingroup MASTER
 * シリアルから入力されたコマンド形式の電文を処理します。
 *
 * - 先頭バイトがアドレス指定で、0xDB 指定の場合、自モジュールに対しての指令となります。
 * - ２バイト目がコマンドで、0x80 以降を指定します。0x7F 以下は特別な処理は割り当てられていません。
 * - コマンド(0xDB向け)
 *   - SERCMD_ID_GET_MODULE_ADDRESS\n
 *     モジュールのシリアル番号を取得する
 * - コマンド(外部アドレス向け)
 *   - SERCMD_ID_REQUEST_IO_DATA\n
 *     IO状態の設定
 *   - それ以外のコマンドID\n
 *     通常送信 (ただし 0x80 以降は今後の機能追加で意味が変わる可能性あり)
 *
 * @param pSer シリアルコマンド入力の管理構造体
 */
static void vProcessSerialCmd(TWESERCMD_tsSerCmd_Context *pSer) {
//	uint8 *p = pSer->au8data;

	// COMMON FORMAT
//	uint8 u8addr = G_OCTET(); // 送信先論理アドレス
//	uint8 u8cmd = G_OCTET(); // コマンド

//	DBGOUT(3, "* UARTCMD ln=%d cmd=%02x req=%02x %02x%0x2%02x%02x..." LB,
//		pSer->u16len, u8addr, u8cmd, *p, *(p + 1), *(p + 2), *(p + 3));

}