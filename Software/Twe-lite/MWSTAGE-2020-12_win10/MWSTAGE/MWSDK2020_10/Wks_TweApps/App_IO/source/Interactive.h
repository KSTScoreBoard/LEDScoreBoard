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
#include "input_string.h"

uint8 Config_u8SetDefaults(tsFlashApp *p);
void Config_vUnSetAll(tsFlashApp *p);

void Config_vSave();
void Config_vSaveAndReset();

bool_t Config_bSetModuleParam(uint8 *p, uint8 u8len);

void vProcessInputByte(uint8 u8Byte);
void vProcessInputString(tsInpStr_Context *pContext);

void vSerUpdateScreen();
extern void vSerInitMessage();

/****************************************************************************
 * フラッシュ設定情報
 ***************************************************************************/

#define FL_MASTER_u32(c) sAppData.sFlash.sData.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u32(c) sAppData.sConfig_UnSaved.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u32(c) (sAppData.sConfig_UnSaved.u32##c != 0xFFFFFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u16(c) sAppData.sFlash.sData.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u16(c) sAppData.sConfig_UnSaved.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u16(c) (sAppData.sConfig_UnSaved.u16##c != 0xFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u8(c) sAppData.sFlash.sData.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u8(c) sAppData.sConfig_UnSaved.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u8(c) (sAppData.sConfig_UnSaved.u8##c != 0xFF) //!< 構造体要素アクセス用のマクロ @ingroup FLASH

/** @ingroup FLASH
 * フラッシュ設定内容の列挙体
 */
enum {
	E_APPCONF_APPID = 0,     //!< アプリケーションID
	E_APPCONF_CHMASK = 1,    //!< チャネルマスク
	E_APPCONF_TX_POWER = 2,  //!< TX 出力
	E_APPCONF_ID = 3,        //!< 8bitのID(ネットワークアドレス)
	E_APPCONF_ROLE = 4,      //!<
	E_APPCONF_LAYER  = 5,    //!<
	E_APPCONF_BAUD_SAFE = 7, //!< BPS ピンをGにしたときのボーレート
	E_APPCONF_BAUD_PARITY = 8, //!< BPS ピンをGにしたときのパリティ設定 (0:None, 1:Odd, 2:Even)
	E_APPCONF_CRYPT_MODE = 9,  //!< 暗号化モード
	E_APPCONF_CRYPT_KEY = 10,   //!< 暗号化鍵
	E_APPCONF_SLEEP4,    //!< mode4 のスリープ期間設定
	E_APPCONF_SLEEP7,    //!< mode7 のスリープ期間設定
	E_APPCONF_FPS,       //!< 連続送信モードの秒あたりの送信数
	E_APPCONF_PWM_HZ,    //!< PWM の周波数
	E_APPCONF_SYS_HZ,    //!<
	E_APPCONF_HOLD_MASK,   //!< Lo 維持の対象ポート
	E_APPCONF_HOLD_DUR,    //!< Lo 維持の期間
	E_APPCONF_OPT = 0x80,       //!< DIOの入力方法に関する設定
	E_APPCONF_TEST
};

/** @ingroup FLASH
 * フラッシュ設定で ROLE に対する要素名の列挙体
 * (未使用、将来のための拡張のための定義)
 */
enum {
	E_APPCONF_ROLE_MAC_NODE = 0,  //!< MAC直接のノード（親子関係は無し）
	E_APPCONF_ROLE_NWK_MASK = 0x10, //!< NETWORKモードマスク
	E_APPCONF_ROLE_PARENT,          //!< NETWORKモードの親
	E_APPCONF_ROLE_ROUTER,        //!< NETWORKモードの子
	E_APPCONF_ROLE_ENDDEVICE,     //!< NETWORKモードの子（未使用、スリープ対応）
	E_APPCONF_ROLE_SILENT = 0x7F, //!< 何もしない（設定のみ)
};

#define E_APPCONF_OPT_LOW_LATENCY_INPUT 0x0001UL //!< Hi>Lo を検知後直ぐに送信する。 @ingroup FLASH
#define IS_APPCONF_OPT_LOW_LATENCY_INPUT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT) //!< E_APPCONF_OPT_LOW_LATENCY_INPUT 判定 @ingroup FLASH

#define E_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT 0x0002UL //!< スリープを H>L 検出した場合に、割り込み要因のポートのみ送信する @ingroup FLASH
#define IS_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT) //!< E_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT 判定 @ingroup FLASH

#define E_APPCONF_OPT_ACK_MODE 0x0010 //!< ACK付き通信を行う @ingroup FLASH
#define IS_APPCONF_OPT_ACK_MODE() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_ACK_MODE) //!< E_APPCONF_OPT_ACK_MODE判定 @ingroup FLASH

