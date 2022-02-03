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

typedef struct{
	uint8	u8LID;
	uint8	u8SNSID;
	uint8	u8HallIC;
	int16	i16Temp;
	uint16	u16Hum;
	uint32	u32Illumi;
	int16	au16Accle[3][32];
	uint8	u8Event;
	uint8	au8LED[4];
	uint8	u8LEDBlink;
}tsSnsData;

static void vReceiveNwkMsg(tsRxDataApp *pRx);
static void vReceiveActData(tsRxDataApp *pRx);
static void vReceive_AppTwelite(tsRxDataApp *pRx);
static void vReceive_AppUart(tsRxDataApp *pRx);
static void vReceive_AppIO(tsRxDataApp *pRx);

static int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq);
static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, uint32 u32AddrSrc,
		uint8 u8AddrSrc, uint8 u8AddrDst, uint8 u8RelayLv, uint8 u8Req);

static void vProcessSerialCmd(TWESERCMD_tsSerCmd_Context *pSer);

void vSerOutput_PAL(tsRxPktInfo sRxPktInfo, uint8 *p);
void vSerOutput_Tag(tsRxPktInfo sRxPktInfo, uint8 *p);

void vSetReplyData(tsSnsData* data);

extern TWE_tsFILE sSer;
extern TWEINTRCT_tsContext* sIntr;

extern tsTimerContext sTimerPWM; //!< タイマー管理構造体  @ingroup MASTER

tsSerSeq sSerSeqTx; //!< 分割パケット管理構造体（送信用）  @ingroup MASTER
tsSerSeq sSerSeqRx; //!< 分割パケット管理構造体（受信用）  @ingroup MASTE
uint8 au8SerBuffRx[SERCMD_MAXPAYLOAD + 32]; //!< sSerSeqRx 用に確保  @ingroup MASTER
extern TWESERCMD_tsSerCmd_Context sSerCmdOut; //!< シリアル出力

extern tsToCoNet_DupChk_Context* psDupChk;

uint8 au8Color[9][4] = {
//	 R, G, B, W
	{1, 0, 0, 0},	// red
	{0, 1, 0, 0},	// green
	{0, 0, 1, 0},	// blue
	{1, 1, 0, 0},	// yellow
	{1, 0, 1, 0},	// magenta
	{0, 1, 1, 0},	// cyan
	{1, 1, 1, 0},	// white
	{0, 0, 0, 1},	// warm white
	{1, 1, 1, 1}	// all
};

uint8 au8BlinkCycle[16] = { 0, 0x17, 0x0B, 0x05, 0x17, 0x17, 0x2E, 0x2E, 0x45, 0x45, 0x0C, 0x0C, 0x08, 0x08, 0x2E, 0x45 };
uint8 au8BlinkDuty[16] =  { 0, 0x7F, 0x7F, 0x7F, 0x06, 0x0D, 0x03, 0x07, 0x02, 0x05, 0x0C, 0x1A, 0x12, 0x27, 0x7F, 0x7F };

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

		// 中継ネットの設定
		sAppData.sNwkLayerTreeConfig.u8Layer = 0;
		sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_PARENT;
		sAppData.sNwkLayerTreeConfig.u8StartOpt = TOCONET_MOD_LAYERTREE_STARTOPT_NB_BEACON;
		sAppData.sNwkLayerTreeConfig.u8Second_To_Beacon = TOCONET_MOD_LAYERTREE_DEFAULT_BEACON_DUR;

		sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);
		if (sAppData.pContextNwk) {
			ToCoNet_Nwk_bInit(sAppData.pContextNwk);
			ToCoNet_Nwk_bStart(sAppData.pContextNwk);
		}
	} else if (eEvent == E_EVENT_TOCONET_NWK_START) {
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if( eEvent == E_EVENT_NEW_STATE ){
		DBGOUT(3, LB "*** RUNNING ***");
	}

	if( eEvent == E_EVENT_TICK_SECOND ){
		ToCoNet_DupChk_vClean( psDupChk );

		uint8 i, j;
		uint8 count = 0;
		uint8 au8id[8];
		for( i=0; i<2; i++ ){
			for( j=0; j<100; j++ ){
				tsReplyData sReplyData;
				uint8 id = j|(i?0x80:0x00);
				Reply_bGetData( id, &sReplyData );
				if( sReplyData.bCommand == TRUE ){
					if( sReplyData.u32LightsOutTime_sec ){
						sReplyData.u32LightsOutTime_sec--;
						DBGOUT(3, LB"! Decrement Time %d:%d", id, sReplyData.u32LightsOutTime_sec);
						if(sReplyData.u32LightsOutTime_sec==0){
							sReplyData.u16RGBW = 0;
							au8id[count] = id;
							count++;

							DBGOUT(3, LB"! TurnOff %d", id);
						}
						Reply_bSetData(id, &sReplyData);

						if(count == 8){
							Reply_bSendShareData( au8id, 8, TOCONET_MAC_ADDR_BROADCAST );
							count = 0;
						}
					}
				}
			}
		}
		if(count){
			Reply_bSendShareData( au8id, count, TOCONET_MAC_ADDR_BROADCAST );
		}
	}
	return;
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
		_C{

		}
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
	switch (eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		// send this event to the local event machine.
		ToCoNet_Event_Process(eEvent, u32arg, vProcessEvCore);
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
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
	DBGOUT(3, "Rx packet (cm:%02x, fr:%08x, to:%08x)"LB, psRx->u8Cmd,
			psRx->u32SrcAddr, psRx->u32DstAddr);

	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE() && !IS_APPCONF_OPT_RCV_NOSECURE()) {
		if (!psRx->bSecurePkt) {
			return;
		}
	}

	if( psRx->u8Cmd == TOCONET_PACKET_CMD_APP_PAL_REPLY ){
		Reply_bReceiveReplyData( psRx );
		return;
	}

	if(psRx->u8Cmd == TOCONET_PACKET_CMD_APP_MWX){
		DBGOUT(3, "Act"LB);
		vReceiveActData(psRx);
	}else{
		uint8* p = psRx->auData;
		uint16 u16Ver = G_BE_WORD();
		if( ((u16Ver>>8)&0x7F) == 'R' || ((u16Ver>>8)&0x7F) == 'T' ){
			DBGOUT(3, "PAL/TAG"LB);
			vReceiveNwkMsg(psRx);
		}else if( (u16Ver&0x00FF) == APP_TWELITE_PROTOCOL_VERSION ){
			DBGOUT(3, "TWELITE"LB);
			vReceive_AppTwelite(psRx);
		}else if( (u16Ver&0x00FF) == APP_IO_PROTOCOL_VERSION && psRx->u8Len == 18 ){
			DBGOUT(3, "IO"LB);
			vReceive_AppIO(psRx);
		}else if( (u16Ver&0x001F) == APP_UART_PROTOCOL_VERSION ){
			DBGOUT(3, "Uart"LB);
			vReceive_AppUart(psRx);
		}
	}
}

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	// UART 送信の完了チェック
	if (sSerSeqTx.bWaitComplete) {
		if (u8CbId >= sSerSeqTx.u8Seq
				&& u8CbId < sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum) {
			uint8 idx = u8CbId - sSerSeqTx.u8Seq;
			if (bStatus) {
				sSerSeqTx.bPktStatus[idx] = 1;
			} else {
				if (sSerSeqTx.bPktStatus[idx] == 0) {
					sSerSeqTx.bPktStatus[idx] = -1;
				}
			}
		}

		int i, isum = 0;
		for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
			if (sSerSeqTx.bPktStatus[i] == 0)
				break;
			isum += sSerSeqTx.bPktStatus[i];
		}

		if (i == sSerSeqTx.u8PktNum) {
			/* 送信完了 (MAC レベルで成功した) */
			sSerSeqTx.bWaitComplete = FALSE;

			// VERBOSE MESSAGE
			DBGOUT(3, "* >>> MacAck%s(tick=%d,req=#%d) <<<" LB,
					(isum == sSerSeqTx.u8PktNum) ? "" : "Fail",
					u32TickCount_ms & 65535, sSerSeqTx.u8ReqNum);
		}
	}
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
void vInitAppParent() {
	psCbHandler = &sCbHandler;
	pvProcessEv = vProcessEvCore;
	pvProcessSerialCmd = vProcessSerialCmd;
}

/** @ingroup MASTER
 * Actから来たパケットをシリアル通信アプリの書式拡張形式でシリアル出力する
 *
 * @param pRx 受信データ
 */
