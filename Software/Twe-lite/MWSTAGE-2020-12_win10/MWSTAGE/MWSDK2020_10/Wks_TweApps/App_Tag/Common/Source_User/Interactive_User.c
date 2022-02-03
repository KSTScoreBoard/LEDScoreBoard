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
static void Config_vSetDefaults(tsFlashApp *p) {
	p->u32appid = APP_ID;
	p->u32chmask = CHMASK;
	p->u8ch = CHANNEL;

#ifdef ENDDEVICE_INPUT
	p->u8pow = 0x13;
#else
	p->u8pow = 3;
#endif
	p->u8id = 0;

	p->u32baud_safe = UART_BAUD_SAFE;
	p->u8parity = 0;

#ifdef ENDDEVICE_INPUT
	p->u16RcClock = 10000;
	p->u32Slp = DEFAULT_SLEEP_DUR_ms;

#if defined(LITE2525A) || defined(OTA) || defined(SWING)
	p->u8wait = 0;
#else
	p->u8wait = 30;
#endif

	p->u8mode = DEFAULT_SENSOR;
#if defined(LITE2525A) || defined(OTA)
	p->i16param = 15;
#elif defined(SWING)
	p->i16param = 4;
#else
	p->i16param = 0;
#endif

	p->bFlagParam = TRUE;
	memset( &p->uParam.au8Param, 0x00, PARAM_MAX_LEN);
#endif

	p->u32Opt = E_APPCONF_OPT_TO_ROUTER; // デフォルトの設定ビット
#if defined(LITE2525A) || defined(OTA)
	p->u32Opt = p->u32Opt | 0x10;	// App_TweLite宛に送る
#elif defined(SWING)
	p->u32Opt = p->u32Opt | 0x10 | 0x400;	// App_TweLite宛に送る
#endif

#ifdef ROUTER
	p->u8layer = 4; // Layer:1
	p->u32AddrHigherLayer = 0; // 指定送信先なし
#endif

	p->u32EncKey = DEFAULT_ENC_KEY;
}
