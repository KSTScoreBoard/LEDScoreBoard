/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include "ToCoNet.h"
#include "utils.h"

#include "App_Wings.h"
#include "App_PAL.h"

#define REPLY_PKTVER 0x01

#define REPLY_ASK 0x00
#define REPLY_SHARE 0x01

static uint8* pu8CreateData( uint8 u8slot, uint8 u8Id, uint8* q );

static tsReplyData sReplyData[2][100];
static uint32 u32Seq = 0;
static uint32 u32Tick_ms = 0;

extern TWE_tsFILE sSer;
extern tsToCoNet_DupChk_Context* psDupChk;

// 構造体を初期化する
void Reply_vInit()
{
	uint8 i, j;
	for( i=0; i<2; i++){
		for( j=0; j<8; j++){
			memset( &sReplyData[i][j], 0xFF, sizeof(tsReplyData) );
		}
	}
}

// 子機に送るデータをセットし、下位レイヤに共有する。
bool_t Reply_bSetData( uint8 u8Id, tsReplyData* sData )
{
	uint8 u8slot = u8Id>>7;
	uint8 u8lid = (u8Id&0x7F)-1;

	if( u8lid > 99 ) return FALSE;
	if( 1 < u8slot ) return FALSE;

	if( sData->u8Identifier != 0 ){
		if( u8Id == 0xFF ){
			uint8 i, j;
			DBGOUT( 3, LB"%08X Set Data.", u32TickCount_ms );
			for(i=0; i<2; i++){
				for(j=0; j<100; j++){
					sReplyData[i][j] = *sData;
					sReplyData[i][j].u16Count = (uint16)u32Seq;
				}

				DBGOUT( 3, LB"SET ID:%d", (u8slot<<7)|(j+1) );
				DBGOUT( 3, ", EVENT:%d", sReplyData[i][0].u8Event  );
				DBGOUT( 3, ", PKT:%d", sReplyData[i][0].u8Identifier );
/*
				if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_LED ){
					DBGOUT( 3, ", RGB:%04X",sReplyData[i][j].u16RGBW  );
				}else if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_IRC ){
					DBGOUT( 3, ", IRC:%02X",sReplyData[i][j].u8IRCID  );
				}
*/
			}

			u32Seq++;
			return TRUE;

		}else{
			uint8 u8Ok = ((sData->bCommand == sReplyData[u8slot][u8lid].bCommand) ? 0: 1);
			u8Ok += ((sData->u16RGBW == sReplyData[u8slot][u8lid].u16RGBW) ? 0: 1);
			u8Ok += ((sData->u8BlinkCycle == sReplyData[u8slot][u8lid].u8BlinkCycle) ? 0: 1);
			u8Ok += ((sData->u8BlinkDuty == sReplyData[u8slot][u8lid].u8BlinkDuty) ? 0: 1);
			u8Ok += ((sData->u8Identifier == sReplyData[u8slot][u8lid].u8Identifier) ? 0: 1);
			u8Ok += ((sData->u8IRCID == sReplyData[u8slot][u8lid].u8IRCID) ? 0: 1);
			u8Ok += ((sData->u8LightsOutCycle == sReplyData[u8slot][u8lid].u8LightsOutCycle) ? 0: 1);
			u8Ok += ((sData->u8Event == sReplyData[u8slot][u8lid].u8Event) ? 0: 1);
			u8Ok += ((sData->u32LightsOutTime_sec == sReplyData[u8slot][u8lid].u32LightsOutTime_sec) ? 0: 1);
			//TWE_fprintf(&sSer, LB"bOk = %d", u8Ok );

			if(u8Ok > 0){
				sReplyData[u8slot][u8lid] = *sData;
				sReplyData[u8slot][u8lid].u16Count = (uint16)u32Seq;

				DBGOUT( 3, LB"%08X Set Data.", u32TickCount_ms );
				DBGOUT( 3, LB"ID:%d", u8Id );
				DBGOUT( 3, LB"EVENT:%d", sReplyData[u8slot][u8lid].u8Event  );
				DBGOUT( 3, LB"PKT:%d", sReplyData[u8slot][u8lid].u8Identifier );
				if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_LED ){
					DBGOUT( 3, LB"RGB:%04X",sReplyData[u8slot][u8lid].u16RGBW  );
				}else if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_IRC ){
					DBGOUT( 3, LB"IRC:%02X",sReplyData[u8slot][u8lid].u8IRCID  );
				}

				u32Seq++;
				return TRUE;
			}
		}
	}

	DBGOUT( 3, LB"%08X Not Set Data...", u32TickCount_ms );
	return FALSE;
}

