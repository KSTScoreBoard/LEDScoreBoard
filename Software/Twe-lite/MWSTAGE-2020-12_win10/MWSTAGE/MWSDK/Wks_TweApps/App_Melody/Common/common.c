/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "ToCoNet.h"

#include "config.h"
#include "utils.h"
#include "modbus_ascii.h"

#include "common.h"

/**
 * DI のポート番号のテーブル
 */
const uint8 au8PortTbl_DIn[4] = {
	PORT_INPUT1,
	PORT_INPUT2,
	PORT_INPUT3,
	PORT_INPUT4
};

/**
 * DO のポート番号のテーブル
 */
const uint8 au8PortTbl_DOut[4] = {
	PORT_OUT1,
	PORT_OUT2,
	PORT_OUT3,
	PORT_OUT4
};

/**
 * タイマーデバイス番号のテーブル
 */
const uint8 au8PortTbl_PWM_Timer[4] = {
	E_AHI_DEVICE_TIMER1,
	E_AHI_DEVICE_TIMER2,
	E_AHI_DEVICE_TIMER3,
	E_AHI_DEVICE_TIMER4
};

/**
 * MODE設定ビットからデフォルト割り当てされる論理ＩＤテーブル
 */
const uint8 au8IoModeTbl_To_LogicalID[8] = {
	120, // CHILD
	0,   // PARENT
	254, // ROUTER
	123, // 32fps mode (7B)
	124, // 1sec sleep (7C)
	255, // NODEF
	255, // NODEF
	127  // 10sec sleep (7F)
};

/** @ingroup MBUSA
 * MODBUS ASCII シリアル出力用のバッファ
 */
extern uint8 au8SerOutBuff[];

/** @ingroup MBUSA
 * 自身のシリアル番号を出力する（起動時メッセージにも利用）
 * @param pSer 出力先ストリーム
 */
void vModbOut_MySerial(tsFILE *pSer) {
	uint8 *q = au8SerOutBuff;

	S_OCTET(VERSION_MAIN);
	S_OCTET(VERSION_SUB);
	S_OCTET(VERSION_VAR);

	S_BE_DWORD(ToCoNet_u32GetSerial());

	vSerOutput_ModbusAscii(pSer,
			SERCMD_ADDR_FR_MODULE,
			SERCMD_ID_INFORM_MODULE_ADDRESS,
			au8SerOutBuff,
			q - au8SerOutBuff);
}

#if 0
/** @ingroup MBUSA
 * ACK/NACK を出力する
 * @param pSer 出力先ストリーム
 * @param bAck TRUE:ACK, FALSE:NACK
 */
void vModbOut_AckNack(tsFILE *pSer, bool_t bAck) {
	uint8 *q = au8SerOutBuff;

	S_OCTET(bAck ? 1 : 0);

	vSerOutput_ModbusAscii(pSer,
			SERCMD_ADDR_FR_MODULE,
			bAck ? SERCMD_ID_ACK : SERCMD_ID_NACK,
			au8SerOutBuff,
			q - au8SerOutBuff);
}


/** @ingroup MBUSA
 * フラッシュ設定データ列を解釈する。入力は modbus のアドレス・コマンドを含むデータ列。
 * ※ 本実装は実験的で、フラッシュのデータ全てに対応していません。
 *
 * @param p 入力バイト列
 * @param pConfig データの書き出し先
 * @return TRUE: データが正しい
 */
bool_t bModbIn_Config(uint8 *p,  tsFlashApp *pConfig) {
	uint8 u8adr;
	uint8 u8cmd;
	OCTET(u8adr);
	OCTET(u8cmd);
	BE_DWORD(pConfig->u32appid);
	OCTET(pConfig->u8role);
	OCTET(pConfig->u8layer);
	OCTET(pConfig->u8ch);
	BE_DWORD(pConfig->u32chmask);

	// 必要ならデータの正当性チェックを行う！

	return TRUE;
}

/** @ingroup MBUSA
 * フラッシュ設定を出力する。
 * ※ 本実装は実験的で、フラッシュのデータ全てに対応していません。
 *
 * @param pSer 出力先ストリーム
 * @param pConfig 設定構造体
 */
void vModbOut_Config(tsFILE *pSer, tsFlashApp *pConfig) {
	uint8 *q = au8SerOutBuff;

	S_BE_DWORD(pConfig->u32appid);
	S_OCTET(pConfig->u8role);
	S_OCTET(pConfig->u8layer);
	S_OCTET(pConfig->u8ch);
	S_BE_DWORD(pConfig->u32chmask);

	vSerOutput_ModbusAscii(pSer,
			SERCMD_ADDR_FR_MODULE,
			SERCMD_ID_INFORM_NETWORK_CONFIG,
			au8SerOutBuff,
			q - au8SerOutBuff);
}
#endif
