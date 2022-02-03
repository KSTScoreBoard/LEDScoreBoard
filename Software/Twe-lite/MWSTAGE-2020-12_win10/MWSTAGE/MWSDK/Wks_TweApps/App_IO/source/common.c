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

/** @ingroup MBUSA
 * DI のポート番号のテーブル
 */
uint8 au8PortTbl_DIn[MAX_IO_TBL];
uint8 u8_PORT_INPUT_COUNT;  //!< @ingroup MASTER IO入力数
uint32 u32_PORT_INPUT_MASK; //!< 入力ポートのマスク @ingroup MASTER

/** @ingroup MBUSA
 * DO のポート番号のテーブル
 */
uint8 au8PortTbl_DOut[MAX_IO_TBL];
uint8 u8_PORT_OUTPUT_COUNT; //!< @ingroup MASTER IO出力数
uint32 u32_PORT_OUTPUT_MASK; //!< 出力ポートのマスク @ingroup MASTER

/** @ingroup MBUSA
 * 子機向け IO テーブル
 * （出力割り当てがある場合は後ろから順番に DI1,DI2
 */
const uint8 au8PortTbl[MAX_IOTBL_SETS][2][MAX_IO_TBL][2] = {
	{ // SET1 (12:0)
		{ // EndDevice Input --> Parent Output
			{PORT_INPUT1,	PORT_OUT1},
			{PORT_INPUT2,	PORT_OUT2},
			{PORT_INPUT3,	PORT_OUT3},
			{PORT_INPUT4,	PORT_OUT4},
			{PORT_OUT1,		PORT_INPUT1},
			{PORT_OUT2,		PORT_INPUT2},
			{PORT_OUT3,		PORT_INPUT3},
			{PORT_OUT4,		PORT_INPUT4},
			{PORT_I2C_CLK,	PORT_I2C_CLK},
			{PORT_I2C_DAT,	PORT_I2C_DAT},
			{PORT_EO1,		PORT_EO1},
			{PORT_EO2,		PORT_EO2},
			{0xFF, 0xFF} // TERMINATOR
		},
		{ //  Child Output <-- Parent Input
			{0xFF, 0xFF} // TERMINATOR
		},
	},
	{ // SET2 (8:4)
		{ // EndDevice Input --> Parent Output
			{PORT_INPUT1,	PORT_OUT1},
			{PORT_INPUT2,	PORT_OUT2},
			{PORT_INPUT3,	PORT_OUT3},
			{PORT_INPUT4,	PORT_OUT4},
			{PORT_I2C_CLK,	PORT_I2C_CLK},
			{PORT_I2C_DAT,	PORT_I2C_DAT},
			{PORT_EO1,		PORT_EO1},
			{PORT_EO2,		PORT_EO2},
			{0xFF, 0xFF} // TERMINATOR
		},
		{ //  Child Output <-- Parent Input
			{PORT_OUT1,		PORT_INPUT1},
			{PORT_OUT2,		PORT_INPUT2},
			{PORT_OUT3,		PORT_INPUT3},
			{PORT_OUT4,		PORT_INPUT4},
			{0xFF, 0xFF} // TERMINATOR
		},
	},
	{ // SET3 (6:6)
		{ // EndDevice Input --> Parent Output
			{PORT_INPUT1,	PORT_OUT1},
			{PORT_INPUT2,	PORT_OUT2},
			{PORT_INPUT3,	PORT_OUT3},
			{PORT_INPUT4,	PORT_OUT4},
			{PORT_EO1,		PORT_EO1},
			{PORT_EO2,		PORT_EO2},
			{0xFF, 0xFF} // TERMINATOR
		},
		{ //  Child Output <-- Parent Input
			{PORT_OUT1,		PORT_INPUT1},
			{PORT_OUT2,		PORT_INPUT2},
			{PORT_OUT3,		PORT_INPUT3},
			{PORT_OUT4,		PORT_INPUT4},
			{PORT_I2C_CLK,	PORT_I2C_CLK},
			{PORT_I2C_DAT,	PORT_I2C_DAT},
			{0xFF, 0xFF} // TERMINATOR
		},
	},
	{ // SET4 (0:12)
		{ // EndDevice Input --> Parent Output
			{0xFF, 0xFF} // TERMINATOR
		},
		{ //  Child Output <-- Parent Input
			{PORT_OUT1,	PORT_INPUT1},
			{PORT_OUT2,	PORT_INPUT2},
			{PORT_OUT3,	PORT_INPUT3},
			{PORT_OUT4,	PORT_INPUT4},
			{PORT_INPUT1,		PORT_OUT1},
			{PORT_INPUT2,		PORT_OUT2},
			{PORT_INPUT3,		PORT_OUT3},
			{PORT_INPUT4,		PORT_OUT4},
			{PORT_I2C_CLK,	PORT_I2C_CLK},
			{PORT_I2C_DAT,	PORT_I2C_DAT},
			{PORT_EO1,		PORT_EO1},
			{PORT_EO2,		PORT_EO2},
			{0xFF, 0xFF} // TERMINATOR
		},
	},
};

/** @ingroup MBUSA
 * MODE設定ビットからデフォルト割り当てされる論理ＩＤテーブル
 */
const uint8 au8IoModeTbl_To_LogicalID[] = {
	120, // CHILD
	0,   // PARENT
	254, // ROUTER
	123, // 32fps mode (7B)
	124, // 1sec sleep (7C)
	255, // NODEF
	240, // PAIRING
	127  // 10sec sleep (7F)
};

/** @ingroup MBUSA
 * MODE設定ビットからデフォルト割り当てされる論理ＩＤテーブル
 */
const uint32 au32ChMask_Preset[] = {
		CHMASK,
		CHMASK_1,
		CHMASK_2,
		CHMASK_3,
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

/** @ingroup MBUSA
 * テーブル設定の書き換え
 * @param u8tbl テーブル番号
 * @param bParent TRUE:親機 FALSE:子機
 * @return TRUE:成功 FALSE:失敗（デフォルトのテーブル）
 */
bool_t bPortTblInit(uint8 u8tbl, bool_t bParent) {
	int i;

	u8_PORT_INPUT_COUNT = 0;
	u32_PORT_INPUT_MASK = 0UL;
	u8_PORT_OUTPUT_COUNT = 0;
	u32_PORT_OUTPUT_MASK = 0UL;

	uint8 u8DevID = bParent ? 1 : 0;
	uint8 u8TblID = (u8tbl >= MAX_IOTBL_SETS) ? 0 : u8tbl;

	// 入力テーブル
	for (i = 0; i < MAX_IO_TBL; i++) {
		uint8 u8in = au8PortTbl[u8TblID][u8DevID][i][u8DevID];
		if (u8in == 0xFF) {
			break;
		} else {
			au8PortTbl_DIn[i] = u8in;
			u32_PORT_INPUT_MASK |= (1UL << u8in);
		}
	}
	u8_PORT_INPUT_COUNT = i;

	// 出力テーブル
	for (i = 0; i < MAX_IO_TBL; i++) {
		uint8 u8out = au8PortTbl[u8TblID][1-u8DevID][i][u8DevID];
		if (u8out == 0xFF) {
			break;
		} else {
			au8PortTbl_DOut[i] = u8out;
			u32_PORT_OUTPUT_MASK |= (1UL << u8out);
		}
	}
	u8_PORT_OUTPUT_COUNT = i;

	return (u8tbl < MAX_IOTBL_SETS);
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
