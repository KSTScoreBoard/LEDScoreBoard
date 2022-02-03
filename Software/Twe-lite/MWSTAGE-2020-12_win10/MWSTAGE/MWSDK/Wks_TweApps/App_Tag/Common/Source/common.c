/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

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

#ifdef ENDDEVICE_INPUT
#include "ccitt8.h"
#include "EndDevice_Input.h"
#endif

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

#ifdef ENDDEVICE_INPUT
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
	if( !(sAppData.sFlash.sData.u8mode == 0x35 && (sAppData.sFlash.sData.i16param&128)) ){
		LED_ON(LED);
	}
	// センサー用の電源制御回路を Hi に戻す
	vPortSetSns(FALSE);

	// 暗号化鍵の登録
	if (IS_APPCONF_OPT_SECURE()) {
		bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
		V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
	}

	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！
	uint8 *q =  sTx.auData;

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	if (IS_APPCONF_OPT_SECURE()) {
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
	S_OCTET(sAppData.sFlash.sData.u8id);
	S_BE_WORD(sAppData.u16frame_count);

	if( sAppData.sFlash.sData.u8mode == 0xA1 ){
		S_OCTET(0x35);	// ADXL345 LowEnergy Mode の時、普通のADXL345として送る
	}else{
		S_OCTET(sAppData.sFlash.sData.u8mode); // パケット識別子
	}

	//	センサ固有のデータ
	memcpy(q,pu8Data,u8Len);
	q += u8Len;

	sTx.u8Cmd = 0; // 0..7 の値を取る。パケットの種別を分けたい時に使用する
	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)
	sTx.u8Retry = sAppData.u8Retry;
#ifdef SWING
	sTx.u16RetryDur = 0;
#endif

	return ToCoNet_Nwk_bTx(pNwk, &sTx);
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
bool_t bTransmitToAppTwelite( uint8 *pu8Data, uint8 u8Len )
{
	//	DO+PWMの設定値が7バイトでない場合送信失敗にする。
	if( u8Len != 7 ){
		return FALSE;
	}

	if( !(sAppData.sFlash.sData.u8mode == 0x35 && (sAppData.sFlash.sData.i16param&128)) ){
		LED_ON(LED);
	}
	// センサー用の電源制御回路を Hi に戻す
	vPortSetSns(FALSE);

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

#ifdef SWING
	sTx.u16RetryDur = 0;
#endif

	return ToCoNet_bMacTxReq(&sTx);
}
#endif

/** @ingroup MASTER
 * スリープの実行
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep) {
#ifdef ENDDEVICE_INPUT
	vPortSetSns(FALSE);
	if(IS_APPCONF_OPT_WAKE_RANDOM()){		//	起床ランダムのオプションが立っていた時
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
#endif
	// print message.
	vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

	// stop interrupt source, if interrupt source is still running.
	;

	// set UART Rx port as interrupt source
	vAHI_DioSetDirection(PORT_INPUT_MASK, 0); // set as input

	(void)u32AHI_DioInterruptStatus(); // clear interrupt register
	vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
	//vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE
	 vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
	//vAHI_DioWakeEdge(PORT_INPUT_MASK, 0); // 割り込みエッジ（立上がりに設定）

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