static void vReceiveActData(tsRxDataApp *pRx) {
	int i;
	uint8* p = pRx->auData;

	uint8 u8ActHeader = G_OCTET();
	uint8 u8lid = G_OCTET();
	uint32 u32addr = G_BE_DWORD();
	uint32 u32addr_dst = G_BE_DWORD();

	uint8 u8rpt = G_OCTET();(void)u8rpt;

	// Act からのパケットじゃなかったら処理しない
	if( u8ActHeader != 0x01 ){
		return;
	}

	// 親機からだったら処理しない
	if( u8lid == sAppData.u8AppLogicalId ){
		return;
	}

	// 以下の条件がすべて当てはまったら受信しない
	if( u32addr_dst != sAppData.u8AppLogicalId &&		// 自分のLID宛じゃない
		u32addr_dst != 0xFF &&						// ブロードキャストじゃない
		u32addr_dst != ToCoNet_u32GetSerial() ){	// 自分のSID宛じゃない
		return;
	}

	if(pRx->u8Len < (p - pRx->auData) ){
		return;
	}

	if( ToCoNet_DupChk_bAdd(psDupChk, u32addr, pRx->u8Seq ) ){
		return;
	}

	if(!TWEINTRCT_bIsVerbose()){
		uint8 u8len = pRx->u8Len - (p - pRx->auData) ;
		uint8* q = sSerCmdOut.au8data;
	
		S_OCTET(u8lid);				// 論理デバイスID
		S_OCTET(0xAA);				// 識別子
		S_OCTET(0x00);				// 応答IDにしたかった
		S_BE_DWORD(u32addr);		// 送信元シリアルID
		S_BE_DWORD(u32addr_dst);	// 送信先シリアルID
		S_OCTET(pRx->u8Lqi);		// LQI
		S_BE_WORD(u8len);	// ペイロードの長さ
		for( i=0; i<u8len; i++ ){
			uint8 u8data = G_OCTET();
			S_OCTET(u8data);
		}

		sSerCmdOut.u16len = q - sSerCmdOut.au8data;
		sSerCmdOut.vOutput( &sSerCmdOut, &sSer );

		sSerCmdOut.u16len = 0;
		memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
	}
	return;
}

/***
 * 標準アプリの受信処理
 ***/
static void vReceive_AppTwelite(tsRxDataApp *pRx) {
	uint8* p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier)
		return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_TWELITE_PROTOCOL_VERSION)
		return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();
	(void) u8AppLogicalId_Dest;

	uint16 u16TimeStamp = G_BE_WORD();

	bool_t bQuick = u16TimeStamp & 0x8000 ? TRUE : FALSE; // 優先パケット（全部処理する）
	u16TimeStamp &= 0x7FFF;

	// 宛先の確認
	if( u8AppLogicalId_Dest != sAppData.u8AppLogicalId ){
		return;
	}

	/* 重複の確認を行う */
	if(pRx->u8Cmd != TOCONET_PACKET_CMD_APP_DATA){
		if(!bQuick && ToCoNet_DupChk_bAdd( psDupChk, u32Addr, pRx->u8Seq )){
			DBGOUT(3, "Dup,");
			return;
		}
	}

	// 中継フラグ
	uint8 u8TxFlag = G_OCTET();(void)u8TxFlag;

	switch ( pRx->u8Cmd ){
	// 0x01 コマンド
	case TOCONET_PACKET_CMD_APP_DATA:
		_C{
			uint8 u8req = G_OCTET();
			uint8 u8pktnum = G_OCTET();
			uint8 u8idx = G_OCTET();
			uint16 u16offset = G_BE_WORD();
			uint8 u8len = G_OCTET();

			// 受信パケットのチェック。
			//  - 分割パケットが混在したような場合は、新しい系列で始める。
			//    複数のストリームを同時受信できない！
			bool_t bNew = FALSE;
			if (sSerSeqRx.bWaitComplete) {
				// exceptional check
				if (u32TickCount_ms - sSerSeqRx.u32Tick > 2000) {
					// time out check
					bNew = TRUE;
				}
				if (u8req != sSerSeqRx.u8ReqNum) {
					// different request number is coming.
					bNew = TRUE;
				}
				if (u32Addr != sSerSeqRx.u32SrcAddr) {
					// packet comes from different nodes. (discard this one!)
					bNew = TRUE;
				}
			} else {
				// 待ち状態ではないなら新しい系列
				bNew = TRUE;
			}

			if (bNew) {
				// treat this packet as new, so clean control buffer.
				memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
			}

			if (!sSerSeqRx.bWaitComplete) {
				// 新しいパケットを受信した

				// 最初にリクエスト番号が適切かどうかチェックする。
				if(ToCoNet_DupChk_bAdd( psDupChk, u32Addr, u8req )){
					DBGOUT(3, "Dup,");
					bNew = FALSE;
				}


				if (bNew) {
					sSerSeqRx.bWaitComplete = TRUE;
					sSerSeqRx.u32Tick = u32TickCount_ms;
					sSerSeqRx.u32SrcAddr = u32Addr;
					sSerSeqRx.u8PktNum = u8pktnum;
					sSerSeqRx.u8ReqNum = u8req;

					sSerSeqRx.u8IdSender = u8AppLogicalId;
					sSerSeqRx.u8IdReceiver = u8AppLogicalId_Dest;
				}
			}

			if (sSerSeqRx.bWaitComplete) {
				if (u16offset + u8len <= sizeof(au8SerBuffRx)
					&& u8idx < sSerSeqRx.u8PktNum) {
					// check if packet size and offset information is correct,
					// then copy data to buffer and mark it as received.
					if (!sSerSeqRx.bPktStatus[u8idx]) {
						sSerSeqRx.bPktStatus[u8idx] = 1;
						memcpy(au8SerBuffRx + u16offset, p, u8len);
					}

					// the last packet indicates full data length.
					if (u8idx == sSerSeqRx.u8PktNum - 1) {
						sSerSeqRx.u16DataLen = u16offset + u8len;
					}

					// 中継パケットのフラグを格納する
					if (u8TxFlag) {
						if (u8TxFlag > sSerSeqRx.bRelayPacket) {
							sSerSeqRx.bRelayPacket = u8TxFlag;
						}
					}
				}

				// check completion
				int i;
				for (i = 0; i < sSerSeqRx.u8PktNum; i++) {
					if (sSerSeqRx.bPktStatus[i] == 0)
						break;
				}
				if (i == sSerSeqRx.u8PktNum && !TWEINTRCT_bIsVerbose()) {
					// 分割パケットが全て届いた！

					uint8* q = sSerCmdOut.au8data;

					S_OCTET( sSerSeqRx.u8IdSender );
					S_OCTET( au8SerBuffRx[1] );

					for(i=2; i<sSerSeqRx.u16DataLen; i++){
						S_OCTET(au8SerBuffRx[i]);
					}
					sSerCmdOut.u16len = q - sSerCmdOut.au8data;
				}
			}
		}
		break;

	// 0x81 コマンド
	case TOCONET_PACKET_CMD_APP_USER_IO_DATA:
		_C{
			/* 電圧 */
			uint16 u16Volt = G_BE_WORD();(void)u16Volt;

			/* 温度 */
			int8 i8Temp = (int8)G_OCTET();(void)i8Temp;

			/* BUTTON */
			uint8 u8ButtonState = G_OCTET();
			bool_t bRegular = !!(u8ButtonState & 0x80); // 通常送信パケットであることを意味する
			uint8 u8ButtonChanged = G_OCTET();
			bool_t bRespReq = !!(u8ButtonChanged & 0x80);(void)bRespReq; // 速やかな応答を要求する

			// ポートの値を設定する（変更フラグのあるものだけ）
			if (u8ButtonChanged & 0x01) {
				vDoSet_TrueAsLo( PORT_OUT1, u8ButtonState & 0x01);
			}
#ifndef USE_MONOSTICK
			if (u8ButtonChanged & 0x02) {
				vDoSet_TrueAsLo( PORT_OUT2, u8ButtonState & 0x02);
			}
#endif

			uint8 i;

			/* ADC 値 */
			uint16 au16OutputDAC[4];
			for (i = 0; i < 4; i++) {
				// 8bit scale
				au16OutputDAC[i] = G_OCTET();
				if (au16OutputDAC[i] == 0xFF) {
					au16OutputDAC[i] = 0xFFFF;
				} else {
					// 10bit scale に拡張
					au16OutputDAC[i] <<= 2;
				}
			}
			uint8 u8DAC_Fine = G_OCTET();
			for (i = 0; i < 4; i++) {
				if (au16OutputDAC[i] != 0xFFFF) {
					// 下２ビットを復旧
					au16OutputDAC[i] |= (u8DAC_Fine & 0x03);
				}
				u8DAC_Fine >>= 2;
			}

			// ADC 値を PWM の DUTY 比に変換する
			// 下は 5%, 上は 10% を不感エリアとする。
#ifdef USE_MONOSTICK
			uint16 u16Adc = au16OutputDAC[2];
#else
			uint16 u16Adc = au16OutputDAC[0];
#endif
			uint16 u16OutputPWMDuty = 0;
			u16Adc <<= 2; // 以下の計算は 12bit 精度
			if (u16Adc > ADC_MAX_THRES) { // 最大レンジの 98% 以上なら、未定義。
				u16OutputPWMDuty = 0xFFFF;
			} else {
				// 10bit+1 スケール正規化
				int32 iS;
				int32 iR = (uint32) u16Adc * 2 * 1024 / u16Volt; // スケールは 0～Vcc/2 なので 2倍する
				// y = 1.15x - 0.05 の線形変換
				//   = (115x-5)/100 = (23x-1)/20 = 1024*(23x-1)/20/1024 = 51.2*(23x-1)/1024 ~= 51*(23x-1)/1024
				// iS/1024 = 51*(23*iR/1024-1)/1024
				// iS      = (51*23*iR - 51*1024)/1024
				iS = 51 * 23 * iR - 51 * 1024;
				if (iS <= 0) {
					iS = 0;
				} else {
					iS >>= 10; // 1024での割り算
					if (iS >= 1024) { // DUTY は 0..1024 で正規化するので最大値は 1024。
						iS = 1024;
					}
				}
				u16OutputPWMDuty = iS;
			}

			if (u16OutputPWMDuty != 0xFFFF) {
					sTimerPWM.u16duty = _PWM(u16OutputPWMDuty);
				if (sTimerPWM.bStarted) {
					vTimerStart(&sTimerPWM); // DUTY比だけ変更する
				}
			}

			/* UART 出力 */
			if (!TWEINTRCT_bIsVerbose()) {
				if (IS_APPCONF_OPT_REGULAR_PACKET_NO_DISP() && bRegular) {
					; // 通常パケットの場合の出力抑制設定
				} else {
					// 以下のようにペイロードを書き換えて UART 出力
					pRx->auData[0] = pRx->u8Len; // １バイト目はバイト数
					pRx->auData[2] = pRx->u8Lqi; // ３バイト目(もともとは送信元の LogicalID) は LQI

					uint8* q = sSerCmdOut.au8data;

					S_OCTET( u8AppLogicalId );
					S_OCTET( SERCMD_ID_INFORM_IO_DATA );

					for(i=0; i<pRx->u8Len; i++){
						S_OCTET(pRx->auData[i]);
					}

					sSerCmdOut.u16len = q - sSerCmdOut.au8data;
				}
			}
		}
		break;

	// 0x80 コマンド
	case TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT:
		_C{
			/* 書式 */
			uint8 u8Format = G_OCTET();

			if (u8Format == 1) {
				/* BUTTON */
				uint8 u8ButtonState = G_OCTET();
				uint8 u8ButtonChanged = G_OCTET();
				// ポートの値を設定する（変更フラグのあるものだけ）
				if (u8ButtonChanged & 0x01) {
					vDoSet_TrueAsLo( PORT_OUT1, u8ButtonState & 0x01);
				}
#ifndef USE_MONOSTICK
				if (u8ButtonChanged & 0x02) {
					vDoSet_TrueAsLo( PORT_OUT2, u8ButtonState & 0x02);
				}
#endif

				uint16 u16OutputPWMDuty = 0;
				uint8 i;
				for (i = 0; i < 4; i++) {
					uint16 u16Duty = G_BE_WORD();
#ifdef USE_MONOSTICK
					if(i == 2){
#else
					if(i == 0){
#endif
						if (u16Duty <= 1024) {
							u16OutputPWMDuty = u16Duty;
						} else {
							u16OutputPWMDuty = 0xFFFF;
						}
					}
				}

				// PWM の再設定
				if (u16OutputPWMDuty != 0xFFFF) {
					sTimerPWM.u16duty = _PWM( u16OutputPWMDuty );
					if (sTimerPWM.bStarted)
						vTimerStart(&sTimerPWM); // DUTY比だけ変更する
				}
			}
		}
		return;

	default:
		return;
	}

	if( sSerCmdOut.u16len > 0 && !TWEINTRCT_bIsVerbose() ) {
		sSerCmdOut.vOutput( &sSerCmdOut, &sSer );

		sSerCmdOut.u16len = 0;
		memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
		memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
	}

}