#define E_APPCONF_OPT_NO_REGULAR_TX 0x0020 //!< REGULAR 通信しない @ingroup FLASH
#define IS_APPCONF_OPT_NO_REGULAR_TX() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_NO_REGULAR_TX) //!< E_APPCONF_OPT_NO_REGULAR_TX判定 @ingroup FLASH

#define E_APPCONF_OPT_ON_PRESS_TRANSMIT 0x0100UL //!< 押し下げ時のみ送信する特殊動作モード。 @ingroup FLASH
#define IS_APPCONF_OPT_ON_PRESS_TRANSMIT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_ON_PRESS_TRANSMIT) //!< E_APPCONF_OPT_ON_PRESS_TRANSMIT判定 @ingroup FLASH

#define E_APPCONF_OPT_NO_C1C2_CONFIG 0x0200 //!< C1/C2ポートによる設定切り替えを無効にする @ingroup FLASH
#define IS_APPCONF_OPT_NO_C1C2_CONFIG() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_NO_C1C2_CONFIG) //!< E_APPCONF_OPT_NO_C1C2_CONFIG判定 @ingroup FLASH

#define E_APPCONF_OPT_NO_PULLUP_FOR_INPUT			0x000800UL //!< DI入力にプルアップを適用しない。@ingroup FLASH
#define IS_APPCONF_OPT_NO_PULLUP_FOR_INPUT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_NO_PULLUP_FOR_INPUT) //!< E_APPCONF_OPT_NO_C1C2_CONFIG判定 @ingroup FLASH

#define E_APPCONF_OPT_PORT_TBL1 0x1000UL //!< ポートの割り当てテーブル @ingroup FLASH
#define E_APPCONF_OPT_PORT_TBL2 0x2000UL //!< ポートの割り当てテーブル @ingroup FLASH
#define IS_APPCONF_OPT_PORT_TBL1() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_PORT_TBL1) //!< E_APPCONF_OPT_PORT_TBL1判定 @ingroup FLASH
#define IS_APPCONF_OPT_PORT_TBL2() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_PORT_TBL2) //!< E_APPCONF_OPT_PORT_TBL2判定 @ingroup FLASH

#define E_APPCONF_OPT_CHILD_RECV_OTHER_NODES 0x10000 //!< 子機通常モードで受信を可能とする @ingroup FLASH
#define IS_APPCONF_OPT_CHILD_RECV_OTHER_NODES() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_CHILD_RECV_OTHER_NODES) //!< E_APPCONF_OPT_CHILD_RECV_OTHER_NODES判定 @ingroup FLASH

#define E_APPCONF_OPT_CHILD_RECV_NO_IO_DATA 0x20000 //!< IO状態のUART出力を抑制する @ingroup FLASH
#define IS_APPCONF_OPT_CHILD_RECV_NO_IO_DATA() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_CHILD_RECV_NO_IO_DATA) //!< E_APPCONF_OPT_CHILD_RECV_NO_IO_DATA判定 @ingroup FLASH

#define E_APPCONF_OPT_WATCHDOG_OUTPUT 0x40000 //!< PORT_EI2 をウォッチドッグ出力とする @ingroup FLASH
#define IS_APPCONF_OPT_WATCHDOG_OUTPUT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_WATCHDOG_OUTPUT) //!< E_APPCONF_OPT_WATCHDOG_OUTPUT判定 @ingroup FLASH

#define E_APPCONF_OPT_DO_INVERT 0x400000UL //!< DIO出力を反転する @ingroup FLASH
#define IS_APPCONF_OPT_DO_INVERT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_DO_INVERT) //!< E_APPCONF_OPT_DO_INVERT判定 @ingroup FLASH

#define E_APPCONF_OPT_DI_INVERT 0x0400UL //!< DIO入力を反転する @ingroup FLASH
#define IS_APPCONF_OPT_DI_INVERT() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_DI_INVERT) //!< E_APPCONF_OPT_DI_INVERT判定 @ingroup FLASH

/** サイレントモードの判定マクロ  @ingroup FLASH */
#define IS_APPCONF_ROLE_SILENT_MODE() (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_SILENT)

/** AES 利用のマクロ判定  @ingroup FLASH */
#define IS_CRYPT_MODE() (sAppData.sFlash.sData.u8Crypt)

/*
 * 反転を考慮したマクロ
 */
#define PORT_SET_HI(c)  vPortSet_TrueAsLo(c, (IS_APPCONF_OPT_DO_INVERT() ? TRUE : FALSE))
#define PORT_SET_LO(c)  vPortSet_TrueAsLo(c, (IS_APPCONF_OPT_DO_INVERT() ? FALSE : TRUE))
#define PORT_SET_TRUEASLO(c, t)  vPortSet_TrueAsLo(c, (IS_APPCONF_OPT_DO_INVERT() ? (!(t)) : (t)))

#endif /* INTERACTIVE_H_ */
