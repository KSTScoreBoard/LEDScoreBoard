/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * Interactive.c
 *
 *  Created on: 2014/04/21
 *      Author: seigo13
 */

#include <string.h>

#include "App_IO.h"

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

//#include "sercmd_plus3.h"
//#include "sercmd_gen.h"
#include "modbus_ascii.h"

#include "Interactive.h"

/*
 * 外部定義変数
 */
extern tsAppData sAppData;
extern tsFILE sSerStream;
extern tsModbusCmd sSerCmdIn;
extern tsInpStr_Context sSerInpStr;
extern uint16 u16HoldUpdateScreen;

#if 0
/**  @ingroup MASTER
 * UART のオプションを表示する
 */
static void vSerPrintUartOpt(uint8 u8conf) {
	const uint8 au8name_bit[] = { '8', '7' };
	const uint8 au8name_parity[] = { 'N', 'O', 'E' };
	const uint8 au8name_stop[] = { '1', '2' };

	V_PRINT("%c%c%c",
				au8name_bit[u8conf & APPCONF_UART_CONF_WORDLEN_MASK ? 1 : 0],
				au8name_parity[u8conf & APPCONF_UART_CONF_PARITY_MASK],
				au8name_stop[u8conf & APPCONF_UART_CONF_STOPBIT_MASK ? 1 : 0]);
}
#endif

/** @ingroup MASTER
 * チャネル一覧を表示する
 */
static void vSerPrintChannelMask(uint32 u32mask) {
	uint8 au8ch[MAX_CHANNELS], u8ch_idx = 0;
	int i;
	memset(au8ch,0,MAX_CHANNELS);

	for (i = 11; i <= 26; i++) {
		if (u32mask & (1UL << i)) {
			if (u8ch_idx) {
				V_PUTCHAR(',');
			}
			V_PRINT("%d", i);
			au8ch[u8ch_idx++] = i;
		}

		if (u8ch_idx == MAX_CHANNELS) {
			break;
		}
	}
}

/** @ingroup FLASH
 * カスタムデフォルトがロードできるならロードし、設定データエリアに書き込む
 *
 * @param p 書き込むデータエリア
 * @return TRUE: ロード出来た FALSE: データなし
 */
static bool_t Config_bSetCustomDefaults(tsFlashApp *p) {
	tsFlash *pFlashCustom = NULL;

	tsFlash sFlashCustomDefault;

	uint32 u32addr = *(uint32*)(0x80020); // u32addr は４バイト境界にアラインされているはずだが・・・

	// u32addr のチェック
	if (u32addr > (32*1024*5 - 1024) || (u32addr & 0x3)) {
		// サイズはフラッシュの容量内で、
		// u32addr はバイト境界にアラインされるはず
		return FALSE;
	}

	// u32addr を示すフラッシュ領域をチェックしてみる
	pFlashCustom = (tsFlash *)(u32addr + 0x80000);
	if (!bFlash_DataValidateHeader(pFlashCustom)) {
		pFlashCustom = NULL;
	}
	// バイト境界にデータが無い場合（最大３バイト遡ってチェックする)
	int i;
	for (i = 1; i < 4; i++) {
		if (!pFlashCustom) {
			memcpy((void*)&sFlashCustomDefault, (void*)(u32addr + 0x80000 - i), sizeof(tsFlash));

			if (bFlash_DataValidateHeader(&sFlashCustomDefault)) {
				pFlashCustom = &sFlashCustomDefault;
				break;
			}
		}
	}

	if (pFlashCustom) {
		*p = pFlashCustom->sData; // データを上書き
		return TRUE;
	} else {
		return FALSE;
	}
}

/** @ingroup FLASH
 * フラッシュ設定構造体をデフォルトに巻き戻す。
 * @param p 構造体へのアドレス
 * @return bit0: Load Custom Defaults
 */
uint8 Config_u8SetDefaults(tsFlashApp *p) {
	uint8 u8ret = 0;
	if (Config_bSetCustomDefaults(p)) {
		// カスタムデフォルト
		u8ret |= 0x01;
	} else
	{
		// コンパイルデフォルト
		p->u32appid = APP_ID;
		p->u32chmask = CHMASK;
		p->u8pow = 3;
		p->u8id = 0;
		p->u8role = E_APPCONF_ROLE_MAC_NODE;
		p->u8layer = 1;

		p->u16SleepDur_ms = (MODE4_SLEEP_DUR_ms);
		p->u16SleepDur_s = (MODE7_SLEEP_DUR_ms / 1000);
		p->u8Fps = 16;

		p->u32baud_safe = UART_BAUD_SAFE;
		p->u8parity = 0; // none

		p->u32Opt = 0;

		p->u8Crypt = 0;
		p->au8AesKey[0] = 0;

		p->u32HoldMask = 0;
		p->u16HoldDur_ms = 1000;
	}
	return u8ret;
}

/** @ingroup FLASH
 * フラッシュ設定構造体を全て未設定状態に巻き戻す。
 * @param p 構造体へのアドレス
 */