/** @ingroup MASTER
 * シリアルメッセージの受信処理。分割パケットがそろった時点で UART に出力。
 * - 中継機なら新たな系列として中継。
 *   - 中継機と送信元が両方見える場合は、２つの系列を受信を行う事になる。
 * tsRxDataApp *pRx
 */
static void vReceive_AppUart(tsRxDataApp *pRx) {
	uint8 *p = pRx->auData;

	/* ヘッダ情報の読み取り */
	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != (sAppData.u8AppIdentifier&0xFE) ) { return; }
	uint8 u8PtclVersion = G_OCTET();
	uint8 u8RepeatFlag = u8PtclVersion >> 6;
	u8PtclVersion &= 0x1F;
	if (u8PtclVersion != APP_UART_PROTOCOL_VERSION) { return; }
	uint8 u8RespId = G_OCTET();
	uint8 u8AppLogicalId = G_OCTET(); (void)u8AppLogicalId;
	uint32 u32AddrSrc = pRx->bNwkPkt ? pRx->u32SrcAddr : G_BE_DWORD();
	uint8 u8AppLogicalId_Dest = G_OCTET();
	uint32 u32AddrDst = pRx->bNwkPkt ? pRx->u32DstAddr : G_BE_DWORD();

	/* ここから中身 */
	uint8 u8req = G_OCTET();
	uint8 u8pktsplitinfo = G_OCTET();

	uint8 u8pktnum = u8pktsplitinfo & 0xF;
	uint8 u8idx = (u8pktsplitinfo & 0xF0) >> 4;

	uint8 u8opt = G_OCTET();

	uint8 u8len = (pRx->auData + pRx->u8Len) - p;
	uint16 u16offset = u8idx * SERCMD_SER_PKTLEN;

	/* 宛先と送信元のアドレスが一致する場合は処理しない */
	if (u32AddrSrc == u32AddrDst) return;
	if (u8AppLogicalId == u8AppLogicalId_Dest && u8AppLogicalId < 0x80) return;

	/* 宛先によって処理するか決める */
	bool_t bAcceptAddress = TRUE;

	if ( u8AppLogicalId_Dest == 0x80 ) {
		// 拡張アドレスの場合 (アドレスによってパケットを受理するか決定する)
		if (u32AddrDst == TOCONET_MAC_ADDR_BROADCAST || u32AddrDst == TOCONET_NWK_ADDR_BROADCAST) {
		} else if (u32AddrDst < 0xFFFF) {
			// ショートアドレス形式 (アドレスの一致が原則)
			if (u32AddrDst != sToCoNet_AppContext.u16ShortAddress) {
				bAcceptAddress = FALSE;
			}
		} else if (u32AddrDst & 0x80000000) {
			// 拡張アドレス (アドレスの一致が原則)
			if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)
					&& u32AddrDst == TOCONET_NWK_ADDR_PARENT
			) {
				// 親機で拡張親機アドレス
				bAcceptAddress = TRUE;
			} else
			if (u32AddrDst != ToCoNet_u32GetSerial()) {
				bAcceptAddress = FALSE;
			}
		} else {
			bAcceptAddress = FALSE;
		}
	} else if (u8AppLogicalId_Dest == LOGICAL_ID_BROADCAST) {
		// ブロードキャストは受信する
		bAcceptAddress = (u32AddrSrc == ToCoNet_u32GetSerial()) ? FALSE : TRUE;
	} else {
		//  簡易アドレスが宛先で、親機の場合
		if (u8AppLogicalId_Dest != LOGICAL_ID_PARENT) {
			// 親機同士の通信はしない
			bAcceptAddress = FALSE;
		}
	}

	// 受信パケットのチェック。
	//  - 分割パケットが混在したような場合は、新しい系列で始める。
	//    複数のストリームを同時受信できない！
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
			// 重複しいたら新しい人生は始めない
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

		if (!TWEINTRCT_bIsVerbose() && i == sSerSeqRx.u8PktNum) {
			// 分割パケットが全て届いた！

			// 自分宛のメッセージなら、UART に出力する
			if (bAcceptAddress) {
				// 受信データの出力
				uint8 *q = sSerCmdOut.au8data;
				if (u8opt == 0xA0) {
					// 拡張形式のパケットの出力
					// 以下ヘッダは１４バイト
					S_OCTET(sSerSeqRx.u8IdSender);
					S_OCTET(u8opt);
					S_OCTET(sSerSeqRx.u8RespID);
					S_BE_DWORD(u32AddrSrc);
					S_BE_DWORD(u32AddrDst);
					S_OCTET(pRx->u8Lqi);
					S_OCTET(sSerSeqRx.u16DataLen >> 8);
					S_OCTET(sSerSeqRx.u16DataLen & 0xFF);

				} else {
					// 以下ヘッダは２バイト
					S_OCTET(sSerSeqRx.u8IdSender);
					S_OCTET(u8opt);
				}

				int j = 0;
				for( j=0; j<sSerSeqRx.u16DataLen; j++ ){
					S_OCTET( au8SerBuffRx[j] );
				}

				// UART 出力を行う
				sSerCmdOut.u16len = q-sSerCmdOut.au8data;
				sSerCmdOut.vOutput(&sSerCmdOut, &sSer);
				sSerCmdOut.u16len = 0;
				memset(sSerCmdOut.au8data, 0, sizeof(sSerCmdOut.au8data));
			}
			memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
		}
	}
}

