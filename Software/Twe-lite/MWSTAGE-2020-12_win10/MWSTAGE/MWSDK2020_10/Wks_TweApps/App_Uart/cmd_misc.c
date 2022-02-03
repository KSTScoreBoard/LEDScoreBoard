/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * cmd_misc.c
 *
 *  Created on: 2014/04/21
 *      Author: seigo13
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>
#include <AppHardwareApi.h>

#include "App_Uart.h"

#include "config.h"

#include "utils.h"
#include "input_string.h"

#include "flash.h"

#include "common.h"
#include "config.h"


// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "sercmd_plus3.h"
#include "sercmd_gen.h"

extern tsSerCmd_Context sSerCmdTemp;
extern tsFILE sSerStream;
extern tsAppData sAppData;

/** @ingroup MASTER
 * 送信完了応答を返す
 * @param u8Status True/False
 */
void vSerResp_Ack(uint8 u8Status) {
	uint8 *q = sSerCmdTemp.au8data;

	S_OCTET(0xDB);
	S_OCTET(SERCMD_ID_ACK);
	S_OCTET(u8Status);

	sSerCmdTemp.u16len = q - sSerCmdTemp.au8data;
	sSerCmdTemp.vOutput(&sSerCmdTemp, &sSerStream);
}

/** @ingroup MASTER
 * モジュールのシリアル番号を出力する。
 * @param u8Status True/False
 */
void vSerResp_GetModuleAddress() {
	uint8 *q = sSerCmdTemp.au8data;
	uint32 u32ver =
			  (((uint32)VERSION_MAIN & 0xFF) << 16)
			| (((uint32)VERSION_SUB & 0xFF) << 8)
			| (((uint32)VERSION_VAR & 0xFF) << 0);

	S_OCTET(0xDB);
	S_OCTET(SERCMD_ID_GET_MODULE_ADDRESS);
	S_BE_DWORD(APP_ID);
	S_BE_DWORD(u32ver);
	S_OCTET(sAppData.u8AppLogicalId);
	S_BE_DWORD(ToCoNet_u32GetSerial());
	S_OCTET(sAppData.bSilent);
	S_OCTET(sAppData.bNwkUp);

	sSerCmdTemp.u16len = q - sSerCmdTemp.au8data;
	sSerCmdTemp.vOutput(&sSerCmdTemp, &sSerStream);
}

/** @ingroup MASTER
 * 送信完了応答を返す
 * @param u8RspId 応答ID
 * @param u8Status 完了ステータス
 */
void vSerResp_TxEx(uint8 u8RspId, uint8 u8Status) {
	uint8 *q = sSerCmdTemp.au8data;

	S_OCTET(0xDB);
	S_OCTET(SERCMD_ID_TRANSMIT_EX_RESP);
	S_OCTET(u8RspId);
	S_OCTET(u8Status);

	sSerCmdTemp.u16len = q - sSerCmdTemp.au8data;
	sSerCmdTemp.vOutput(&sSerCmdTemp, &sSerStream);
}

/** @ingroup MASTER
 * SILENTモード状態を返す
 * @param u8RspId 応答ID
 * @param u8Status 完了ステータス
 */
void vSerResp_Silent() {
	uint8 *q = sSerCmdTemp.au8data;

	S_OCTET(0xDB);
	S_OCTET(SERCMD_ID_MODULE_CONTROL);
	S_OCTET(SERCMD_ID_MODULE_CONTROL_INFORM_SILENT);
	S_OCTET(sAppData.bSilent);

	sSerCmdTemp.u16len = q - sSerCmdTemp.au8data;
	sSerCmdTemp.vOutput(&sSerCmdTemp, &sSerStream);
}
