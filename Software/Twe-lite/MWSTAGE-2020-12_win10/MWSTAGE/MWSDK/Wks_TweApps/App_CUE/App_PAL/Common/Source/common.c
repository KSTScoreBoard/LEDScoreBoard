/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"
#include "utils.h"

#include "config.h"
#include "common.h"

#include "utils.h"
//#include "serialInputMgr.h"

// Serial options
#include "serial.h"
#include "fprintf.h"

#include "Interactive.h"

// ToCoNet Header
#include "ToCoNet.h"

#include "ccitt8.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "SMBus.h"

#define CONTINUE() {u8StartAddr += 64; bOk = FALSE; memset(&sPALData, 0x00, sizeof(sPALData)); continue;}

/**
 * Sleep の DIO wakeup 用
 */
uint32 u32DioPortWakeUp = PORT_INPUT_MASK;

/**
 * 暗号化カギ
 */
const uint8 au8EncKey[] = {
		0x00, 0x01, 0x02, 0x03,
		0xF0, 0xE1, 0xD2, 0xC3,
		0x10, 0x20, 0x30, 0x40,
		0x1F, 0x2E, 0x3D, 0x4C
};

/**
 * 暗号化カギの登録
 * @param u32seed
 * @return
 */
 bool_t bRegAesKey(uint32 u32seed) {
	uint8 au8key[16], *p = (uint8 *)&u32seed;
	int i;

	for (i = 0; i < 16; i++) {
		// 元のキーと u32seed の XOR をとる
		au8key[i] = au8EncKey[i] ^ p[i & 0x3];
	}

	return ToCoNet_bRegisterAesKey((void*)au8key, NULL);
}

/**
 * ToCoNet のネットワーク情報を UART に出力する
 *
 * @param psSerStream 出力先
 * @param pc ネットワークコンテキスト
 *
 */
void vDispInfo(tsFILE *psSerStream, tsToCoNet_NwkLyTr_Context *pc) {
	if (pc) {
		vfPrintf(psSerStream, LB "* Info: la=%d ty=%d ro=%02x st=%02x",
				pc->sInfo.u8Layer, pc->sInfo.u8NwkTypeId, pc->sInfo.u8Role, pc->sInfo.u8State);
		vfPrintf(psSerStream, LB "* Parent: %08x", pc->u32AddrHigherLayer);
		vfPrintf(psSerStream, LB "* LostParent: %d", pc->u8Ct_LostParent);
		vfPrintf(psSerStream, LB "* SecRescan: %d, SecRelocate: %d", pc->u8Ct_Second_To_Rescan, pc->u8Ct_Second_To_Relocate);
	}
}

uint64 u64GetTimer_ms()
{
	if(u8AHI_WakeTimerStatus()&E_AHI_WAKE_TIMER_MASK_1){
		uint64 u64WakeCount = 0x1FFFFFFFFFFULL - u64AHI_WakeTimerReadLarge(E_AHI_WAKE_TIMER_1);
		uint64 u64Time_ms =  (u64WakeCount >> 5) * (uint64)sAppData.sFlash.sData.u16RcClock / 10000ULL;
		return u64Time_ms;
	}
	return 0xFFFFFFFFFFFFFFFFULL;
}

/**
 * PALのモデルIDやオプションをPALから取得する
 */