/***
 * App_IO の受信処理
 ***/
static void vReceive_AppIO(tsRxDataApp *pRx) {
	uint8 *p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_IO_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();
	uint32 u32Addr = G_BE_DWORD();
	uint8 u8AppLogicalId_Dest = G_OCTET(); (void)u8AppLogicalId_Dest;
	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	bool_t bQuick = u16TimeStamp & 0x8000 ? TRUE : FALSE; // 優先パケット（全部処理する）
	u16TimeStamp &= 0x7FFF;
	if (bQuick == FALSE && ToCoNet_DupChk_bAdd( psDupChk, u32Addr, pRx->u8Seq )) {
		DBGOUT(3, "Dup,");
		return;
	}
	static uint32 u32LastQuick;
	if (bQuick) {
		if ((u32TickCount_ms - u32LastQuick) < 20) {
			// Quickパケットを受けて一定期間未満のパケットは無視する
			return;
		} else {
			u32LastQuick = u32TickCount_ms; // タイムスタンプを巻き戻す
		}
	}

	// 中継フラグ
	uint8 u8TxFlag = G_OCTET();(void)u8TxFlag;

	// 親機子機の判定
	if ( u8AppLogicalId == sAppData.u8AppLogicalId ) {
		return;
	}

	/* UART 出力 */
	if ( !IS_APPCONF_OPT_REGULAR_PACKET_NO_DISP() ) {
		// 以下のようにペイロードを書き換えて UART 出力
		pRx->auData[0] = pRx->u8Len; // １バイト目はバイト数
		pRx->auData[2] = pRx->u8Lqi; // ３バイト目(もともとは送信元の LogicalID) は LQI

		uint8* q = sSerCmdOut.au8data;
		S_OCTET(u8AppLogicalId);
		S_OCTET(SERCMD_ID_INFORM_IO_DATA);
		int i = 0;
		for(i=0; i<pRx->u8Len; i++){
			S_OCTET(pRx->auData[i]);
		}

		sSerCmdOut.u16len = q - sSerCmdOut.au8data;
		sSerCmdOut.vOutput( &sSerCmdOut, &sSer );

		sSerCmdOut.u16len = 0;
		memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
	}
}

/**
 * 子機または中継機を経由したデータを受信する。
 *
 * - UART に指定書式で出力する
 *
 * @param pRx 受信データ構造体
 */
void vReceiveNwkMsg(tsRxDataApp *pRx) {
	tsRxPktInfo sRxPktInfo;

	uint8 *p = pRx->auData;

	// パケットの表示
	if (pRx->u8Cmd == TOCONET_PACKET_CMD_APP_DATA) {
		// 基本情報
		sRxPktInfo.u8lqi_1st = pRx->u8Lqi;
		sRxPktInfo.u32addr_1st = pRx->u32SrcAddr;

		// データの解釈
		uint8 u8b = G_OCTET();

		// PALからのパケットかどうかを判定する
		bool_t bPAL = u8b&0x80 ? TRUE:FALSE;
		u8b = u8b&0x7F;

		// 違うデータなら表示しない
		if( u8b != 'T' && u8b != 'R' ){
			return;
		}


		// 受信機アドレス
		sRxPktInfo.u32addr_rcvr = TOCONET_NWK_ADDR_PARENT;
		if (u8b == 'R') {
			// ルータからの受信
			sRxPktInfo.u32addr_1st = G_BE_DWORD();
			sRxPktInfo.u8lqi_1st = G_OCTET();

			sRxPktInfo.u32addr_rcvr = pRx->u32SrcAddr;
		}

		// ID などの基本情報
		sRxPktInfo.u8id = G_OCTET();
		sRxPktInfo.u16fct = G_BE_WORD();

		// パケットの種別により処理を変更
		sRxPktInfo.u8pkt = G_OCTET();

		// インタラクティブモードだったら何も出力しない
		if(!TWEINTRCT_bIsVerbose()){
			// 出力用の関数を呼び出す
			if(bPAL){
				vSerOutput_PAL(sRxPktInfo, p);
			}else{
				vSerOutput_Tag(sRxPktInfo, p);
			}
		}
	}
}

/**
 * UART形式の出力 (PAL)
 */
