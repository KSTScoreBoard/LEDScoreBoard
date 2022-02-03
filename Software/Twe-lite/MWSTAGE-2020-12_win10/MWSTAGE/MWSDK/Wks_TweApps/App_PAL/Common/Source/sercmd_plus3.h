/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/** @file
 *
 * @defgroup SERCMD_PLUS3 プラス入力３回の判定を行う
 *
 */

#ifndef PLUS3_H_
#define PLUS3_H_

#include <jendefs.h>
// Serial options
#include <serial.h>
#include <fprintf.h>

/** @ingroup SERCMD_PLUS3
 * 内部状態
 */
typedef enum {
	E_SERCMD_PLUS3_EMPTY = 0,      //!< 入力されていない
	E_SERCMD_PLUS3_PLUS1,          //!< E_PLUS3_CMD_PLUS1
	E_SERCMD_PLUS3_PLUS2,          //!< E_PLUS3_CMD_PLUS2
	E_SERCMD_PLUS3_ERROR = 0x81,   //!< エラー状態
	E_SERCMD_PLUS3_VERBOSE_OFF = 0x90,     //!< verbose モード ON になった
	E_SERCMD_PLUS3_VERBOSE_ON     //!< verbose モード OFF になった
} tePlus3CmdState;

/** @ingroup SERCMD_PLUS3
 * 管理構造体
 */
typedef struct {
	uint32 u32timestamp; //!< タイムアウトを管理するためのカウンタ
	uint8 u8state; //!< 状態

	bool_t bverbose; //!< TRUE: verbose モード
	uint32 u32timevbs; //!< verbose モードになったタイムスタンプ
} tsSerCmdPlus3_Context;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

uint8 SerCmdPlus3_u8Parse(tsSerCmdPlus3_Context *pCmd, uint8 u8byte);
#define SerCmdPlus3_bVerbose(pCmd) (pCmd->bverbose) //!< VERBOSEモード判定マクロ @ingroup MBUSA

#endif /* MODBUS_ASCII_H_ */