// 子機に送るデータを受け取る
bool_t Reply_bGetData( uint8 u8Id, tsReplyData* sData )
{
	uint8 u8slot = u8Id>>7;
	uint8 u8lid = (u8Id&0x7F)-1;

	if( u8lid > 99 ) return FALSE;
	if( 1 < u8slot ) return FALSE;

	*sData = sReplyData[u8slot][u8lid];
	//DBGOUT( 3, LB"Get Data." );
	return TRUE;
}

// 子機にデータを送信する
bool_t Reply_bSendData( tsRxPktInfo sRxPktInfo )
{
//	uint8 u8id = (sRxPktInfo.u8id==0x78) ? 0 : sRxPktInfo.u8id;
//	if( u8id > 100 ){
//		return FALSE;
//	}

	uint8 u8slot = sRxPktInfo.u8id>>7;
	uint8 u8lid = (sRxPktInfo.u8id&0x7F)-1;

	if( u8lid > 99 ) return FALSE;
	if( 1 < u8slot ) return FALSE;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();
	sTx.u32DstAddr = sRxPktInfo.u32addr_1st;

	DBGOUT( 3, LB"ID:%d", sRxPktInfo.u8id );
	DBGOUT( 3, LB"EVENT:%d", sReplyData[u8slot][u8lid].u8Event  );
	DBGOUT( 3, LB"PKT:%d", sReplyData[u8slot][u8lid].u8Identifier );
	if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_LED ){
		DBGOUT( 3, LB"RGB:%04X",sReplyData[u8slot][u8lid].u16RGBW  );
	}else if( sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_IRC ){
		DBGOUT( 3, LB"IRC:%02X",sReplyData[u8slot][u8lid].u8IRCID  );
	}

	S_OCTET('P'+0x80);
	S_OCTET(121);
	S_BE_WORD(sReplyData[u8slot][u8lid].u16Count);
	S_OCTET(1);			// Version番号ということにする

	S_OCTET( sRxPktInfo.u8pkt&0x1F );
	S_OCTET( sReplyData[u8slot][u8lid].u8Event );
	if( sReplyData[u8slot][u8lid].u8Identifier == (sRxPktInfo.u8pkt&0x1F) ){
		if(sReplyData[u8slot][u8lid].u8Identifier == PKT_ID_LED){
			S_BE_WORD(sReplyData[u8slot][u8lid].u16RGBW);
			S_OCTET(sReplyData[u8slot][u8lid].u8BlinkCycle);
			S_OCTET(sReplyData[u8slot][u8lid].u8BlinkDuty);
			S_OCTET(sReplyData[u8slot][u8lid].u8LightsOutCycle);
		}else{
			S_OCTET(0xFE);
		}
	}else{
		S_OCTET(0xFF);
	}

	sTx.u16RetryDur = 0; // 再送間隔

	if(IS_ROUTER()){
		sTx.u16DelayMin = 4; // 衝突を抑制するため送信タイミングを遅らせる
		sTx.u16DelayMax = 12; // 衝突を抑制するため送信タイミングを遅らせる
	}else{
		sTx.u16DelayMin = 0; // 衝突を抑制するため送信タイミングを遅らせる
		sTx.u16DelayMax = 0; // 衝突を抑制するため送信タイミングを遅らせる
	}
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_PAL_REPLY; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = 0; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sReplyData[u8slot][u8lid].u16Count; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8StandardTxRetry;

	if(ToCoNet_bMacTxReq(&sTx)){
		if(sTx.u16DelayMax == 0){
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		}
		DBGOUT( 3, LB"Send data to EndDevice.");
		return TRUE;
	}
	return FALSE;
}