void vSerOutput_PAL(tsRxPktInfo sRxPktInfo, uint8 *p) {
	uint8* q = sSerCmdOut.au8data; // 出力バッファ
	bool_t bDup = ToCoNet_DupChk_bAdd(psDupChk, sRxPktInfo.u32addr_1st, (uint8)sRxPktInfo.u16fct );

	tsSnsData sSnsData;
	memset( &sSnsData, 0x00, sizeof(tsSnsData) );
	sSnsData.u8Event = 0xFF;

	// 受信機のアドレス
	S_BE_DWORD(sRxPktInfo.u32addr_rcvr);

	// LQI
	S_OCTET(sRxPktInfo.u8lqi_1st);

	// フレーム
	S_BE_WORD(sRxPktInfo.u16fct);

	// 送信元子機アドレス
	S_BE_DWORD(sRxPktInfo.u32addr_1st);
	S_OCTET(sRxPktInfo.u8id);

	// パケットの種別により処理を変更
	S_OCTET(0x80);
	S_OCTET(sRxPktInfo.u8pkt);

	uint8 u8Length = G_OCTET();
	S_OCTET(u8Length);
	uint8 i = 0;

	sSnsData.u8LID = sRxPktInfo.u8id;
	sSnsData.u8SNSID = sRxPktInfo.u8pkt&0x1F;

	bool_t bReplyFlag = FALSE;

	while( i<u8Length ){
		uint8 u8Sensor = G_OCTET();

		switch(u8Sensor){
			case HALLIC:
				_C{
					uint8 u8num = G_OCTET();(void)u8num;
					uint8 u8Status = G_OCTET();
					S_OCTET(UNUSE_EXBYTE|TYPE_UNSIGNED|TYPE_CHAR);
					S_OCTET(u8Sensor);
					S_OCTET(0x00);
					S_OCTET(0x01);
					S_OCTET(u8Status);

					sSnsData.u8HallIC = u8Status;
				}
				break;
			case TEMP:
				_C{
					uint8 u8num = G_OCTET();(void)u8num;
					int16 i16temp = G_BE_WORD();

					if(i16temp == -32767 || i16temp == -32768){
						S_OCTET(ERROR|( (i16temp == -32767)?0x01:0x00 ));
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x00);
					}else{
						S_OCTET(UNUSE_EXBYTE|TYPE_SIGNED|TYPE_SHORT);
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x02);
						S_BE_WORD(i16temp);
					}
					sSnsData.i16Temp = i16temp;
				}
				break;
			case HUM:
				_C{
					uint8 u8num = G_OCTET();(void)u8num;
					uint16 u16hum = G_BE_WORD();

					if( u16hum == 0x8001 || u16hum == 0x8000 ){
						S_OCTET(ERROR|( (u16hum == 0x8001)?0x01:0x00 ));
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x00);
					}else{
						S_OCTET(UNUSE_EXBYTE|TYPE_UNSIGNED|TYPE_SHORT);
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x02);
						S_BE_WORD(u16hum);
					}
					sSnsData.u16Hum = u16hum;
				}
				break;
			case ILLUM:
				_C{
					uint8 u8num = G_OCTET();(void)u8num;
					uint32 u32illum = G_BE_DWORD();

					if(u32illum == 0xFFFFFFFE || u32illum == 0xFFFFFFFF ){
						S_OCTET(ERROR|((u32illum == 0xFFFFFFFE)?0x01:0x00));
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x00);
					}else{
						S_OCTET(UNUSE_EXBYTE|TYPE_UNSIGNED|TYPE_LONG);
						S_OCTET(u8Sensor);
						S_OCTET(0x00);
						S_OCTET(0x04);
						S_BE_DWORD(u32illum);
					}
					sSnsData.u32Illumi = u32illum;
				}
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
						int16 X[2], Y[2], Z[2];

						uint8 tmp = G_OCTET(); X[0] = tmp<<4;
						tmp = G_OCTET(); X[0] |= (tmp>>4); Y[0] = (tmp&0x0F)<<8;
						tmp = G_OCTET(); Y[0] |= tmp;
						tmp = G_OCTET(); Z[0] = tmp<<4;
						tmp = G_OCTET(); Z[0] |= (tmp>>4); X[1] = (tmp&0x0F)<<8;
						tmp = G_OCTET(); X[1] |= tmp;
						tmp = G_OCTET(); Y[1] = tmp<<4;
						tmp = G_OCTET(); Y[1] |= (tmp>>4); Z[1] = (tmp&0x0F)<<8;
						tmp = G_OCTET(); Z[1] |= tmp;

						uint8 k;
						for( k=0; k<2; k++ ){
							S_OCTET(USE_EXBYTE|TYPE_SIGNED|TYPE_SHORT);
							S_OCTET(u8Sensor);
							S_OCTET((u8Sampling|(j+k)));
							S_OCTET(0x06);

							// 符号があれば上位4ビットをFで埋める
							X[k] = (X[k]&0x0800) ? (X[k]|0xF000)*8:X[k]*8;
							Y[k] = (Y[k]&0x0800) ? (Y[k]|0xF000)*8:Y[k]*8;
							Z[k] = (Z[k]&0x0800) ? (Z[k]|0xF000)*8:Z[k]*8;
							S_BE_WORD(X[k]);
							S_BE_WORD(Y[k]);
							S_BE_WORD(Z[k]);
							sSnsData.au16Accle[0][j*k] = X[k];
							sSnsData.au16Accle[1][j*k] = Y[k];
							sSnsData.au16Accle[2][j*k] = Z[k];
						}


						j += 2;
					}
					i += (u8Num-1);
				}
				break;
			case EVENT:
				_C{
					uint8 Sns = G_OCTET();
					uint8 Event = G_OCTET();
					S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_LONG);
					S_OCTET(u8Sensor);
					S_OCTET(Sns);
					S_OCTET(0x04);
					S_OCTET(Event);
					if(Sns&0x80){
						uint8 temp = G_OCTET();
						S_OCTET( temp );
						temp = G_OCTET();
						S_OCTET( temp );
						temp = G_OCTET();
						S_OCTET( temp );
					}else{
						S_OCTET(0x00);
						S_OCTET(0x00);
						S_OCTET(0x00);
					}
					sSnsData.u8Event = Event;
				}
				break;
			case LED:	// ここは要修正
				_C{
					uint8 state = G_OCTET();
					S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_CHAR);
					S_OCTET(u8Sensor);
					S_OCTET(0x01);
					S_OCTET(0x01);
					S_OCTET(state);
				}
				break;
			case ADC:
				_C{
					uint8 u8num = G_OCTET();
					uint16 u16ADC = 0;
					if(u8num == 0x01 || u8num == 0x08){
						u8num = 0x08;
						uint8 u8Pwr = G_OCTET();
						u16ADC = DECODE_VOLT(u8Pwr);
					}else{
						u8num--;
						u16ADC = G_BE_WORD();
					}
					S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_SHORT);
					S_OCTET(u8Sensor);
					S_OCTET(u8num);
					S_OCTET(0x02);
					S_BE_WORD(u16ADC);
				}
				break;
			case DIO:
				_C{
					uint8	u8num = G_OCTET();
					uint32	u32DIO;
					if(u8num <= 8){
						u32DIO = G_OCTET();
						S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_CHAR);
					}else if(u8num<=16){
						u32DIO = G_BE_WORD();
						S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_SHORT);
					}else{
						u32DIO = G_BE_DWORD();
						S_OCTET(USE_EXBYTE|TYPE_UNSIGNED|TYPE_LONG);
					}
					S_OCTET(u8Sensor);
					S_OCTET(u8num);
					if(u8num <= 8){
						S_OCTET(0x01);
						S_OCTET(u32DIO&0xFF);
					}else if(u8num<=16){
						S_OCTET(0x02);
						S_BE_WORD(u32DIO&0xFFFF);
					}else{
						S_OCTET(0x04);
						S_BE_DWORD(u32DIO);
					}				
				}
				break;
			case EEPROM:
				_C{
					uint8 u8num = G_OCTET();
					uint8 u8Status = G_OCTET();
					S_OCTET(0x80|(u8Status&0x7F));
					S_OCTET(u8Sensor);
					S_OCTET(u8num);
					S_OCTET(0x00);
				}
				break;
			case REPLY:
				bReplyFlag = TRUE;
				sSerCmdOut.au8data[14]--;
				break;
			case FACTOR:
				_C{
					uint8 u8pktid = G_OCTET();
					uint8 u8device = G_OCTET();
					uint8 u8factor = G_OCTET();

					S_OCTET(TYPE_UNSIGNED|TYPE_CHAR);
					S_OCTET(u8Sensor);
					S_OCTET(0x00)
					S_OCTET(3);
					S_OCTET(u8pktid);
					S_OCTET(u8device);
					S_OCTET(u8factor);
				}
				break;

			default:
				break;
		}

		i++;
	}
	uint8 u8crc = u8CCITT8( sSerCmdOut.au8data, q-sSerCmdOut.au8data );
	S_OCTET(u8crc);

	sSerCmdOut.u16len = q - sSerCmdOut.au8data;
	sSerCmdOut.vOutput( &sSerCmdOut, &sSer );

	if( !bDup && sSnsData.u8Event != 0xFF ){
		vSetReplyData(&sSnsData);
	}

	// LED PALからのパケットで中継されていない場合は送り返す
	if( bReplyFlag && (sRxPktInfo.u32addr_rcvr == 0x80000000) ){
		Reply_bSendData(sRxPktInfo);
	}

	sSerCmdOut.u16len = 0;
	memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
}

/**
 * UART形式の出力 (TAG)
 */