void Config_vUnSetAll(tsFlashApp *p) {
	memset (p, 0xFF, sizeof(tsFlashApp));
}

/** @ingroup FLASH
 * フラッシュ設定構造体を全て未設定状態に巻き戻す。
 * @param p 構造体へのアドレス
 */
static void Config_vOutputXmodem(tsFlash *pFlash) {
	int nData = sizeof(tsFlash);

	// sizeof(tsFlash)のサイズチェック(サイズが超過するとコンパイル時エラーになる)
	// 末尾バイトを 0x00 にしたいため (Ctrl+Z だと切り詰められるため) １バイト余裕を持たせる
	BUILD_BUG_ON(sizeof(tsFlash) > XMODEM_BLOCK_SIZE - 1);

	// --> SOH
	V_PUTCHAR(ASC_SOH);

	// --> ブロック番号 (0x01)
	V_PUTCHAR(0x01);

	// --> ブロック番号反転 (0xfe)
	V_PUTCHAR(0xfe);

	// --> データ(128バイト)
	int i;
	uint8 *p = (uint8 *)pFlash;
	uint8 u8sum = 0, c;
	for (i = 0; i < XMODEM_BLOCK_SIZE; i++) {
		if (i < nData) {
			c = p[i];
		} else if (i == nData) {
			c = 0x00; // 末尾は 0x00 とする
		} else {
			c = ASC_CTRL_Z; // 残りは CTRL+Z
		}

		u8sum += c;
		V_PUTCHAR(c);
	}

	// --> チェックサム
	V_PUTCHAR(u8sum);

	// この後 <-- ACK, --> EOT と続くが、vProcessInputByte() で処理する
}

/** @ingroup FLASH
 * UnSaved データを sFlash.sData にシンクロする
 */
static void Config_vSyncUnsaved(tsFlash *pFlash, tsFlashApp *pAppData) {
	if (pAppData->u32appid != 0xFFFFFFFF) {
		pFlash->sData.u32appid = pAppData->u32appid;
	}
	if (pAppData->u32chmask != 0xFFFFFFFF) {
		pFlash->sData.u32chmask = pAppData->u32chmask;
	}
	if (pAppData->u8id != 0xFF) {
		pFlash->sData.u8id = pAppData->u8id;
	}
	if (pAppData->u8pow != 0xFF) {
		pFlash->sData.u8pow = pAppData->u8pow;
	}
	if (pAppData->u8layer != 0xFF) {
		pFlash->sData.u8layer = pAppData->u8layer;
	}
	if (pAppData->u8role != 0xFF) {
		pFlash->sData.u8role = pAppData->u8role;
	}
	if (pAppData->u16SleepDur_ms != 0xFFFF) {
		pFlash->sData.u16SleepDur_ms = pAppData->u16SleepDur_ms;
	}
	if (pAppData->u16SleepDur_s != 0xFFFF) {
		pFlash->sData.u16SleepDur_s = pAppData->u16SleepDur_s;
	}
	if (pAppData->u8Fps != 0xFF) {
		pFlash->sData.u8Fps = pAppData->u8Fps;
	}
	if (pAppData->u32baud_safe != 0xFFFFFFFF) {
		pFlash->sData.u32baud_safe = pAppData->u32baud_safe;
	}
	if (pAppData->u8parity != 0xFF) {
		pFlash->sData.u8parity = pAppData->u8parity;
	}
	if (pAppData->u32Opt != 0xFFFFFFFF) {
		pFlash->sData.u32Opt = pAppData->u32Opt;
	}
	if (pAppData->u8Crypt != 0xFF) {
		pFlash->sData.u8Crypt = pAppData->u8Crypt;
	}
	{
		int i;
		for (i = 0; i < sizeof(pAppData->au8AesKey); i++) {
			if (pAppData->au8AesKey[i] != 0xFF) break;
		}
		if (i != sizeof(pAppData->au8AesKey)) {
			memcpy(pFlash->sData.au8AesKey,
					pAppData->au8AesKey,
					sizeof(pAppData->au8AesKey));
		}
	}
	if (pAppData->u32HoldMask != 0xFFFFFFFF) {
		pFlash->sData.u32HoldMask = pAppData->u32HoldMask;
	}
	if (pAppData->u16HoldDur_ms != 0xFFFF) {
		pFlash->sData.u16HoldDur_ms = pAppData->u16HoldDur_ms;
	}

	pFlash->sData.u32appkey = APP_ID;
	pFlash->sData.u32ver = VERSION_U32;
}

/** @ingroup FLASH
 * フラッシュまたはEEPROMへの保存を行う。
 */
void Config_vSave() {
	tsFlash sFlash = sAppData.sFlash;

	Config_vSyncUnsaved(&sFlash, &sAppData.sConfig_UnSaved);
	bool_t bRet = bFlash_Write(&sFlash, FLASH_SECTOR_NUMBER - 1, 0);
	V_PRINT("!INF Write config %s"LB, bRet ? "Success" : "Failed");
	Config_vUnSetAll(&sAppData.sConfig_UnSaved);
	vWait(100000);
}

