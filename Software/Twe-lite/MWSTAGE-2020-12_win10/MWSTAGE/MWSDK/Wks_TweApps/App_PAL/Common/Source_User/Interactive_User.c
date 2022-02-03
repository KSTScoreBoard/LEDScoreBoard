/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// 本ファイルは Interactive.c から include される

/**
 * フラッシュ設定構造体をデフォルトに巻き戻します。
 * - ここでシステムのデフォルト値を決めています。
 *
 * @param p 構造体へのアドレス
 */
#include "config.h"

static void Config_vSetDefaults(tsFlashApp *p) {
	p->u32appid = APP_ID;
	p->u32chmask = CHMASK;
	p->u8ch = CHANNEL;

	p->u8pow = 0x13;
	p->u8id = 0;

	p->u32baud_safe = UART_BAUD_SAFE;
	p->u8parity = 0;

	p->u16RcClock = 0;
	p->u32Slp = DEFAULT_SLEEP_DUR;
	p->u32param = 0;

#ifndef USE_CUE
	memset( p->au8Event, 0, 137 );
	memcpy( p->au8Event, "0180002A0208002A0300802A0488002A0580802A0608802A0880000A1008000A", 8*8);
	p->u8EventNum = 8;
#endif

	p->u32Opt = 1; // デフォルトの設定ビット

	p->u32EncKey = DEFAULT_ENC_KEY;
}