void vSerOutput_Tag(tsRxPktInfo sRxPktInfo, uint8 *p) {
	uint8* q = sSerCmdOut.au8data; // 出力バッファ

	// 受信機のアドレス
	S_BE_DWORD(sRxPktInfo.u32addr_rcvr);

	// LQI
	S_OCTET(sRxPktInfo.u8lqi_1st);

	// フレーム
	S_BE_WORD(sRxPktInfo.u16fct);

	// 送信元子機アドレス
	S_BE_DWORD(sRxPktInfo.u32addr_1st);
	S_OCTET(sRxPktInfo.u8id);

	// パケットの種別により処理を変更
	S_OCTET(sRxPktInfo.u8pkt);

	switch(sRxPktInfo.u8pkt) {
	//	温度センサなど
	case PKT_ID_STANDARD:
	case PKT_ID_LM61:
	case PKT_ID_SHT21:
	case PKT_ID_SHT31:
	case PKT_ID_SHTC3:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();
			uint16	u16humi = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);
			S_BE_WORD(u16humi);
		}

		if (sRxPktInfo.u8pkt == PKT_ID_LM61) {
			int16	bias = G_BE_WORD();
			S_BE_WORD( bias );
		}
		break;

	case PKT_ID_MAX31855:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int32	i32temp = G_BE_DWORD();
			int32	i32itemp = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i32temp);
			S_BE_WORD(i32itemp);
		}
		break;

	case PKT_ID_ADT7410:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);
		}
		break;

	case PKT_ID_BME280:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();
			uint16	u16hum = G_BE_WORD();
			uint16	u16atmo = G_BE_WORD();

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);		//	Result
			S_BE_WORD(u16hum);		//	Result
			S_BE_WORD(u16atmo);		//	Result
		}
		break;

	case PKT_ID_MPL115A2:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			uint16	u16atmo = G_BE_WORD();

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16atmo);		//	Result
		}
		break;

	case PKT_ID_LIS3DH:
	case PKT_ID_ADXL345:
	case PKT_ID_L3GD20:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16x = G_BE_WORD();
			int16	i16y = G_BE_WORD();
			int16	i16z = G_BE_WORD();

			uint8 u8ActTapSource = ( u16adc0>>12 )|((u16adc1>>8)&0xF0);(void)u8ActTapSource;

			u16adc0 = u16adc0&0x0FFF;
			u16adc1 = u16adc1&0x0FFF;

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);

			if( sRxPktInfo.u8pkt == PKT_ID_ADXL345 ){
				uint8 u8mode = G_OCTET();
				S_OCTET(u8mode);

				if(u8mode == 0xfa){
					uint8 u8num = G_OCTET();
					S_OCTET(u8num);
					S_BE_WORD(i16x);		//	1回目は先に表示
					S_BE_WORD(i16y);		//
					S_BE_WORD(i16z);		//
					uint8 i;
					for( i=0; i<u8num-1; i++ ){
						i16x = G_BE_WORD();
						i16y = G_BE_WORD();
						i16z = G_BE_WORD();
						S_BE_WORD(i16x);		//	Result
						S_BE_WORD(i16y);		//	Result
						S_BE_WORD(i16z);		//	Result
					}
				}else if(u8mode == 0xF9 ){
					uint16 u16Sample = G_BE_WORD();
					S_BE_WORD(i16x);		//	average
					S_BE_WORD(i16y);		//
					S_BE_WORD(i16z);		//
					i16x = G_BE_WORD();
					i16y = G_BE_WORD();
					i16z = G_BE_WORD();
					S_BE_WORD(i16x);		//	minimum
					S_BE_WORD(i16y);		//
					S_BE_WORD(i16z);		//
					i16x = G_BE_WORD();
					i16y = G_BE_WORD();
					i16z = G_BE_WORD();
					S_BE_WORD(i16x);		//	maximum
					S_BE_WORD(i16y);		//
					S_BE_WORD(i16z);		//
					S_BE_WORD(u16Sample);	// 今回使用したサンプル数
				}else{
					S_BE_WORD(i16x);		//	Result
					S_BE_WORD(i16y);		//	Result
					S_BE_WORD(i16z);		//	Result
				}
			}else{
				S_BE_WORD(i16x);		//	Result
				S_BE_WORD(i16y);		//	Result
				S_BE_WORD(i16z);		//	Result
			}
		}
		break;

	case PKT_ID_TSL2561:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			uint32	u32lux = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_BE_DWORD(u32lux);		//	Result
		}
		break;

	case PKT_ID_S1105902:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 u16R = G_BE_WORD();
			int16 u16G = G_BE_WORD();
			int16 u16B = G_BE_WORD();
			int16 u16I = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_BE_WORD(u16R);		//	Result
			S_BE_WORD(u16G);		//	Result
			S_BE_WORD(u16B);
			S_BE_WORD(u16I);
		}
		break;

	case PKT_ID_ADXL362:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			uint16 u16bitmap = G_BE_WORD();
			uint8 u8Interrupt = G_OCTET();
			uint8 u8num = G_OCTET();
			uint8 u8Freq = G_OCTET();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16bitmap);
			S_OCTET(u8Interrupt);
			S_OCTET(u8num);
			S_OCTET(u8Freq);

			uint8 i;
			int16 i16x,i16y,i16z;
			if( u16bitmap&0x10 ){
				uint16 SampleNum = G_BE_WORD();
				S_BE_WORD(SampleNum);

				int16 min,ave,max;
				uint32 dave;
				for( i=0; i<3; i++ ){
					min = G_BE_WORD();
					ave = G_BE_WORD();
					dave = G_BE_DWORD();
					max = G_BE_WORD();
					S_BE_WORD(min);
					S_BE_WORD(ave);
					S_BE_DWORD(dave);
					S_BE_WORD(max);
				}
			}else{
				if( u16bitmap&0x01 ){
					uint8 au8accel[9];
					for( i=0; i<u8num; i+=2 ){
						uint8 j;
						for( j=0; j<9; j++ ){
							au8accel[j] = G_OCTET();
						}
						i16x = (au8accel[0]<<4) + (au8accel[1]>>4);
						if( i16x&0x0800 ) i16x = (int16)(i16x|0xF000);
						i16y = ((au8accel[1]&0x0F)<<8) + au8accel[2];
						if( i16y&0x0800 ) i16y = (int16)(i16y|0xF000);
						i16z = (au8accel[3]<<4) + (au8accel[4]>>4);
						if( i16z&0x0800 ) i16z = (int16)(i16z|0xF000);
						S_BE_WORD(i16x*4);		//	Result
						S_BE_WORD(i16y*4);		//	Result
						S_BE_WORD(i16z*4);		//	Result

						i16x = ((au8accel[4]&0x0F)<<8) + au8accel[5];
						if( i16x&0x0800 ) i16x = (int16)(i16x|0xF000);
						i16y = (au8accel[6]<<4) + (au8accel[7]>>4);
						if( i16y&0x0800 ) i16y = (int16)(i16y|0xF000);
						i16z = ((au8accel[7]&0x0F)<<8) + au8accel[8];
						if( i16z&0x0800 ) i16z = (int16)(i16z|0xF000);
						S_BE_WORD(i16x*4);		//	Result
						S_BE_WORD(i16y*4);		//	Result
						S_BE_WORD(i16z*4);		//	Result
					}
				}else{
					for( i=0; i<u8num; i++ ){
						if( u16bitmap&0x02 ){
							i16x = G_OCTET();
							i16y = G_OCTET();
							i16z = G_OCTET();

							i16x = (i16x&0x0080) ? (i16x|0xFF00)<<6:i16x<<6;
							i16y = (i16y&0x0080) ? (i16y|0xFF00)<<6:i16y<<6;
							i16z = (i16z&0x0080) ? (i16z|0xFF00)<<6:i16z<<6;
						}else{
							i16x = G_BE_WORD();
							i16y = G_BE_WORD();
							i16z = G_BE_WORD();
						}
						S_BE_WORD(i16x);		//	Result
						S_BE_WORD(i16y);		//	Result
						S_BE_WORD(i16z);		//	Result
					}
				}
			}
		}
		break;

	//	磁気スイッチ
	case PKT_ID_IO_TIMER:
		_C {
			uint8	u8batt = G_OCTET();
			uint8	u8stat = G_OCTET();
			uint32	u32dur = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_OCTET(u8stat); // stat
			S_BE_DWORD(u32dur); // dur
		}
		break;

	case PKT_ID_UART:
		_C {
			uint8 u8len = G_OCTET();
			S_OCTET(u8len);

			uint8	tmp;
			while (u8len--) {
				tmp = G_OCTET();
				S_OCTET(tmp);
			}
		}
		break;

	//	押しボタン
	case PKT_ID_BUTTON:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			uint8	u8mode = G_OCTET();
			uint8	u8bitmap = G_OCTET();

			bool_t	bRegularTransmit = (u8mode&0x80) ? TRUE:FALSE;
			uint8	u8DO_State = 0;
			if( !bRegularTransmit ){
				if( (u8mode&0x04) || (u8mode&0x02) ){
					if(u8bitmap){
						//vPortSetLo(OUTPUT_LED);
						u8DO_State = 1;
					}else{
						//vPortSetLo(OUTPUT_LED);
						u8DO_State = 0;
						//sAppData.u32LedCt = 0;
					}
				}
			}

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_OCTET( u8mode );
			S_OCTET( u8bitmap );
			S_OCTET( u8DO_State );
		}
		break;

	case PKT_ID_MULTISENSOR:
		_C {
			uint8 u8batt = G_OCTET();
			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			uint8 u8SnsNum = G_OCTET();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_OCTET(u8SnsNum);

			uint8 u8Sensor, i;
			for( i=0; i<u8SnsNum; i++ ){
				u8Sensor = G_OCTET();
				S_OCTET(u8Sensor); // batt
				switch( u8Sensor){
					case PKT_ID_SHT21:
					{
						int16 i16temp = G_BE_WORD();
						int16 i16humd = G_BE_WORD();
						S_BE_WORD(i16temp);
						S_BE_WORD(i16humd);
					}
					break;
					case PKT_ID_ADT7410:
					{
						int16 i16temp = G_BE_WORD();
						S_BE_WORD(i16temp);
					}
					break;
					case PKT_ID_MPL115A2:
					{
						int16 i16atmo = G_BE_WORD();
						S_BE_WORD(i16atmo);
					}
					break;
					case PKT_ID_LIS3DH:
					case PKT_ID_L3GD20:
					{
						int16 i16x = G_BE_WORD();
						int16 i16y = G_BE_WORD();
						int16 i16z = G_BE_WORD();
						S_BE_WORD(i16x);
						S_BE_WORD(i16y);
						S_BE_WORD(i16z);
					}
					break;
					case PKT_ID_ADXL345:
					{
						uint8 u8mode = G_OCTET();
						S_OCTET(u8mode);
						int16 i16x, i16y, i16z;
						if( u8mode == 0xfa ){
							uint8 u8num = G_OCTET();
							uint8 j;
							for( j=0; j<u8num; j++ ){
								i16x = G_BE_WORD();
								i16y = G_BE_WORD();
								i16z = G_BE_WORD();
								S_BE_WORD(i16x);
								S_BE_WORD(i16y);
								S_BE_WORD(i16z);
							}
						}else{
							i16x = G_BE_WORD();
							i16y = G_BE_WORD();
							i16z = G_BE_WORD();
							S_BE_WORD(i16x);
							S_BE_WORD(i16y);
							S_BE_WORD(i16z);
						}
					}
					break;
					case PKT_ID_TSL2561:
					{
						uint32	u32lux = G_BE_DWORD();
						S_BE_DWORD(u32lux);
					}
					break;
					case PKT_ID_S1105902:
					{
						int16 u16R = G_BE_WORD();
						int16 u16G = G_BE_WORD();
						int16 u16B = G_BE_WORD();
						int16 u16I = G_BE_WORD();
						S_BE_WORD(u16R);
						S_BE_WORD(u16G);
						S_BE_WORD(u16B);
						S_BE_WORD(u16I);
					}
					break;
					case PKT_ID_BME280:
					{
						int16	i16temp = G_BE_WORD();
						uint16	u16hum = G_BE_WORD();
						uint16	u16atmo = G_BE_WORD();
						S_BE_WORD(i16temp);
						S_BE_WORD(u16hum);
						S_BE_WORD(u16atmo);
					}
					break;
					default:
						break;
				}
			}
		}
		break;
	default:
		break;
	}

	sSerCmdOut.u16len = q - sSerCmdOut.au8data;
	sSerCmdOut.vOutput( &sSerCmdOut, &sSer );

	sSerCmdOut.u16len = 0;
	memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
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
	uint8 *p = pSer->au8data;

	uint8 u8addr; // 送信先論理アドレス
	uint8 u8cmd; // コマンド

	uint8 *p_end;
	p_end = &(pSer->au8data[pSer->u16len]); // the end points 1 byte after the data end.

	bool_t bTransmitRfPacket = FALSE;

	// COMMON FORMAT
	OCTET(u8addr); // [1] OCTET : 論理ID
	OCTET(u8cmd); // [1] OCTET : 要求番号

	DBGOUT(3, "* UARTCMD ln=%d cmd=%02x req=%02x %02x%0x2%02x%02x..." LB,
		pSer->u16len, u8addr, u8cmd, *p, *(p + 1), *(p + 2), *(p + 3));

	if (u8addr == SERCMD_ADDR_TO_MODULE) {
		/*
		 * モジュール自身へのコマンド (0xDB)
		 */
		switch (u8cmd) {
		case SERCMD_ID_GET_MODULE_ADDRESS:
			vModbOut_MySerial(&sSer);
			break;

		case SERCMD_ID_I2C_COMMAND:
			break;

		default:
			break;
		}
	} else {
		/*
		 * 外部アドレスへの送信(IO情報の設定要求)
		 */
		if (u8cmd == SERCMD_ID_REQUEST_IO_DATA) {
			/*
			 * OCTET: 書式 (0x01)
			 * OCTET: 出力状態
			 * OCTET: 出力状態マスク
			 * BE_WORD: PWM1
			 * BE_WORD: PWM2
			 * BE_WORD: PWM3
			 * BE_WORD: PWM4
			 */
			uint8 u8format = G_OCTET();

			if (u8format == 0x01) {
				tsIOSetReq sIOreq;
				memset(&sIOreq, 0, sizeof(tsIOSetReq));

				sIOreq.u8IOports = G_OCTET();
				sIOreq.u8IOports_use_mask = G_OCTET();

				int i;

				for (i = 0; i < 4; i++) {
					sIOreq.au16PWM_Duty[i] = G_BE_WORD();
				}

				if (p_end < p)
					return; // v1.1.3 (終端チェック)

				DBGOUT(1, "SERCMD:IO REQ: %02x %02x %04x:%04x:%04x:%04x"LB,
						sIOreq.u8IOports, sIOreq.u8IOports_use_mask,
						sIOreq.au16PWM_Duty[0], sIOreq.au16PWM_Duty[1],
						sIOreq.au16PWM_Duty[2], sIOreq.au16PWM_Duty[3]);

				i16TransmitIoSettingRequest(u8addr, &sIOreq);
			}

			return;
		}else if(u8cmd == SERCMD_ID_PAL_COMMAND){
			uint8 u8Num = G_OCTET();
			tsReplyData sReplyData;
			memset(&sReplyData, 0x00, sizeof(tsReplyData));

			uint8 i;
			for( i=0; i<u8Num; i++ ){
				uint8 u8com = G_OCTET();
				switch(u8com){
					case 0:
						sReplyData.u8Identifier = G_OCTET();
						p++;
						sReplyData.u8Event = G_OCTET();
						sReplyData.bCommand = TRUE;
						break;
					case 1:
						_C{
							uint8 u8Color = G_OCTET();
							uint8 u8Blink = G_OCTET();
							uint8 u8Bright = G_OCTET();

							sReplyData.u8Event = 0xFE;
							if( u8Bright > 15 ) u8Bright = 15;
							if( u8Blink > 15 ) u8Bright = 0;
							sReplyData.u16RGBW = (au8Color[u8Color][0]*u8Bright);
							sReplyData.u16RGBW |= (au8Color[u8Color][1]*u8Bright)<<4;
							sReplyData.u16RGBW |= (au8Color[u8Color][2]*u8Bright)<<8;
							sReplyData.u16RGBW |= (au8Color[u8Color][3]*u8Bright)<<12;
							sReplyData.u8BlinkCycle = au8BlinkCycle[u8Blink];
							sReplyData.u8BlinkDuty = au8BlinkDuty[u8Blink];
							sReplyData.bCommand = TRUE;
							sReplyData.u8Identifier = PKT_ID_LED;
						}
						break;
					case 2:
						sReplyData.bCommand = TRUE;
						p++;
						//sReplyData.u8Identifier = G_OCTET();
						//sReplyData.u8LightsOutCycle = G_BE_WORD()&0xFF;
						sReplyData.u32LightsOutTime_sec = G_BE_WORD();
						break;
					case 3:
						sReplyData.u8Event = 0xFE;
						sReplyData.bCommand = TRUE;
						sReplyData.u8Identifier = PKT_ID_LED;
						p++;
						sReplyData.u16RGBW = G_BE_WORD();
						break;
					case 4:
						sReplyData.u8Event = 0xFE;
						sReplyData.bCommand = TRUE;
						sReplyData.u8Identifier = PKT_ID_LED;
						p++;
						sReplyData.u8BlinkDuty = G_OCTET();
						sReplyData.u8BlinkCycle = G_OCTET();
						break; 
					default:
						break;
				} 
			}
			if(Reply_bSetData( u8addr, &sReplyData )){
				uint8 id[] = {u8addr, 0};
				Reply_bSendShareData( id, 1, TOCONET_MAC_ADDR_BROADCAST );
			}
			return;
		}

		/*
		 * 書式なしデータ送信
		 */
		if (sAppData.u8AppLogicalId != u8addr) {
			// 自分宛でないなら送る
			bTransmitRfPacket = TRUE;
		}
	}

	// 無線パケットを送信する
	if (bTransmitRfPacket) {
		bool_t bDoNotTransmit = FALSE;

		p = pSer->au8data; // バッファの先頭からそのまま送る
		uint16 u16len = p_end - p;

		DBGOUT(3, "* len = %d" LB, u16len);

		if (u16len > SERCMD_SER_PKTLEN * SERCMD_SER_PKTNUM || u16len <= 1) {
			// パケットが長過ぎる、または短すぎる
			bDoNotTransmit = TRUE;
		}

		if (!bDoNotTransmit) {
			// 送信リクエスト
			i16TransmitSerMsg(p, u16len, ToCoNet_u32GetSerial(),
					sAppData.u8AppLogicalId, p[0], FALSE,
					sAppData.u8UartReqNum++);
		}
	}
}