// 接続している上位レイヤに更新データを要求する。
bool_t Reply_bAskData(uint8 u8Id)
{
	uint8 u8slot = u8Id>>7;
	uint8 u8lid = (u8Id&0x7F)-1;

	if( (u8Id&0x80) != 0 ){
		if( u8lid > 99 ) return FALSE;
		if( 1 < u8slot ) return FALSE;
	}

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	uint8 num = ((u8Id&0x80) == 0x00) ? 8:1;
	uint8 i;
	uint16 u16Seq = 0;
	if(num == 8){
		for( i=0;i<num;i++ ){
			if( u16Seq < sReplyData[u8slot][i].u16Count ){
				u16Seq = sReplyData[u8slot][i].u16Count;
			}
		}
	}else{
		u16Seq = sReplyData[u8slot][u8lid].u16Count;
	}

	uint32 u32TickCount = (u32TickCount_ms >> 4)&0x00FFFFFF;

	S_OCTET('S'+0x80);
	S_OCTET(REPLY_PKTVER);
	S_OCTET(REPLY_ASK);
	S_BE_DWORD(u16Seq);
	S_BE_DWORD(u32TickCount);
	S_OCTET(u8Id);

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();
	sTx.u32DstAddr = sAppData.sNwkLayerTreeConfig.u32AddrHigherLayer;	// 必ず接続しているTWELITEに問い合わせる。

	sTx.u16RetryDur = 0; // 再送間隔
	sTx.u16DelayMin = 4; // 衝突を抑制するため送信タイミングを遅らせる
	sTx.u16DelayMax = 12; // 衝突を抑制するため送信タイミングを遅らせる

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_PAL_REPLY; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = 0; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = u16Seq; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8StandardTxRetry;

	if(ToCoNet_bMacTxReq(&sTx)){
		if(sTx.u16DelayMax == 0){
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		}
		DBGOUT( 3, LB"Ask data.");
		return TRUE;
	}

	return FALSE;
}

/**
 * 下位のレイヤに更新データを送信する。
 * 
 * @param pu8Id		送信するID
 * @param u8Datanum	送信するIDの数
 * @param 送信先
 **/ 
bool_t Reply_bSendShareData( uint8* pu8Id, uint8 u8Datanum, uint32 u32Addr )
{
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

//	DBGOUT( 3, LB"sh_Id = %d", u8lid );
//	DBGOUT( 3, ", slot = %d", u8slot );
//	DBGOUT( 3, ", num = %d", num );

	S_OCTET('S'+0x80);
	S_OCTET(REPLY_PKTVER);
	S_OCTET(REPLY_SHARE);
	S_BE_DWORD( u32Seq );

	uint32 u32TimeStamp = (u32TickCount_ms>>4)&0x00FFFFFF;
	if(IS_ROUTER()){
		u32TimeStamp = u32Tick_ms;
	}

	S_BE_DWORD( u32TimeStamp );

	S_OCTET(u8Datanum);

	uint8 i;
	for( i=0; i<u8Datanum; i++ ){
		uint8 id = (pu8Id[i]&0x7F)-1;
		uint8 slot = pu8Id[i]>>7;
		if(u8Datanum == 0){
			return FALSE;
		}
		q = pu8CreateData( slot, id, q );

		if( q == NULL ){
			return FALSE;
		}
	}

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();
	sTx.u32DstAddr = u32Addr;

	sTx.u16RetryDur = 0; // 再送間隔
	sTx.u16DelayMin = 4; // 衝突を抑制するため送信タイミングを遅らせる
	sTx.u16DelayMax = 80; // 衝突を抑制するため送信タイミングを遅らせる

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_PAL_REPLY; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = 0; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = u32Seq&0xFF; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8StandardTxRetry;

	if(ToCoNet_bMacTxReq(&sTx)){
		if(sTx.u16DelayMax == 0){
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
		}
		DBGOUT( 3, LB"Shere data.");
		return TRUE;
	}

	return FALSE;
}

