/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * cmd_config.c
 *
 *  Created on: 2014/04/21
 *      Author: seigo13
 */

#include "cmd_gen.h"

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

#include "Interactive.h"

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "sercmd_plus3.h"
#include "sercmd_gen.h"

extern tsAppData sAppData;
extern tsSerCmd_Context sSerCmdTemp;
extern tsFILE sSerStream;


/** @ingroup MASTER
 * シリアルポートに設定を書き出す
 * - 書式
 *    - OCTET: 設定番号
 *    - ?????: 設定データ（データ構造は設定に依存）
 *
 * @param u32setting 設定番号
 */
void vSerResp_GetModuleSetting(uint32 u32setting) {
	uint8 *q = sSerCmdTemp.au8data;

	S_OCTET(0xDB);
	S_OCTET(SERCMD_ID_GET_MODULE_SETTING);
	uint8 *r = q;
	S_OCTET(u32setting & 0xFF);

	switch(u32setting) {
	case E_APPCONF_APPID:
		S_BE_DWORD(FL_IS_MODIFIED_u32(appid) ? FL_UNSAVE_u32(appid) : FL_MASTER_u32(appid)); break;
	case E_APPCONF_CHMASK:
		S_BE_DWORD(FL_IS_MODIFIED_u32(chmask) ? FL_UNSAVE_u32(chmask) : FL_MASTER_u32(chmask)); break;
	case E_APPCONF_POWER:
		S_BE_WORD(FL_IS_MODIFIED_u16(power) ? FL_UNSAVE_u16(power) : FL_MASTER_u16(power)); break;
	case E_APPCONF_ID:
		S_OCTET(FL_IS_MODIFIED_u8(id) ? FL_UNSAVE_u8(id) : FL_MASTER_u8(id)); break;
	case E_APPCONF_ROLE:
		S_OCTET(FL_IS_MODIFIED_u8(role) ? FL_UNSAVE_u8(role) : FL_MASTER_u8(role)); break;
	case E_APPCONF_LAYER:
		S_OCTET(FL_IS_MODIFIED_u8(layer) ? FL_UNSAVE_u8(layer) : FL_MASTER_u8(layer)); break;
	case E_APPCONF_UART_MODE:
		S_OCTET(FL_IS_MODIFIED_u8(uart_mode) ? FL_UNSAVE_u8(uart_mode) : FL_MASTER_u8(uart_mode)); break;
	case E_APPCONF_BAUD_SAFE: // u32baud_safe
		S_BE_DWORD(FL_IS_MODIFIED_u32(baud_safe) ? FL_UNSAVE_u32(baud_safe) : FL_MASTER_u32(baud_safe)); break;
	case E_APPCONF_BAUD_PARITY:
		S_OCTET(FL_IS_MODIFIED_u8(parity) ? FL_UNSAVE_u8(parity) : FL_MASTER_u8(parity)); break;
	case E_APPCONF_CRYPT_MODE:
		S_OCTET(FL_IS_MODIFIED_u8(Crypt) ? FL_UNSAVE_u8(Crypt) : FL_MASTER_u8(Crypt)); break;
	case E_APPCONF_CRYPT_KEY:
		_C {
			int i;
			uint8 *p = sAppData.sConfig_UnSaved.au8AesKey[FLASH_APP_AES_KEY_SIZE] == 0xFF ? sAppData.sFlash.sData.au8AesKey : sAppData.sConfig_UnSaved.au8AesKey;
			for (i = 0; i < FLASH_APP_AES_KEY_SIZE; i++) {
				*q++ = *p++;
			}
		} break;
	case E_APPCONF_UART_LINE_SEP:
			S_BE_WORD(FL_IS_MODIFIED_u16(uart_lnsep) ? FL_UNSAVE_u16(uart_lnsep) : FL_MASTER_u16(uart_lnsep));
			S_OCTET(FL_IS_MODIFIED_u8(uart_lnsep_minpkt) ? FL_UNSAVE_u8(uart_lnsep_minpkt) : FL_MASTER_u8(uart_lnsep_minpkt));
			S_OCTET(FL_IS_MODIFIED_u8(uart_txtrig_delay) ? FL_UNSAVE_u8(uart_txtrig_delay) : FL_MASTER_u8(uart_txtrig_delay));
			break;
	case E_APPCONF_OPT_BITS:
		S_BE_DWORD(FL_IS_MODIFIED_u32(Opt) ? FL_UNSAVE_u32(Opt) : FL_MASTER_u32(Opt)); break;

	default:
		*r = 0xFF;
		break;
	}

	sSerCmdTemp.u16len = q - sSerCmdTemp.au8data;
	sSerCmdTemp.vOutput(&sSerCmdTemp, &sSerStream);
}