/** @ingroup MASTER
 * IO(DO/PWM)を設定する要求コマンドパケットを送信します。
 *
 * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 中継フラグ
 *   - OCTET: 形式 (1固定)
 *   - OCTET: ボタン (LSB から順に SW1 ... SW4, 1=Lo)
 *   - OCTET: ボタン使用フラグ (LSB から順に SW1 ... SW4, 1=このポートを設定する)
 *   - BE_WORD: PWM1 (0..1024 or 0xffff) 0xffff を設定すると、このポートの設定をしない。
 *   - BE_WORD: PWM2 (0..1024 or 0xffff)
 *   - BE_WORD: PWM3 (0..1024 or 0xffff)
 *   - BE_WORD: PWM4 (0..1024 or 0xffff)
 *
 * @param u8DstAddr 送信先
 * @param pReq 設定データ
 * @return -1:Error, 0..255:CbId
 */
int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq) {
	int16 i16Ret, i;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_TWELITE_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(u8DstAddr); // 宛先
	S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ
	S_OCTET(0); // 中継フラグ

	S_OCTET(1); // パケット形式

	// DIO の設定
	S_OCTET(pReq->u8IOports);
	S_OCTET(pReq->u8IOports_use_mask);

	// PWM の設定
	for (i = 0; i < 4; i++) {
		S_BE_WORD(pReq->au16PWM_Duty[i]);
	}

	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT; // パケット種別

	// 送信する
	sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry = sAppData.u8StandardTxRetry; // 再送

	sTx.bSecurePacket = IS_APPCONF_OPT_SECURE() ? TRUE:FALSE;

	{
		/* 送信設定 */
		sTx.bAckReq = FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 4; // 再送間隔
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			i16Ret = sTx.u8CbId;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * シリアルメッセージの送信要求を行います。１パケットを分割して送信します。
 *
 * - Packet 構造
 *   - OCTET    : 識別ヘッダ(APP ID より生成)
 *   - OCTET    : プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET    : 送信元個体識別論理ID
 *   - BE_DWORD : 送信元シリアル番号
 *   - OCTET    : 送信先シリアル番号
 *   - OCTET    : 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET    : 送信フラグ(リピータ用)
 *   - OCTET    : 要求番号
 *   - OCTET    : パケット数(total)
 *   - OCTET    : パケット番号 (0...total-1)
 *   - BE_WORD  : 本パケットのバッファ位置
 *   - OCTET    : 本パケットのデータ長
 *
 * @param p ペイロードのデータへのポインタ
 * @param u16len ペイロード長
 * @param bRelay 中継フラグ TRUE:中継する
 * @return -1:失敗, 0:成功
 */
static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, uint32 u32AddrSrc, uint8 u8AddrSrc, uint8 u8AddrDst, uint8 u8RelayLv, uint8 u8Req) {
	// パケットを分割して送信する。
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));
	uint8 *q; // for S_??? macros

	// 処理中のチェック（処理中なら送信せず失敗）
	if (sSerSeqTx.bWaitComplete) {
		return -1;
	}

	// sSerSeqTx は分割パケットの管理構造体
	sSerSeqTx.u8IdSender = sAppData.u8AppLogicalId;
	sSerSeqTx.u8IdReceiver = u8AddrDst;

	sSerSeqTx.u8PktNum = (u16len - 1) / SERCMD_SER_PKTLEN + 1;
	sSerSeqTx.u16DataLen = u16len;
	sSerSeqTx.u8Seq = sSerSeqTx.u8SeqNext; // パケットのシーケンス番号（アプリでは使用しない）
	sSerSeqTx.u8SeqNext = sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum; // 次のシーケンス番号（予め計算しておく）
	sSerSeqTx.u8ReqNum = u8Req; // パケットの要求番号（この番号で送信系列を弁別する）
	sSerSeqTx.bWaitComplete = TRUE;
	sSerSeqTx.u32Tick = u32TickCount_ms;
	memset(sSerSeqTx.bPktStatus, 0, sizeof(sSerSeqTx.bPktStatus));

	DBGOUT(3, "* >>> Transmit(req=%d) Tick=%d <<<" LB, sSerSeqTx.u8ReqNum,
			u32TickCount_ms & 65535);

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA; // data packet.
	sTx.u8Retry = sAppData.u8StandardTxRetry;
	sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // application retry
	sTx.bSecurePacket = IS_APPCONF_OPT_SECURE() ? TRUE:FALSE;

	int i;
	for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
		q = sTx.auData;
		sTx.u8Seq = sSerSeqTx.u8Seq + i;
		sTx.u8CbId = sTx.u8Seq; // callback will reported with this ID

		// ペイロードを構成
		S_OCTET(sAppData.u8AppIdentifier);
		S_OCTET(APP_TWELITE_PROTOCOL_VERSION);
		S_OCTET(u8AddrSrc); // アプリケーション論理アドレス
		S_BE_DWORD(u32AddrSrc);  // シリアル番号
		S_OCTET(sSerSeqTx.u8IdReceiver); // 宛先
		S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ

		S_OCTET(u8RelayLv); // 中継レベル

		S_OCTET(sSerSeqTx.u8ReqNum); // request number
		S_OCTET(sSerSeqTx.u8PktNum); // total packets
		S_OCTET(i); // packet number
		S_BE_WORD(i * SERCMD_SER_PKTLEN); // offset

		uint8 u8len_data =
				(u16len >= SERCMD_SER_PKTLEN) ? SERCMD_SER_PKTLEN : u16len;
		S_OCTET(u8len_data);

		memcpy(q, p, u8len_data);
		q += u8len_data;

		sTx.u8Len = q - sTx.auData;

		// あて先など
		sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト

		if (sAppData.eNwkMode == E_NWKMODE_MAC_DIRECT) {
			sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
			sTx.bAckReq = FALSE;
			sTx.u8Retry = sAppData.u8StandardTxRetry;

			ToCoNet_bMacTxReq(&sTx);
		}

		p += u8len_data;
		u16len -= SERCMD_SER_PKTLEN;
	}

	return 0;
}