bool_t bGetPALOptions( void )
{
	const uint8 u8Addr = 0x56;
	uint8 u8StartAddr = 0x00;
	uint8 u8Status = 0;
	uint8 u8Data[64];
	uint8* p;
	bool_t bOk = TRUE;
	uint8 i = 0;

	for( i=0; i<2; i++ ){
		bOk = bSMBusWrite( u8Addr, u8StartAddr, 0, NULL );
		bOk &= bSMBusSequentialRead( u8Addr, 64, u8Data );

		if(bOk==FALSE){
			V_PRINTF(LB"Cannot communicate EEPROM...");
			sPALData.u8EEPROMStatus |= 0x40;		// 接続エラー
			return FALSE;
		}

		// マジックナンバーを読み込む
		p = u8Data;
		uint32 u32MagicNumber = G_BE_DWORD();
		if( u32MagicNumber != 0xA55A00FF ){
			if(i==1){
				u8Status |= 0x08;	// 2ブロック目のMagicNumberが間違っている
			}else{
				u8Status |= 0x01;	// 1ブロック目のMagicNumberが間違っている
			}
			V_PRINTF(LB"Magic number error...");
			CONTINUE();
		}
		V_PRINTF(LB"Magic number : %08X", u32MagicNumber);

		sPALData.u8OptionLength = G_OCTET();
		sPALData.u8FormatVersion = G_OCTET();
		sPALData.u8OptionLength--;
		V_PRINTF(LB"OptionLength : %d", sPALData.u8OptionLength );
		V_PRINTF(LB"Format Version : %d", sPALData.u8FormatVersion );

		if( sPALData.u8FormatVersion == 0x01 ){
			sPALData.u8PALModel = G_OCTET();
			sPALData.u8PALVersion = G_OCTET();
			V_PRINTF( LB"PAL Ver %d.%d", sPALData.u8PALModel, sPALData.u8PALVersion );
			(void)G_BE_WORD();				// 予約領域
			sPALData.u8OptionLength -= 4;
			for(i=0;i<sPALData.u8OptionLength;i++){
				sPALData.au8Option[i] = G_OCTET();
			}
			uint8 u8EEPcrc = G_OCTET();
			uint8 u8crc = u8CCITT8(u8Data+5, sPALData.u8OptionLength+5);
			if( u8crc == u8EEPcrc ){
				break;
			}else{
				if(i==1){
					u8Status |= 0x20;	// 2ブロック目がCRCエラー
				}else{
					u8Status |= 0x04;	// 1ブロック目がCRCエラー
				}
				V_PRINTF(LB"CRC error... %02X  %02X", u8EEPcrc, u8crc );
				CONTINUE();
			}
		}else{
			if(i==1){
				u8Status |= 0x10;	// 2ブロック目のフォーマットのバージョンが定義外
			}else{
				u8Status |= 0x02;	// 1ブロック目のフォーマットのバージョンが定義外
			}
			V_PRINTF(LB"Data version error...");
			CONTINUE();
		}
	}

	if(bOk){
		// シリアルIDを読み込む
		i = sPALData.u8OptionLength/4;
		p = sPALData.au8Option;
		uint32 u32Data;
		while(i>0){
			u32Data = G_BE_DWORD();
			// シリアル番号がある場合、シリアルを表示する
			if((u32Data&0xFF000000)>>24 == 0xF0 ){
				uint8 u8ModelVer = (sPALData.u8PALVersion<<7)|((sPALData.u8PALVersion&0x02)<<5)|((sPALData.u8PALVersion&0x04)<<3)|(sPALData.u8PALModel&0x1F);	// PALのIDとバージョン
				sPALData.u32SerialID = (u32Data&0x00FFFFFF)|(u8ModelVer<<24);
				break;
			}
			i--;
		}

		V_PRINTF(LB"SID : %08X", sPALData.u32SerialID );
		V_PRINTF(LB"Success!!!" );
	}
	sPALData.u8EEPROMStatus = u8Status;	// 2ブロック目のMagicNumberが間違っている
	return bOk;
}

/**
 *
 * 親機に対して送信する
 *
 * @param pNwk ネットワークコンテキスト
 * @param pu8Data ペイロード
 * @param u8Len ペイロード長
 *
 */
bool_t bTransmitToParent(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len)
{
	// LEDを点灯させる
	if(sAppData.u8LedState == 0x00){
		LED_ON();
	}

	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	// 暗号化鍵の登録
	if (IS_APPCONF_OPT_SECURE()) {
		bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
		V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
		sTx.bSecurePacket = TRUE;
	}

	if (IS_APPCONF_OPT_TO_ROUTER()) {
		// ルータがアプリ中で一度受信して、ルータから親機に再配送
		sTx.u32DstAddr = TOCONET_NWK_ADDR_NEIGHBOUR_ABOVE;
	} else {
		// ルータがアプリ中では受信せず、単純に中継する
		sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
	}

	// ペイロードの準備
	S_OCTET('T'+0x80);
	S_OCTET(sAppData.u8LID);
	S_BE_WORD(sAppData.u16frame_count);

	// 上下をひっくり返す
	uint8 u8nEventFlag = 0x80;
	uint8 u8PID = sPALData.u8PALModel&0x7F;
	if( sPALData.u8PALModel == PKT_ID_CUE  ){
		if( IS_APPCONF_OPT_FIFOMODE() ){
			u8PID = PKT_ID_MOT;
		}else
		if( IS_APPCONF_OPT_EVENTMODE() || IS_APPCONF_OPT_DICEMODE() ){
			u8nEventFlag = 0x00;
			u8PID = PKT_ID_MOT;
		}else
		if( IS_APPCONF_OPT_MAGMODE() ){
			u8PID = PKT_ID_MAG;
			if( IS_APPCONF_OPT_EVENTMODE_MAG() ){
				u8nEventFlag = 0x00;
			}		
		}else
		{
			u8nEventFlag = 0;
		}
	}else
	if( sPALData.u8PALModel == PKT_ID_MOT ){
		if( IS_APPCONF_OPT_EVENTMODE() || IS_APPCONF_OPT_DICEMODE() ){
			u8nEventFlag = 0x00;
		}
	}
	if( sPALData.u8PALModel == PKT_ID_MAG ){
		if( IS_APPCONF_OPT_EVENTMODE_MAG() ){
			u8nEventFlag = 0x00;
		}
	}

	S_OCTET( (u8nEventFlag|u8PID) );	// PALのIDとバージョン

	//	センサ固有のデータ
	memcpy(q,pu8Data,u8Len);
	q += u8Len;

	sTx.u8Cmd = 0; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8Retry;

	return ToCoNet_Nwk_bTx(pNwk, &sTx);
}