/** @ingroup FLASH
 * フラッシュまたはEEPROMへの保存とリセットを行う。
 */
void Config_vSaveAndReset() {
	Config_vSave();

	V_PRINT("!INF RESET SYSTEM..." LB);
	vWait(1000000);
	vAHI_SwReset();
}

/** @ingroup MASTER
 * １バイト入力コマンドの処理\n
 * - 設定値の入力が必要な項目の場合、INPSTR_vStart() を呼び出して文字列入力モードに遷移します。
 * - フラッシュへのセーブ時の手続きでは、sAppData.sConfig_UnSaved 構造体で入力が有ったものを
 *   sFlash.sData 構造体に格納しています。
 * - デバッグ用の確認コマンドも存在します。
 *
 * @param u8Byte 入力バイト
 */
void vProcessInputByte(uint8 u8Byte) {
	static uint8 u8lastbyte;

	switch (u8Byte) {
	case 0x0d: case 'h': case 'H':
		// 画面の書き換え
		u16HoldUpdateScreen = 1;
		break;

	case 'a': // set application ID
		V_PRINT("Input Application ID (HEX:32bit): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8, E_APPCONF_APPID);
		break;

	case 'c': // チャネルの設定
		V_PRINT("Input Channel(s) (e.g. 11,16,21): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 8, E_APPCONF_CHMASK);
		break;

	case 'i': // set application role
		V_PRINT("Input Device ID (DEC:1-100): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 3, E_APPCONF_ID);
		break;

	case 'x': // 出力の変更
		V_PRINT("Rf Power/Retry"
				LB "   YZ Y=Retry(0:default,F:0,1-9:count"
				LB "      Z=Power(3:Max,2,1,0:Min)"
				LB "Input: "
		);
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 2, E_APPCONF_TX_POWER);
		break;

	case 't': // set application role
		V_PRINT("Input mode4 sleep dur[ms] (DEC:100-10000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_SLEEP4);
		break;

	case 'y': // set application role
		V_PRINT("Input mode7 sleep dur[ms] (DEC:0-10000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_SLEEP7);
		break;

	case 'f': // set application role
		V_PRINT("Input mode3 fps (DEC:4,8,16,32): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 2, E_APPCONF_FPS);
		break;

	case 'b': // ボーレートの変更
		V_PRINT("Input baud rate (DEC:9600-230400): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 10, E_APPCONF_BAUD_SAFE);
		break;

	case 'p': // パリティの変更
		V_PRINT("Input parity (N, E, O): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 1, E_APPCONF_BAUD_PARITY);
		break;

	case 'o': // オプションビットの設定
		V_PRINT("Input option bits (HEX): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8, E_APPCONF_OPT);
		break;

	case 'C':
		V_PRINT("Input crypt mode (0,1): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 2, E_APPCONF_CRYPT_MODE);
		break;

	case 'K':
		V_PRINT("Input crypt key: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 32, E_APPCONF_CRYPT_KEY);
		break;

	case 'd': // ホールドモード(マスク)
		V_PRINT("Input Hold mask (e.g. DI2,4 -> 000000001010): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, MAX_IO_TBL, E_APPCONF_HOLD_MASK);
		break;

	case 'D': // ホールド時間
		V_PRINT("Input Hold duration[ms] (DEC:20-64000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 6, E_APPCONF_HOLD_DUR);
		break;

	case 'S':
		// フラッシュへのデータ保存
		if (u8lastbyte == 'R') {
			// R S と連続入力された場合は、フラッシュエリアを消去する
			V_PRINT("!INF CLEAR SAVE AREA.");
			bFlash_Erase(FLASH_SECTOR_NUMBER - 1); // SECTOR ERASE

			vWait(1000000);
			vAHI_SwReset();
		} else {
			Config_vSaveAndReset();
		}
		break;

	case 'R':
		Config_u8SetDefaults(&sAppData.sConfig_UnSaved);
		u16HoldUpdateScreen = 1;
		break;

	case '$':
		sAppData.u8DebugLevel++;
		if(sAppData.u8DebugLevel > 5) sAppData.u8DebugLevel = 0;

		V_PRINT("* set App debug level to %d." LB, sAppData.u8DebugLevel);
		break;

	case '@':
		_C {
			static uint8 u8DgbLvl;

			u8DgbLvl++;
			if(u8DgbLvl > 5) u8DgbLvl = 0;
			ToCoNet_vDebugLevel(u8DgbLvl);

			V_PRINT("* set NwkCode debug level to %d." LB, u8DgbLvl);
		}
		break;

	case '!':
		// リセット
		V_PRINT("!INF RESET SYSTEM.");
		vWait(1000000);
		vAHI_SwReset();
		break;

	case '#': // info
		_C {
			V_PRINT("*** TWELITE NET(ver%08X) ***" LB, ToCoNet_u32GetVersion());
			V_PRINT("* AppID %08x, LongAddr, %08x, ShortAddr %04x, Tk: %d" LB,
					sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress, u32TickCount_ms);
			if (sAppData.bFlashLoaded) {
				V_PRINT("** Conf "LB);
				V_PRINT("* AppId = %08x"LB, sAppData.sFlash.sData.u32appid);
				V_PRINT("* ChMsk = %08x"LB, sAppData.sFlash.sData.u32chmask);
				V_PRINT("* Ch=%d, Role=%d, Layer=%d"LB,
						sToCoNet_AppContext.u8Channel,
						sAppData.sFlash.sData.u8role,
						sAppData.sFlash.sData.u8layer);
			} else {
				V_PRINT("** Conf: none"LB);
			}
		}
		break;

	case 'V':
		vSerInitMessage();
		V_PRINT("---"LB);
		V_PRINT("TWELITE NET lib version Core: %08x, Ext: %08x, Utils: %08x"LB,
				ToCoNet_u32GetVersion(),
				ToCoNet_u32GetVersion_LibEx(),
				ToCoNet_u32GetVersion_LibUtils());
		V_PRINT("TWELITE NET Tick Counter: %d"LB, u32TickCount_ms);
		V_PRINT("Run Time: %d+%02d/64 sec"LB, sAppData.u32CtTimer0 >> 6, sAppData.u32CtTimer0 & 0x3F);
		V_PRINT(""LB);
		break;

#ifdef USE_I2C_LCD_TEST_CODE
	case '1':
	case '2':
	case '3':
	case '4':
		_C {
			bool_t bRes;
#if defined(USE_I2C_AQM1602)
			bRes = bDraw2LinesLcd_ACM1602(astrLcdMsgs[u8Byte-'1'][0], astrLcdMsgs[u8Byte-'1'][1]);
#elif defined(USE_I2C_AQM0802A)
			bRes = bDraw2LinesLcd_AQM0802A(astrLcdMsgs[u8Byte-'1'][0], astrLcdMsgs[u8Byte-'1'][1]);
#endif
			V_PRINT("I2C LCD = %d: %s,%s"LB, bRes, astrLcdMsgs[u8Byte-'1'][0], astrLcdMsgs[u8Byte-'1'][1]);
		}

		break;
#endif

	case ASC_NAK: // XMODEM プロトコル NAK 処理
		_C {
			// XMODEM の開始（または、ブロックの再送要求）
			tsFlash sFlash = sAppData.sFlash;
			Config_vSyncUnsaved(&sFlash, &sAppData.sConfig_UnSaved);
			bFlash_DataRecalcHeader(&sFlash);
			Config_vOutputXmodem(&sFlash);
		}
		break;

	case ASC_ACK: // XMODEM プロトコル ACK 処理
		_C {
			// ブロックが残っていれば次のブロック転送を開始する。
			// 全ブロック転送すれば転送完了
			// (今回の実装では１ブロックのみの転送なので、EOTのみを送る)

			// XMODEM転送終了
			V_PUTCHAR(ASC_EOT);
		}
		break;

	default:
		u8lastbyte = 0xFF;
		break;
	}

	// 一つ前の入力
	if (u8lastbyte != 0xFF) {
		u8lastbyte = u8Byte;
	}
}

#if 0
/** @ingroup MASTER
 * １バイト入力コマンドの処理
 * @param u8Byte 入力バイト
 */
void vProcessInputByte(uint8 u8Byte) {
	static uint8 u8lastbyte = 0xFF;
	bool_t bInhibitUpdate = TRUE;

	switch (u8Byte) {
	case 0x0d: // LF
	case 0x0c: // Ctrl+L
		// 画面の書き換え
		u16HoldUpdateScreen = 1;
		bInhibitUpdate = FALSE;
		break;

	case 'a': // set application ID
		V_PRINT("Input Application ID (HEX:32bit): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8, E_APPCONF_APPID);
		break;

	case 'c': // チャネルの設定
		V_PRINT("Input Channel(s) (e.g. 11,16,21): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 8, E_APPCONF_CHMASK);
		break;

	case 'x': // チャネルの設定
		V_PRINT("Rf Power & Kbps"
				LB "   YZ Y=Retry(0:default,F:0,1-9:count"
				LB "      Z=Power(3:Max,2,1,0:Min)"
				LB "Input: "
		);
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 4, E_APPCONF_POWER);
		break;

	case 'i': // set application role
		V_PRINT("Input Device ID (0:parent, 1-100: child): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 3, E_APPCONF_ID);
		break;

	case 'r': // システムの役割定義
		V_PRINT("Input Role ID (HEX): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 2, E_APPCONF_ROLE);
		break;

	case 'l': // システムの役割定義
		V_PRINT("Input Layer (1-63): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 2, E_APPCONF_LAYER);
		break;

	case 'b': // ボーレートの変更
		V_PRINT("UART baud" LB "  !NOTE: only effective when BPS=Lo" LB);
		V_PRINT("Input (DEC:9600-230400): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 10, E_APPCONF_BAUD_SAFE);
		break;

	case 'B': // パリティの変更
		V_PRINT("UART options" LB "  !NOTE: only effective when BPS=Lo" LB);
		V_PRINT("Input (e.g. 8N1): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 3, E_APPCONF_BAUD_PARITY);
		break;

	case 'm': // モードの変更
		V_PRINT("UART mode"LB);
		V_PRINT("  A: ASCII, B: Binary formatted"LB);
		V_PRINT("  C: Chat (TXonCR), D: Chat (TXonPAUSE, no prompt)"LB);
		V_PRINT("  T: Transparent"LB);
		V_PRINT("Input: ");

		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 1, E_APPCONF_UART_MODE);
		break;

	case 'k':
		V_PRINT("Tx Triggger in Transparent,Chat no prompt mode"LB);
		V_PRINT("  {delim(HEX)},{minumum size(1-80)},{time out[ms](10-200)}"LB);
		V_PRINT("  e.g. 20,8,100 (space, 8bytes incl delim, 100ms)"LB);
		V_PRINT("Input: ");

		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 16, E_APPCONF_UART_LINE_SEP);
		break;

	case 'h':
		V_PRINT("Input Handle Name (Chat mode): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, FLASH_APP_HANDLE_NAME_LEN, E_APPCONF_HANDLE_NAME);
		break;

	case 'C':
		V_PRINT("Input crypt mode (0,1): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 2, E_APPCONF_CRYPT_MODE);
		break;

	case 'K':
		V_PRINT("Input crypt key: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, FLASH_APP_AES_KEY_SIZE, E_APPCONF_CRYPT_KEY);
		break;

	case 'o':
		V_PRINT("Input option bits: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8, E_APPCONF_OPT_BITS);
		break;

	case 'S':
		// フラッシュへのデータ保存
		if (u8lastbyte == 'R') {
			// R S と連続入力された場合は、フラッシュエリアを消去する
			V_PRINT("!INF CLEAR SAVE AREA.");
			bFlash_Erase(FLASH_SECTOR_NUMBER - 1); // SECTOR ERASE

			V_PRINT("!INF RESET SYSTEM." LB);
			vWait(1000000);
			vAHI_SwReset();
		} else {
			// フラッシュへのデータ保存
			vConfig_SaveAndReset();
		}
		break;

	case 'R':
		_C {
			vConfig_SetDefaults(&sAppData.sConfig_UnSaved);
			u16HoldUpdateScreen = 1;
		}
		break;

	case '$':
		_C {
			sAppData.u8DebugLevel++;
			if(sAppData.u8DebugLevel > 5) sAppData.u8DebugLevel = 0;

			V_PRINT("* set App debug level to %d." LB, sAppData.u8DebugLevel);
		}
		break;

	case '@':
		_C {
			static uint8 u8DgbLvl;

			u8DgbLvl++;
			if(u8DgbLvl > 5) u8DgbLvl = 0;
			ToCoNet_vDebugLevel(u8DgbLvl);

			V_PRINT("* set NwkCode debug level to %d." LB, u8DgbLvl);
		}
		break;

	case '!':
		// リセット
		V_PRINT("!INF RESET SYSTEM." LB);
		vWait(1000000);
		vAHI_SwReset();
		break;

	case '#': // info
		_C {
			V_PRINT("*** TWELITE NET(ver%08X) ***" LB, ToCoNet_u32GetVersion());
			V_PRINT("* AppID %08x, LongAddr, %08x, ShortAddr %04x, Tk: %d" LB,
					sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress, u32TickCount_ms);
			if (sAppData.bFlashLoaded) {
				V_PRINT("** Conf "LB);
				V_PRINT("* AppId = %08x"LB, sAppData.sFlash.sData.u32appid);
				V_PRINT("* ChMsk = %08x"LB, sAppData.sFlash.sData.u32chmask);
				V_PRINT("* Ch=%d, Role=%d, Layer=%d"LB,
						sToCoNet_AppContext.u8Channel,
						sAppData.sFlash.sData.u8role,
						sAppData.sFlash.sData.u8layer);

				V_PRINT(LB "Nwk mode: %02x", sAppData.eNwkMode);
				if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
					tsToCoNet_NwkLyTr_Context *pc = (tsToCoNet_NwkLyTr_Context *)(sAppData.pContextNwk);

					V_PRINT(LB "Nwk Info: la=%d ty=%d ro=%02x st=%02x",
							pc->sInfo.u8Layer, pc->sInfo.u8NwkTypeId, pc->sInfo.u8Role, pc->sInfo.u8State);
					V_PRINT(LB "Nwk Parent: %08x", pc->u32AddrHigherLayer);
					V_PRINT(LB "Nwk LostParent: %d", pc->u8Ct_LostParent);
					V_PRINT(LB "Nwk SecRescan: %d, SecRelocate: %d", pc->u8Ct_Second_To_Rescan, pc->u8Ct_Second_To_Relocate);
				}
			} else {
				V_PRINT("** Conf: none"LB);
			}
		}
		break;

	case ASC_NAK: // XMODEM プロトコル NAK 処理
		_C {
			// XMODEM の開始（または、ブロックの再送要求）
			tsFlash sFlash = sAppData.sFlash;
			vConfig_SyncUnsaved(&sFlash, &sAppData.sConfig_UnSaved);
			bFlash_DataRecalcHeader(&sFlash);
			vConfig_OutputXmodem(&sFlash);
		}
		break;

	case ASC_ACK: // XMODEM プロトコル ACK 処理
		_C {
			// ブロックが残っていれば次のブロック転送を開始する。
			// 全ブロック転送すれば転送完了
			// (今回の実装では１ブロックのみの転送なので、EOTのみを送る)

			// XMODEM転送終了
			V_PUTCHAR(ASC_EOT);
		}
		break;


	default:
		bInhibitUpdate = FALSE;
		break;
	}

	u8lastbyte = u8Byte;

	if (bInhibitUpdate) {
		u16HoldUpdateScreen = 0;
	}
}
#endif

/** @ingroup MASTER
 * 文字列入力モードの処理を行います。
 *
 */
void vProcessInputString(tsInpStr_Context *pContext) {
	uint8 *pu8str = pContext->au8Data;
	uint8 u8idx = pContext->u8Idx;

	switch(pContext->u32Opt) {
	case E_APPCONF_APPID:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);

			uint16 u16h, u16l;
			u16h = u32val >> 16;
			u16l = u32val & 0xFFFF;

			if (u16h == 0x0000 || u16h == 0xFFFF || u16l == 0x0000 || u16l == 0xFFFF) {
				V_PRINT("(ignored: 0x0000????,0xFFFF????,0x????0000,0x????FFFF can't be set.)");
			} else {
				sAppData.sConfig_UnSaved.u32appid = u32val;
			}

			V_PRINT(LB"-> %08X"LB, u32val);
		}
		break;

	case E_APPCONF_CHMASK:
		_C {
			// チャネルマスク（リスト）を入力解釈する。
			//  11,15,19 のように１０進数を区切って入力する。
			//  以下では区切り文字は任意で MAX_CHANNELS 分処理したら終了する。

			uint8 b = 0, e = 0, i = 0, n_ch = 0;
			uint32 u32chmask = 0; // 新しいチャネルマスク

			V_PRINT(LB"-> ");

			for (i = 0; i <= pContext->u8Idx; i++) {
				if (pu8str[i] < '0' || pu8str[i] > '9') {
					e = i;
					uint8 u8ch = 0;

					// 最低２文字あるときに処理
					if (e - b > 0) {
						u8ch = u32string2dec(&pu8str[b], e - b);
						if (u8ch >= 11 && u8ch <= 26) {
							if (n_ch) {
								V_PUTCHAR(',');
							}
							V_PRINT("%d", u8ch);
							u32chmask |= (1UL << u8ch);

							n_ch++;
							if (n_ch >= MAX_CHANNELS) {
								break;
							}
						}
					}
					b = i + 1;
				}

				if (pu8str[i] == 0x0) {
					break;
				}
			}

			if (u32chmask == 0x0) {
				V_PRINT("(ignored)");
			} else {
				sAppData.sConfig_UnSaved.u32chmask = u32chmask;
			}

			V_PRINT(LB);
		}
		break;

	case E_APPCONF_ID:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);
			V_PRINT(LB"-> ");
			if (u32val <= 0x7F) {
				sAppData.sConfig_UnSaved.u8id = u32val;
				V_PRINT("%d(0x%02x)"LB, u32val, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_TX_POWER:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);
			V_PRINT(LB"-> ");
			if ((u32val & 0xF) <= 3) {
				sAppData.sConfig_UnSaved.u8pow = u32val;
				V_PRINT("%02x"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_SLEEP4:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);
			V_PRINT(LB"-> ");
			if (u32val >= 100 && u32val <= 65534) {
				sAppData.sConfig_UnSaved.u16SleepDur_ms = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_SLEEP7:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINT(LB"-> ");
			if (u32val <= 65534) { // 0 はタイマーを使用しない (
				sAppData.sConfig_UnSaved.u16SleepDur_s = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_FPS:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINT(LB"-> ");
			if (u32val == 4 || u32val == 8 || u32val == 16 || u32val == 32) {
				sAppData.sConfig_UnSaved.u8Fps = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_OPT:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);

			V_PRINT(LB"-> ");

			sAppData.sConfig_UnSaved.u32Opt = u32val;
			V_PRINT("0x%08X"LB, u32val);
		}
		break;

	case E_APPCONF_BAUD_SAFE:
		_C {
			uint32 u32val = 0;

			if (pu8str[0] == '0' && pu8str[1] == 'x') {
				u32val = u32string2hex(pu8str + 2, u8idx - 2);
			} if (u8idx <= 6) {
				u32val = u32string2dec(pu8str, u8idx);
			}

			V_PRINT(LB"-> ");

			if (u32val) {
				sAppData.sConfig_UnSaved.u32baud_safe = u32val;
				if (u32val & 0x80000000) {
					V_PRINT("%x"LB, u32val);
				} else {
					V_PRINT("%d"LB, u32val);
				}
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_BAUD_PARITY:
		_C {
			V_PRINT(LB"-> ");

			if (pu8str[0] == 'N' || pu8str[0] == 'n') {
				sAppData.sConfig_UnSaved.u8parity = 0;
				V_PRINT("None"LB);
			} else if (pu8str[0] == 'O' || pu8str[0] == 'o') {
				sAppData.sConfig_UnSaved.u8parity = 1;
				V_PRINT("Odd"LB);
			} else if (pu8str[0] == 'E' || pu8str[0] == 'e') {
				sAppData.sConfig_UnSaved.u8parity = 2;
				V_PRINT("Even"LB);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_CRYPT_MODE:
		_C {
			if (pu8str[0] == '0') {
				sAppData.sConfig_UnSaved.u8Crypt = 0;
				V_PRINT(LB"--> Plain");
			} else if (pu8str[0] == '1') {
				sAppData.sConfig_UnSaved.u8Crypt = 1;
				V_PRINT(LB"--> AES128");
			} else {
				V_PRINT(LB"(ignored)");
			}
		}
		break;

	case E_APPCONF_CRYPT_KEY:
		_C {
			uint8 u8len = strlen((void*)pu8str);

			if (u8len == 0) {
				memset(sAppData.sConfig_UnSaved.au8AesKey, 0, sizeof(sAppData.sConfig_UnSaved.au8AesKey));
				V_PRINT(LB"(cleared)");
			} else
			if (u8len && u8len <= 32) {
				memset(sAppData.sConfig_UnSaved.au8AesKey, 0, sizeof(sAppData.sConfig_UnSaved.au8AesKey));
				memcpy(sAppData.sConfig_UnSaved.au8AesKey, pu8str, u8len);
				V_PRINT(LB);
			} else {
				V_PRINT(LB"(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_HOLD_MASK:
		_C {
			uint8 u8len = strlen((void*)pu8str);
			uint32 u32mask = 0;
			int i = 0;

			for (i = 0; i < u8len && i < MAX_IO_TBL; i++) {
				if (pu8str[u8len - i - 1] == '1') {
					u32mask |= (1UL << i);
				}
			}

			sAppData.sConfig_UnSaved.u32HoldMask = u32mask;
			V_PRINT(LB "-->%012b", u32mask);
		}
		break;

	case E_APPCONF_HOLD_DUR:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINT(LB"-> ");
			if (u32val >= 20 && u32val <= 65534) { // 0 はタイマーを使用しない (
				sAppData.sConfig_UnSaved.u16HoldDur_ms = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	default:
		break;
	}

	// 一定時間待って画面を再描画
	u16HoldUpdateScreen = 96; // 1.5sec
}

/** @ingroup MASTER
 * インタラクティブモードの画面を再描画する。
 * - 本関数は TIMER_0 のイベント処理時に u16HoldUpdateScreen カウンタがデクリメントされ、
 *   これが0になった時に呼び出される。
 *
 * - 設定内容、設定値、未セーブマーク*を出力している。FL_... のマクロ記述を参照。
 *
 */
void vSerUpdateScreen() {
	V_PRINT("\e[2J\e[H"); // CLEAR SCREEN
	V_PRINT("--- CONFIG/"APP_NAME" V%d-%02d-%d/SID=0x%08x/LID=0x%02x %c%c ---"LB,
			VERSION_MAIN, VERSION_SUB, VERSION_VAR,
			ToCoNet_u32GetSerial(),
			sAppData.u8AppLogicalId,
			sAppData.bCustomDefaults ? 'C' : '-',
			sAppData.bFlashLoaded ? 'E' : '-'
	);

	// Application ID
	V_PRINT(" a: set Application ID (0x%08x)%c" LB,
			FL_IS_MODIFIED_u32(appid) ? FL_UNSAVE_u32(appid) : FL_MASTER_u32(appid),
			FL_IS_MODIFIED_u32(appid) ? '*' : ' ');

	// Device ID
	{
		uint8 u8DevID = FL_IS_MODIFIED_u8(id) ? FL_UNSAVE_u8(id) : FL_MASTER_u8(id);

		if (u8DevID == 0x00) { // unset
			V_PRINT(" i: set Device ID (--)%c"LB,
					FL_IS_MODIFIED_u8(id) ? '*' : ' '
					);
		} else {
			V_PRINT(" i: set Device ID (%d=0x%02x)%c"LB,
					u8DevID, u8DevID,
					FL_IS_MODIFIED_u8(id) ? '*' : ' '
					);
		}
	}

	V_PRINT(" c: set Channels (");
	{
		// find channels in ch_mask
		uint32 u32mask = FL_IS_MODIFIED_u32(chmask) ? FL_UNSAVE_u32(chmask) : FL_MASTER_u32(chmask);
		vSerPrintChannelMask(u32mask);
	}
	V_PRINT(")%c" LB,
			FL_IS_MODIFIED_u32(chmask) ? '*' : ' ');

	V_PRINT(" x: set Tx Power (%02x)%c" LB,
			FL_IS_MODIFIED_u8(pow) ? FL_UNSAVE_u8(pow) : FL_MASTER_u8(pow),
			FL_IS_MODIFIED_u8(pow) ? '*' : ' ');

	V_PRINT(" t: set mode4 sleep dur (%dms)%c" LB,
			FL_IS_MODIFIED_u16(SleepDur_ms) ? FL_UNSAVE_u16(SleepDur_ms) : FL_MASTER_u16(SleepDur_ms),
			FL_IS_MODIFIED_u16(SleepDur_ms) ? '*' : ' ');

	V_PRINT(" y: set mode7 sleep dur (%ds)%c" LB,
			FL_IS_MODIFIED_u16(SleepDur_s) ? FL_UNSAVE_u16(SleepDur_s) : FL_MASTER_u16(SleepDur_s),
			FL_IS_MODIFIED_u16(SleepDur_s) ? '*' : ' ');

	V_PRINT(" f: set mode3 fps (%d)%c" LB,
			FL_IS_MODIFIED_u8(Fps) ? FL_UNSAVE_u8(Fps) : FL_MASTER_u8(Fps),
			FL_IS_MODIFIED_u8(Fps) ? '*' : ' ');

	V_PRINT(" d: set hold mask (%012b)%c" LB,
			FL_IS_MODIFIED_u32(HoldMask) ? FL_UNSAVE_u32(HoldMask) : FL_MASTER_u32(HoldMask),
			FL_IS_MODIFIED_u32(HoldMask) ? '*' : ' ');

	V_PRINT(" D: set hold dur (%dms)%c" LB,
			FL_IS_MODIFIED_u16(HoldDur_ms) ? FL_UNSAVE_u16(HoldDur_ms) : FL_MASTER_u16(HoldDur_ms),
			FL_IS_MODIFIED_u16(HoldDur_ms) ? '*' : ' ');

	V_PRINT(" o: set Option Bits (0x%08X)%c" LB,
			FL_IS_MODIFIED_u32(Opt) ? FL_UNSAVE_u32(Opt) : FL_MASTER_u32(Opt),
			FL_IS_MODIFIED_u32(Opt) ? '*' : ' ');

	{
		uint32 u32baud = FL_IS_MODIFIED_u32(baud_safe) ? FL_UNSAVE_u32(baud_safe) : FL_MASTER_u32(baud_safe);
		if (u32baud & 0x80000000) {
			V_PRINT(" b: set UART baud (%x)%c" LB, u32baud,
					FL_IS_MODIFIED_u32(baud_safe) ? '*' : ' ');
		} else {
			V_PRINT(" b: set UART baud (%d)%c" LB, u32baud,
					FL_IS_MODIFIED_u32(baud_safe) ? '*' : ' ');
		}
	}

	{
		const uint8 au8name[] = { 'N', 'O', 'E' };
		V_PRINT(" p: set UART parity (%c)%c" LB,
					au8name[FL_IS_MODIFIED_u8(parity) ? FL_UNSAVE_u8(parity) : FL_MASTER_u8(parity)],
					FL_IS_MODIFIED_u8(parity) ? '*' : ' ');
	}

	{
		V_PRINT(" C: set crypt mode (%d)%c" LB,
					FL_IS_MODIFIED_u8(Crypt) ? FL_UNSAVE_u8(Crypt) : FL_MASTER_u8(Crypt),
					FL_IS_MODIFIED_u8(Crypt) ? '*' : ' ');
	}

	{
		V_PRINT(" K: set crypt key [%s]%c" LB,
			sAppData.sConfig_UnSaved.au8AesKey[0] == 0xFF ?
				sAppData.sFlash.sData.au8AesKey : sAppData.sConfig_UnSaved.au8AesKey,
			sAppData.sConfig_UnSaved.au8AesKey[0] == 0xFF ? ' ' : '*');
	}

	V_PRINT("---"LB);

	V_PRINT(" S: save Configuration" LB " R: reset to Defaults" LB LB);
	//       0123456789+123456789+123456789+1234567894123456789+123456789+123456789+123456789
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
bool_t Config_bSetModuleParam(uint8 *p, uint8 u8len) {
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
		if (u8len >= 1) FL_UNSAVE_u8(id)= G_OCTET(); else bRet = FALSE;
		break;

	default:
		bRet = FALSE;
		break;
	}

	return bRet;
}