void vSetReplyData( tsSnsData* data )
{
	uint8 u8LID = data->u8LID&0x7F;
	if( u8LID < 1 || 100 < u8LID ){
		return;
	}

	tsReplyData sReplyData;
	sReplyData.bCommand = FALSE;
	sReplyData.u8Event = data->u8Event;
	sReplyData.u8Identifier = 0xFF;
//	sReplyData.u8Identifier = PKT_ID_LED;
/*
	switch (data->u8SNSID){
		case PKT_ID_MAG:
			_C{
				sReplyData.bCommand = FALSE;
				if( (data->u8HallIC&0x7F) == 0 ){
					sReplyData.u8Event = 0;
				}else{
					sReplyData.u8Event = 1;
				}
				sReplyData.u8Identifier = PKT_ID_LED;
			}
			break;
		case PKT_ID_AMB:
			return;
		case PKT_ID_MOT:
			return;
		case PKT_ID_LED:
			sReplyData.u8Event = data->u8AccelEvent;
			sReplyData.u8Identifier = PKT_ID_LED;
			break;

		default:
			break;
	}
*/
	uint8 lid[] = {((data->u8LID&0x80) ? (data->u8LID&0x7F): (data->u8LID|0x80)), 0} ;
	if(Reply_bSetData( lid[0], &sReplyData )){
		Reply_bSendShareData( lid, 1, TOCONET_MAC_ADDR_BROADCAST );
	}
}