/**  @ingroup MASTER
 * シリアルポートより設定を行う処理関数
 *
 * - 書式
 *    - OCTET: 設定番号
 *    - ?????: 設定データ（データ構造は設定に依存）
 *
 * @param p 設定コマンド
 * @param u8len
 * @return 有効ならTRUE, 無効ならFALSE
 */
bool_t bSerCmd_SetModuleSetting(uint8 *p, uint8 u8len) {
	uint8 u8setting = G_OCTET();
	u8len--;

	bool_t bRet = TRUE;

	switch(u8setting) {
	case E_APPCONF_APPID:
		if (u8len >= 4) FL_UNSAVE_u32(appid) = G_BE_DWORD(); else bRet = FALSE;
		break;

	case E_APPCONF_CHMASK:
		if (u8len >= 4) FL_UNSAVE_u32(chmask) = G_BE_DWORD(); else bRet = FALSE;
		break;

	case E_APPCONF_ID:
		if (u8len >= 1){
			uint8 u8id = G_OCTET();
			FL_UNSAVE_u8(id)= ((u8id == 0) ? 121 : u8id);
		}else{
			bRet = FALSE;
		}
		break;

	case E_APPCONF_POWER:
		if (u8len >= 2) FL_UNSAVE_u16(power) = G_BE_WORD(); else bRet = FALSE;
		break;

	case E_APPCONF_ROLE:
		if (u8len >= 1) FL_UNSAVE_u8(role) = G_OCTET(); else bRet = FALSE;
		break;

	case E_APPCONF_LAYER:
		if (u8len >= 1) FL_UNSAVE_u8(layer) = G_OCTET(); else bRet = FALSE;
		break;

	case E_APPCONF_UART_MODE:
		if (u8len >= 1) FL_UNSAVE_u8(uart_mode) = G_OCTET(); else bRet = FALSE;
		break;

	case E_APPCONF_BAUD_SAFE:
		if (u8len >= 4) FL_UNSAVE_u32(baud_safe) = G_BE_DWORD(); else bRet = FALSE;
		break;

	case E_APPCONF_BAUD_PARITY:
		if (u8len >= 1) FL_UNSAVE_u8(parity) = G_OCTET(); else bRet = FALSE;
		break;

	case E_APPCONF_CRYPT_MODE:
		if (u8len >= 1) FL_UNSAVE_u8(Crypt)= G_OCTET(); else bRet = FALSE;
		break;

	case E_APPCONF_CRYPT_KEY:
		bRet = FALSE;
		if (u8len >= FLASH_APP_AES_KEY_SIZE) {
			int i;
			for (i = 0; i < FLASH_APP_AES_KEY_SIZE; i++) {
				sAppData.sConfig_UnSaved.au8AesKey[i] = G_OCTET();
			}
			sAppData.sConfig_UnSaved.au8AesKey[FLASH_APP_AES_KEY_SIZE] = 0;

			bRet = TRUE;
		}
		break;

	case E_APPCONF_UART_LINE_SEP:
		if (u8len < 2) bRet = FALSE;
		if (u8len >= 2) FL_UNSAVE_u16(uart_lnsep)= G_BE_WORD();
		if (u8len >= 3) FL_UNSAVE_u8(uart_lnsep_minpkt) = G_OCTET();
		if (u8len >= 4) FL_UNSAVE_u8(uart_txtrig_delay) = G_OCTET();
		break;

	default:
		bRet = FALSE;
		break;
	}

	return bRet;
}
