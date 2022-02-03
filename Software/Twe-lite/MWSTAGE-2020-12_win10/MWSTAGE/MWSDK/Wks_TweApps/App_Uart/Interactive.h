/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * Interactive.h
 *
 *  Created on: 2014/04/21
 *      Author: seigo13
 */

#ifndef INTERACTIVE_H_
#define INTERACTIVE_H_

#include "flash.h"

void vConfig_SetDefaults(tsFlashApp *p);
void vConfig_UnSetAll(tsFlashApp *p);
void vConfig_SaveAndReset();

void vProcessInputByte(uint8 u8Byte);
void vProcessInputString(tsInpStr_Context *pContext);

void vSerUpdateScreen();

/** @ingroup FLASH
 * フラッシュ設定内容の列挙体
 */
enum {
	E_APPCONF_APPID = 0,      //!< E_APPCONF_APPID
	E_APPCONF_CHMASK = 1,     //!< E_APPCONF_CHMASK
	E_APPCONF_POWER = 2,      //!< E_APPCONF_POWER
	E_APPCONF_ID = 3,         //!< E_APPCONF_ID
	E_APPCONF_ROLE = 4,       //!< E_APPCONF_ROLE
	E_APPCONF_LAYER  = 5,     //!< E_APPCONF_LAYER
	E_APPCONF_UART_MODE = 6,  //!< E_APPCONF_UART_MODE
	E_APPCONF_BAUD_SAFE = 7,  //!< E_APPCONF_BAUD_SAFE
	E_APPCONF_BAUD_PARITY = 8,//!< E_APPCONF_BAUD_PARITY
	E_APPCONF_CRYPT_MODE = 9, //!< E_APPCONF_CRYPT_MODE
	E_APPCONF_CRYPT_KEY = 10,  //!< E_APPCONF_CRYPT_KEY
	E_APPCONF_HANDLE_NAME = 11,//!< E_APPCONF_HANDLE_NAME
	E_APPCONF_UART_LINE_SEP = 12, //!< E_APPCONF_UART_LINE_SEP
	E_APPCONF_OPT_BITS = 0x80,   //!< E_APPCONF_OPT_BITS
	E_APPCONF_VOID            //!< E_APPCONF_TEST
};

/**
 * スリープ用のIOピンのプルアップ停止
 */
#define E_APPCONF_OPT_DISABLE_PULL_UP_SLEEP_PIN 0x1UL
#define IS_APPCONF_OPT_DISABLE_PULL_UP_SLEEP_PIN() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_DISABLE_PULL_UP_SLEEP_PIN) //!< E_APPCONF_OPT_DISABLE_PULL_UP_SLEEP_PIN 判定 @ingroup FLASH

/**
 * 透過モード選択ピンのプルアップの停止
 */
#define E_APPCONF_OPT_DISABLE_PULL_UP_DI_PIN 0x2UL
#define IS_APPCONF_OPT_DISABLE_PULL_UP_DI_PIN() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_DISABLE_PULL_UP_DI_PIN) //!< E_APPCONF_OPT_DISABLE_PULL_UP_DI_PIN 判定 @ingroup FLASH

/**
 * 透過モードで、改行コード (0x0D) で送信を行うオプション(それまでは何もしない)
 */
#define E_APPCONF_OPT_M3_SLEEP_AT_ONCE 0x4UL
#define IS_APPCONF_OPT_M3_SLEEP_AT_ONCE() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_M3_SLEEP_AT_ONCE) //!< E_APPCONF_OPT_M3_SLEEP_AT_ONCE 判定 @ingroup FLASH

/**
 * 透過モードで、改行コード (0x0D) で送信を行うオプション(それまでは何もしない)
 */
#define E_APPCONF_OPT_TX_TRIGGER_CHAR 0x100UL
#define IS_APPCONF_OPT_TX_TRIGGER_CHAR() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_TX_TRIGGER_CHAR) //!< E_APPCONF_OPT_TX_TRIGGER_CHAR 判定 @ingroup FLASH

/**
 * UARTの入力が確定したとき、送信中処理中である場合は、この系列を破棄する
 * (結果として新しい系列が優先される)
 */
#define E_APPCONF_OPT_TX_NEWER 0x200UL
#define IS_APPCONF_OPT_TX_NEWER() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_TX_NEWER) //!< E_APPCONF_OPT_TX_NEWER 判定 @ingroup FLASH

/**
 * 書式モードで送信完了の応答メッセージを表示しない
 */
#define E_APPCONF_OPT_NO_TX_RESULT 0x1000UL
#define IS_APPCONF_OPT_NO_TX_RESULT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_NO_TX_RESULT) //!< E_APPCONF_OPT_NO_TX_RESULT 判定 @ingroup FLASH

/**
 * 書式モード <-> プロンプト無しチャットモードで通信する
 */
#define E_APPCONF_OPT_FORMAT_TO_NOPROMPT 0x2000UL
#define IS_APPCONF_OPT_FORMAT_TO_NOPROMPT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_FORMAT_TO_NOPROMPT) //!< E_APPCONF_OPT_NO_TX_RESULT 判定 @ingroup FLASH

/**
 * UART ボーレート設定の強制
 */
#define E_APPCONF_OPT_UART_FORCE_SETTINGS 0x10000UL
#define IS_APPCONF_OPT_UART_FORCE_SETTINGS() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_FORCE_SETTINGS) //!< E_APPCONF_OPT_UART_FORCE_SETTINGS 判定 @ingroup FLASH

/**
 * UART の副ポートに出力する
 */
#define E_APPCONF_OPT_UART_SLAVE_OUT 0x20000UL
#define IS_APPCONF_OPT_UART_SLAVE_OUT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_SLAVE_OUT) //!< E_APPCONF_OPT_UART_SLAVE_OUT 判定 @ingroup FLASH

/**
 * UART の副ポートと主ポートを入れ替える
 */
#define E_APPCONF_OPT_UART_SWAP_PORT 0x40000UL
#define IS_APPCONF_OPT_UART_SWAP_PORT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_SWAP_PORT) //!< E_APPCONF_OPT_UART_SLAVE_OUT 判定 @ingroup FLASH

/**
 * LayreNetwork で中継機が直上の経路のみに接続する
 */
#define E_APPCONF_OPT_LAYER_NWK_CONNET_ONE_LEVEL_HIGHER_ONLY 0x100000UL
#define IS_APPCONF_OPT_LAYER_NWK_CONNET_ONE_LEVEL_HIGHER_ONLY() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LAYER_NWK_CONNET_ONE_LEVEL_HIGHER_ONLY) //!< E_APPCONF_OPT_LAYER_NWK_CONNET_ONE_LEVEL_HIGHER_ONLY 判定 @ingroup FLASH

#endif /* INTERACTIVE_H_ */
