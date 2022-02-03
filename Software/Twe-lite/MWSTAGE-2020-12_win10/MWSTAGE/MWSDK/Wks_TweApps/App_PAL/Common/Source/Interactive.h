/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef INTERACTIVE_H_
#define INTERACTIVE_H_

#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

//#include "modbus_ascii.h"
#include "sercmd_gen.h"
#include "input_string.h"

#include "flash.h"

/*************************************************************************
 * アプリ依存度が高い定義
 *************************************************************************/
#include "../Source_User/Interactive_User.h"
#include "appsave.h"

/*************************************************************************
 * プロトタイプ宣言
 *************************************************************************/
void Interactive_vInit();
void Interactive_vReInit();
void Interactive_vSetMode(bool_t bVerbose, uint16 u16screen_refresh);
bool_t Interactive_bGetMode();

bool_t Config_bLoad(tsFlash *p);
bool_t Config_bSave();

bool_t Config_bUnSerialize(uint8 *pu8dat, uint16 u16len, uint8 u8fmt);
uint16 Config_u16Serialize(uint8 *pu8dat, uint16 u16len, uint8 u8fmt);

void Config_vUpdateScreen();

void vHandleSerialInput();

extern void vSerInitMessage();
extern void vProcessSerialCmd(tsSerCmd_Context *pCmd);

/*************************************************************************
 * マクロ定義
 *************************************************************************/
#define V_PRINTF(...) if (Interactive_bGetMode() || IS_APPCONF_OPT_VERBOSE()) vfPrintf(&sSerStream,__VA_ARGS__)
#define V_PUTCHAR(c) if (Interactive_bGetMode()  || IS_APPCONF_OPT_VERBOSE()) (&sSerStream)->bPutChar(sSerStream.u8Device, c)
#define V_FLUSH() if (Interactive_bGetMode()     || IS_APPCONF_OPT_VERBOSE()) WAIT_UART_OUTPUT(UART_PORT)

#define A_PRINTF(...) if (!Interactive_bGetMode()) vfPrintf(&sSerStream,__VA_ARGS__)
#define A_PUTCHAR(c) if (!Interactive_bGetMode() ) (&sSerStream)->bPutChar(sSerStream.u8Device, c)
#define A_FLUSH() if (!Interactive_bGetMode()    ) SERIAL_vFlush(UART_PORT)

#define F_PRINTF(...) vfPrintf(&sSerStream,__VA_ARGS__)
#define F_PUTCHAR(c) (&sSerStream)->bPutChar(sSerStream.u8Device, c)
#define F_FLUSH() SERIAL_vFlush(UART_PORT)

#define FL_MASTER_u32(c) sAppData.sFlash.sData.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u32(c) sConfig_UnSaved.u32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u32(c) (sConfig_UnSaved.u32##c != 0xFFFFFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u16(c) sAppData.sFlash.sData.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u16(c) sConfig_UnSaved.u16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u16(c) (sConfig_UnSaved.u16##c != 0xFFFF)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_u8(c) sAppData.sFlash.sData.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_u8(c) sConfig_UnSaved.u8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_u8(c) (sConfig_UnSaved.u8##c != 0xFF) //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_i32(c) sAppData.sFlash.sData.i32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_i32(c) sConfig_UnSaved.i32##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_i32(c) (sConfig_UnSaved.i32##c != -2147483648)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_i16(c) sAppData.sFlash.sData.i16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_i16(c) sConfig_UnSaved.i16##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_i16(c) (sConfig_UnSaved.i16##c != -32768)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define FL_MASTER_i8(c) sAppData.sFlash.sData.i8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_UNSAVE_i8(c) sConfig_UnSaved.i8##c //!< 構造体要素アクセス用のマクロ @ingroup FLASH
#define FL_IS_MODIFIED_i8(c) (sConfig_UnSaved.i8##c != -128)  //!< 構造体要素アクセス用のマクロ @ingroup FLASH

#define INIT_VAL_i32 -2147483648			//	符号ありint32の初期値
#define INIT_VAL_i16 -32768					//	符号ありint16の初期値
#define INIT_VAL_i8 -128					//	符号ありint8の初期値

#define VERSION_U32 ((VERSION_CODE << 24) | (VERSION_MAIN << 16) | (VERSION_SUB << 8) | (VERSION_VAR))

/*************************************************************************
 * その他マクロ
 *************************************************************************/
/* フラッシュ設定パリティ設定（マスク）*/
#define APPCONF_UART_CONF_PARITY_MASK 0x3
#define APPCONF_UART_CONF_STOPBIT_MASK 0x4
#define APPCONF_UART_CONF_WORDLEN_MASK 0x8

/*************************************************************************
 * その他マクロ
 *************************************************************************/
/** @ingroup FLASH
 * フラッシュ設定内容の列挙体
 */
enum {
	E_APPCONF_APPID,     //!< アプリケーションID
	E_APPCONF_CHMASK,    //!< チャネルマスク
	E_APPCONF_TX_POWER,  //!< TX 出力
	E_APPCONF_ID,        //!< 8bitのID(ネットワークアドレス)
	E_APPCONF_LAYER,     //!< ネットワークレイヤ
	E_APPCONF_ADDRHIGH,  //!< 接続先アドレス
	E_APPCONF_SLEEP_DUR, //!< スリープ期間設定
	E_APPCONF_WAIT_DUR,	 //!< センサーのウェイト待ち
	E_APPCONF_PWM_HZ,    //!< PWM の周波数
	E_APPCONF_SAMP_HZ,   //!< サンプリング周波数
	E_APPCONF_OPT,       //!< DIOの入力方法に関する設定
	E_APPCONF_BAUD_SAFE, //!< BPS ピンをGにしたときのボーレート
	E_APPCONF_BAUD_PARITY, //!< BPS ピンをGにしたときのパリティ設定 (0:None, 1:Odd, 2:Even)
	E_APPCONF_ENC_KEY,    //!< 暗号化鍵の設定
	E_APPCONF_SER_MODE,    //!< センサの設定
	E_APPCONF_SER_PARAM,    //!< センサの固有のパラメータの設定
	E_APPCONF_PARAM,    //!< センサの固有のパラメータの設定
	E_APPCONF_EVENT,    //!< LED PAL のイベント発生時のパラメータを指定する
	E_APPCONF_TEST
};

#endif /* INTERACTIVE_H_ */