/**
 *
 * App_Tagとして送信する
 *
 * @param pNwk ネットワークコンテキスト
 * @param pu8Data ペイロード
 * @param u8Len ペイロード長
 *
 */
bool_t bTransmitToAppTag(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len)
{
	// LEDを点灯させる
	if(sAppData.u8LedState == 0x00){
		LED_ON();
	}

	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	// 暗号化鍵の登録
	if (IS_APPCONF_OPT_SECURE()) {
		bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
		V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
		sTx.bSecurePacket = TRUE;
	}

	if (IS_APPCONF_OPT_TO_ROUTER()) {
		// ルータがアプリ中で一度受信して、ルータから親機に再配送
		sTx.u32DstAddr = TOCONET_NWK_ADDR_NEIGHBOUR_ABOVE;
	} else {
		// ルータがアプリ中では受信せず、単純に中継する
		sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
	}

	// ペイロードの準備
	S_OCTET('T');
	S_OCTET(sAppData.u8LID);
	S_BE_WORD(sAppData.u16frame_count);

	if( sPALData.u8PALModel == PKT_ID_MOT || sPALData.u8PALModel == PKT_ID_CUE ){
		S_OCTET(0x35);	
	}else{
		// 上下をひっくり返す
		uint8 u8Version = (sPALData.u8PALVersion<<7)|((sPALData.u8PALVersion&0x02)<<5)|((sPALData.u8PALVersion&0x04)<<3);
		S_OCTET(u8Version|(sPALData.u8PALModel&0x1F));	// PALのIDとバージョン
	}

	//	センサ固有のデータ
	memcpy(q,pu8Data,u8Len);
	q += u8Len;

	sTx.u8Cmd = 0; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8Retry;

	return ToCoNet_Nwk_bTx(pNwk, &sTx);
}

bool_t bTransmitToAppTwelite( uint8 *pu8Data, uint8 u8Len )
{
	//	DO+PWMの設定値が7バイトでない場合送信失敗にする。
	if( u8Len != 7 ){
		return FALSE;
	}

	if(sAppData.u8LedState == 0x00){
		LED_ON();
	}

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	uint8* q = sTx.auData;
	uint8 crc = u8CCITT8((uint8*) &sToCoNet_AppContext.u32AppId, 4);

	S_OCTET(crc);									//
	S_OCTET(0x01);									// プロトコルバージョン
	if(sAppData.sFlash.sData.u8id == 0){
		S_OCTET(0x78);								// アプリケーション論理アドレス
	}else{
		S_OCTET(sAppData.sFlash.sData.u8id);		// アプリケーション論理アドレス
	}
	S_BE_DWORD(ToCoNet_u32GetSerial());				// シリアル番号
	S_OCTET(0x00);									// 宛先
	S_BE_WORD( (u32TickCount_ms & 0x7FFF)+0x8000);	// タイムスタンプ
	S_OCTET(0);										// 中継フラグ
	S_BE_WORD(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);	//	電源電圧
	S_OCTET(0);										// 温度(ダミー)

	//	DI+AI
	memcpy(q,pu8Data,u8Len);
	q += u8Len;

	sTx.u8Cmd = 0x02+0; // パケット種別
	// 送信する
	sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
	sTx.bAckReq = FALSE;
	sTx.u8Retry = sAppData.u8Retry;

	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)

	return ToCoNet_bMacTxReq(&sTx);
}

/** @ingroup MASTER
 * スリープの実行
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep ) {
	vPortSetLo(WDT_OUT);
	if(!IS_APPCONF_OPT_WAKE_RANDOM() && u32SleepDur_ms != 0){		//	起床ランダムのオプションが立っていた時
		uint32 u32max = u32SleepDur_ms>>3;		//	だいたい±10%
		uint32 u32Rand = ToCoNet_u32GetRand();
		uint32 u32Rand_max = u32Rand%(u32max+1);
		if( (u32Rand&0x8000) != 0 ){
			if( u32Rand_max+10 < u32SleepDur_ms && u32SleepDur_ms > 10 ){	//	スリープ時間が10ms以下にならないようにする
				u32SleepDur_ms -= u32Rand_max;
			}
		}else{
			u32SleepDur_ms += u32Rand_max;
		}
	}

	uint32 u32Count = sAppData.u32Sleep_min + ( sAppData.u8Sleep_sec ? 1 : 0 );

	// タイマー起床の時だけカウントする
	if( sAppData.bWakeupByButton == FALSE && u32SleepDur_ms!=0 ){
		sAppData.u32SleepCount++;
	}
	if( sAppData.u32SleepCount >= u32Count ){
		sAppData.u32SleepCount = 0;
	}

	// wake up using wakeup timer as well.
	ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32SleepDur_ms, bPeriodic, bDeep); // PERIODIC RAM OFF SLEEP USING WK0
}

/** @ingroup MASTER
 * 文字列表示後リセットする
 * @param str
 */
void vResetWithMsg(tsFILE *psSerStream, string str) {
	if (str != NULL) {
		vfPrintf(psSerStream, str);
		SERIAL_vFlush(psSerStream->u8Device);
	}
	vAHI_SwReset();
}