bool_t Reply_bReceiveReplyData( tsRxDataApp* psRx )
{
	uint8* p = psRx->auData;

	uint8 u8Identifier = G_OCTET();
	if( u8Identifier != 'S'+0x80 ){
		return FALSE;
	}

	uint8 u8PktVer = G_OCTET();
	if ( u8PktVer != REPLY_PKTVER ){
		return FALSE;
	}

	uint8 u8PktId = G_OCTET();
	uint32 u32Seqnum = G_BE_DWORD();
	uint32 u32TimeStamp = G_BE_DWORD();(void)u32TimeStamp;


	if( u32Seqnum == u32Seq && u32TimeStamp == u32Tick_ms ){
		return FALSE;
	}

	if( u32Seq >= u32Seqnum && ToCoNet_DupChk_bAdd( psDupChk, psRx->u32SrcAddr, (uint8)u32TimeStamp ) ){
//		TWE_fprintf(&sSer, LB"bDup" );
		return FALSE;
	}

	if( IS_ROUTER() ){
		u32Tick_ms = u32TimeStamp;
		u32Seq = u32Seqnum;
//	}else if( IS_PARENT() ){
//		u32Seq++;
	}

	switch(u8PktId){
		case REPLY_ASK:
			_C{
				uint8 u8Id[] = {G_OCTET(), 0};
				Reply_bSendShareData( u8Id, 1, psRx->u32SrcAddr );
			}
			break;
		case REPLY_SHARE:
			if( sAppData.u8layer != 0 ){
				DBGOUT( 3, LB"Rcv Req. %02X", u8PktId);

				uint8 u8Num = G_OCTET();
				DBGOUT( 3, LB"Num = %d", u8Num );

				uint8 i;
//				uint8 u8slot = 0;
				uint8 au8id[8];
				uint8 count = 0;
				for( i=0; i<u8Num; i++ ){
					uint8 u8Id = G_OCTET();
//					u8slot = u8Id>>7;
					tsReplyData sData;
					memset(&sData, 0x00, sizeof(tsReplyData));
					sData.u8Identifier = G_OCTET();
					sData.u8Event = G_OCTET();
					DBGOUT( 3, LB"Id = %d", u8Id );
					DBGOUT( 3, ", Identifier = %d", sData.u8Identifier );
					DBGOUT( 3, ", Event = %d", sData.u8Event );
					switch(sData.u8Identifier){
						case PKT_ID_LED:
							sData.u16RGBW = G_BE_WORD();
							sData.u8BlinkCycle = G_OCTET();
							sData.u8BlinkDuty = G_OCTET();
							sData.u8LightsOutCycle = G_OCTET();
							DBGOUT( 3, ", RGBW = %04X", sData.u16RGBW );

							au8id[count] = u8Id+1;
							if( Reply_bSetData( au8id[count], &sData )) count++;
							break;
						case 0xFF:
							au8id[i] = u8Id+1;
							p += 5;
							if( Reply_bSetData( au8id[count], &sData )) count++;
						default:
							p += 5;
							break;
					}
				}

				if( count ){
					Reply_bSendShareData( au8id, count, TOCONET_MAC_ADDR_BROADCAST );
				}
			}
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static uint8* pu8CreateData( uint8 u8slot, uint8 u8Id, uint8* q )
{
	DBGOUT( 3, LB"CDId = %d", u8Id );
	DBGOUT( 3, ", Id = %d", u8slot ? (u8Id|0x80):u8Id );
	DBGOUT( 3, ", Identifier = %d", sReplyData[u8slot][u8Id].u8Identifier );
	DBGOUT( 3, ", Event = %d", sReplyData[u8slot][u8Id].u8Event );
	S_OCTET( u8slot ? (u8Id|0x80):u8Id );
	S_OCTET( sReplyData[u8slot][u8Id].u8Identifier );
	S_OCTET( sReplyData[u8slot][u8Id].u8Event );
	switch ( sReplyData[u8slot][u8Id].u8Identifier ){
	case PKT_ID_LED:
		DBGOUT( 3, ", Color = %04X", sReplyData[u8slot][u8Id].u16RGBW );
		S_BE_WORD( sReplyData[u8slot][u8Id].u16RGBW );
		S_OCTET( sReplyData[u8slot][u8Id].u8BlinkCycle );
		S_OCTET( sReplyData[u8slot][u8Id].u8BlinkDuty );
		S_OCTET( sReplyData[u8slot][u8Id].u8LightsOutCycle );
		break;
			
	default:
		S_BE_DWORD(0);
		S_OCTET(0);
		break;
	}
	return q;
}