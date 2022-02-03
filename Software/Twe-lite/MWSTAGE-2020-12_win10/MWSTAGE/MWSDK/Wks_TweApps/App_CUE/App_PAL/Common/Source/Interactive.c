/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>
#include <stdlib.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "appdata.h"

#include "utils.h"
#include "flash.h"

#include "common.h"

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "Interactive.h"

#include "config.h"

#include "sercmd_gen.h"
#include "sercmd_plus3.h"

/****************************************************************************/
/***        MACROs                                                        ***/
/****************************************************************************/
#define MAX_CHANNELS 3

/****************************************************************************/
/***        Imported variables/functions                                  ***/
/****************************************************************************/
extern tsFILE sSerStream;
extern tsSerialPortSetup sSerPort;

//tsModbusCmd sSerCmd; //!< シリアル入力系列のパーサー (modbus もどき)  @ingroup MASTER
tsInpStr_Context sSerInpStr; //!< 文字列入力  @ingroup MASTER
uint16 u16HoldUpdateScreen = 0; //!< スクリーンアップデートを行う遅延カウンタ  @ingroup MASTER

tsSerCmdPlus3_Context sSerCmd_P3; //!< シリアル入力系列のパーサー (+ + +)  @ingroup MASTER
static tsSerCmd_Context sSerCmd; //!< シリアル入力系列のパーサー   @ingroup MASTER

uint8 au8SerBuffInput[128]; //!< 入力用のシリアルバッファ

static tsFlashApp sConfig_UnSaved; //!< 未セーブ状態の設定情報

static bool_t bInitInteractive = FALSE;

extern void vSerInitMessage();
extern void vProcessSerialCmd(tsSerCmd_Context *);

static void vSerPrintUartOpt(uint8 u8conf);

static void vProcessInputByte_Command(int16 i16Char);
static void vProcessInputByte(uint8 u8Byte);
static void vProcessInputString(tsInpStr_Context *pContext);

static void vConfig_Update(tsFlashApp *pFlash);

static void vConfig_UnSetAll(tsFlashApp *p);
static void Config_vSetDefaults(tsFlashApp *p);
static void vSerUpdateScreen();

void vProcessSerialCmd();

/****************************************************************************/
/***        Procedures                                                    ***/
/****************************************************************************/

/**
 * インタラクティブモードの初期化
 */
void Interactive_vInit() {
	vConfig_UnSetAll(&sConfig_UnSaved);

	INPSTR_vInit(&sSerInpStr, &sSerStream);

	static uint8 au8SerialBuffCmd[256];
	sSerCmd.au8data = au8SerialBuffCmd;
	sSerCmd.u16maxlen = sizeof(au8SerialBuffCmd);

	// シリアルコマンド処理関数
	memset(&sSerCmd_P3, 0x00, sizeof(sSerCmd_P3));
	memset(&sSerCmd, 0x00, sizeof(sSerCmd));

	// 入力コマンド形式の設定
	if (IS_APPCONF_OPT_UART_BIN()) {
		SerCmdBinary_vInit(&sSerCmd, au8SerBuffInput, sizeof(au8SerBuffInput)); //!< バイナリモード
	} else {
		SerCmdAscii_vInit(&sSerCmd, au8SerBuffInput, sizeof(au8SerBuffInput)); //!< modbus ASCII
	}

	bInitInteractive = TRUE;
	u16HoldUpdateScreen = 100;
}


/**
 * インタラクティブモードの再初期化
 * (UART 初期化前に呼び出す)
 */
void Interactive_vReInit() {
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		(void)SERIAL_i16RxChar(sSerPort.u8SerialPort);
	}

	sSerCmd.u8state = E_SERCMD_EMPTY;
	sSerCmd_P3.u8state = E_SERCMD_EMPTY;
}

/**
 * VERBOSE モードの判定
 * @return TRUEならVERBOSEモード
 */
bool_t Interactive_bGetMode() {
	return sSerCmd_P3.bverbose;
}

/**
 * VERBOSE モードの設定
 * @param bVerbose TRUEならVERBOSEモードに設定する
 * @param u16screen_refresh 再表示までのタイマカウンタ(0なら設定しない、1カウント約16ms)
 */
void Interactive_vSetMode(bool_t bVerbose, uint16 u16screen_refresh) {
	if (!bInitInteractive) return;

	sSerCmd_P3.bverbose = bVerbose;
	if (u16screen_refresh) {
		u16HoldUpdateScreen = u16screen_refresh;
	}
}

/**
 * 書式コマンドの解釈
 * @param i16Char
 */
static void vProcessInputByte_Command(int16 i16Char) {
	// 無効処理
	if (i16Char < 0 || i16Char > 0xFF || sSerCmd.u8Parse == NULL) return;

	// verbose モードならタイムアウトは無効
	if (sSerCmd_P3.bverbose) {
		sSerCmd.u32timestamp = u32TickCount_ms;
	}

	// コマンド書式の系列解釈
	uint8 u8res = sSerCmd.u8Parse(&sSerCmd, (uint8)i16Char);

	// 完了判定
	if (u8res == E_SERCMD_COMPLETE || u8res == E_SERCMD_CHECKSUM_ERROR) {
		// 解釈完了

		if (u8res == E_SERCMD_CHECKSUM_ERROR) {
			// command complete, but CRC error
			V_PRINTF(LB "!INF CHSUM_ERR? (might be %02X)" LB, sSerCmd.u16cksum);
		}

		if (u8res == E_SERCMD_COMPLETE) {
			// process command
			vProcessSerialCmd(&sSerCmd);
		}
	} else {
		if (u8res != E_SERCMD_EMPTY) {
			if (sSerCmd.u16pos == 0) {
				V_PRINTF(LB);
			}
			// エコーバック
			V_PUTCHAR(i16Char);
		}
	}
}

/** @ingroup INTERACTIVE
 * インタラクティブモードの画面更新を強制します。
 */
void Config_vUpdateScreen() {
	u16HoldUpdateScreen = 0;
	if (sSerCmd_P3.bverbose) {
		vSerUpdateScreen();
	}
}

/** @ingroup INTERACTIVE
 * シリアルポートからの入力を処理します。
 * - シリアルポートからの入力は uart.c/serial.c により管理される FIFO キューに値が格納されます。
 *   このキューから１バイト値を得るのが SERIAL_i16RxChar() です。
 * - 本関数では、入力したバイトに対し、アプリケーションのモードに依存した処理を行います。
 *   - 文字列入力モード時(INPSTR_ API 群、インタラクティブモードの設定値入力中)は、INPSTR_u8InputByte()
 *     API に渡す。文字列が完了したときは vProcessInputString() を呼び出し、設定値の入力処理を
 *     行います。
 *   - 上記文字列入力ではない場合は、ModBusAscii_u8Parse() を呼び出します。この関数は + + + の
 *     入力判定および : で始まる書式を認識します。
 *   - 上記書式解釈中でない場合は、vProcessInputByte() を呼び出します。この関数はインタラクティブ
 *     モードにおける１文字入力コマンドを処理します。
 *
 */
void vHandleSerialInput() {
	static uint32 u32last_tick;

	// カウンタ値のチェック
	if (u32TickCount_ms - u32last_tick >= 16) {
		if (u16HoldUpdateScreen) {
			u16HoldUpdateScreen--;
			if (!u16HoldUpdateScreen && sSerCmd_P3.bverbose) {
				vSerUpdateScreen();
			}
		}
		u32last_tick = u32TickCount_ms;
	}

	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		// UART バッファからのバイトの取り出し
		int16 i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);


		// process
		if (i16Char >=0 && i16Char <= 0xFF) {
			//DBGOUT(0, "[%02x]", i16Char);
			if (INPSTR_bActive(&sSerInpStr)) {
				// 文字列入力モード
				uint8 u8res = INPSTR_u8InputByte(&sSerInpStr, (uint8)i16Char);

				if (u8res == E_INPUTSTRING_STATE_COMPLETE) {
					vProcessInputString(&sSerInpStr);
				} else if (u8res == E_INPUTSTRING_STATE_CANCELED) {
					V_PRINTF("(canceled)");
					u16HoldUpdateScreen = 64;
				}
				continue;
			}

#if 0
			{
				// コマンド書式の系列解釈、および verbose モードの判定
				uint8 u8res;
				u8res = SerCmdPlus3_u8Parse(&sSerCmd_P3, (uint8)i16Char);

				if (u8res != E_SERCMD_PLUS3_EMPTY) {
					if (u8res == E_SERCMD_PLUS3_VERBOSE_ON) {
						// verbose モードの判定があった
						if (bInitInteractive) {
							vSerUpdateScreen();
							sSerCmd.u16timeout = 0;
						}
					}

					if (u8res == E_SERCMD_PLUS3_VERBOSE_OFF) {
						if (bInitInteractive) {
							vfPrintf(&sSerStream, "!INF EXIT INTERACTIVE MODE."LB);
							sSerCmd.u16timeout = 1000;
						}
					}

					// still waiting for bytes.
					//continue;
				} else {
					; // コマンド解釈モードではない
				}
			}
#endif
			// Verbose モードのときは、シングルコマンドを取り扱う
			if (sSerCmd_P3.bverbose) {
				// コマンドの解釈
				vProcessInputByte_Command(i16Char);

				if (sSerCmd.u8state == E_SERCMD_EMPTY) {
					// 書式入出力でなければ、１バイトコマンド
					vProcessInputByte(i16Char);
				}
			} else {
				vProcessInputByte_Command(i16Char);
			}
		}
	}
}

/** @ingroup MASTER
 * １バイト入力コマンドの処理\n
 * - 設定値の入力が必要な項目の場合、INPSTR_vStart() を呼び出して文字列入力モードに遷移します。
 * - フラッシュへのセーブ時の手続きでは、sConfig_UnSaved 構造体で入力が有ったものを
 *   sFlash.sData 構造体に格納しています。
 * - デバッグ用の確認コマンドも存在します。
 *
 * @param u8Byte 入力バイト
 */
static void vProcessInputByte(uint8 u8Byte) {
	static uint8 u8lastbyte;

	switch (u8Byte) {
	case 0x0d:
	case 'h':
	case 'H':
		// 画面の書き換え
		u16HoldUpdateScreen = 1;
		break;

	case 'a': // set application ID
		V_PRINTF("Input Application ID (HEX:32bit): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8,
				E_APPCONF_APPID);
		break;

	case 'c': // チャネルの設定
		V_PRINTF("Input Channel: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 8,
				E_APPCONF_CHMASK);
		break;

	case 'i': // set application role
		V_PRINTF("Input Device ID (DEC:1-100): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 3, E_APPCONF_ID);
		break;

	case 'x': // 出力の変更
		V_PRINTF("Retry & Rf Power"
				LB "   YZ Y=Retry(0-9:count)"
				LB "      Z=Power(3:Max,2,1,0:Min)"
				LB "Input: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 2, E_APPCONF_TX_POWER);
		break;

	case 't': // スリープ周期
		V_PRINTF("Input Transmission Interval[sec]: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 8,
				E_APPCONF_SLEEP_DUR);
		break;

	case 'p': // センサのパラメータ
		V_PRINTF("Input Sensor Parameter : ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8,
				E_APPCONF_SER_PARAM);
		break;

	case 'o': // オプションビットの設定
		V_PRINTF("Input option bits (HEX): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8,
				E_APPCONF_OPT);
		break;

	case 'b': // ボーレートの変更
		V_PRINTF("Input UART baud (DEC:9600-230400): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 10, E_APPCONF_BAUD_SAFE);
		break;

	case 'B': // パリティの変更
		V_PRINTF("Input UART option (e.g. 8N1): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 3, E_APPCONF_BAUD_PARITY);
		break;

	case 'k': // オプションビットの設定
		V_PRINTF("Input Encription Key (HEX 32bit): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_HEX, 8,
				E_APPCONF_ENC_KEY);
		break;

#ifndef USE_CUE
	case 'e':
		V_PRINTF("Notice PAL Action(s) Each Event");
		V_PRINTF(LB"    IIRGBWPT");
		V_PRINTF(LB"        II : EventID (0x00-0x10)");
		V_PRINTF(LB"         R : RED     (0x0-0xF)");
		V_PRINTF(LB"         G : GREEN   (0x00-0xF)");
		V_PRINTF(LB"         B : BLUE    (0x00-0xF)");
		V_PRINTF(LB"         W : WHITE   (0x00-0xF)");
		V_PRINTF(LB"         P : Blink Pattern (0x0-0x3)");
		V_PRINTF(LB"         T : Time to turn off(Sec) (0x0-0xF)");
		V_PRINTF(LB"(e.g. 000000000140701A): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 136, E_APPCONF_EVENT);
		break;
#endif

	case 'S':
		// フラッシュへのデータ保存
		if (u8lastbyte == 'R') {
			// R S と連続入力された場合は、フラッシュエリアを消去する
			V_PRINTF("!INF CLEAR SAVE AREA.");
			bFlash_Erase(0); // SECTOR ERASE
		} else {
			bool_t bRet = Config_bSave();
			V_PRINTF("!INF FlashWrite %s"LB, bRet ? "Success" : "Failed");
		}

		V_PRINTF("!INF RESET SYSTEM...");
		vWait(1000000);
		vAHI_SwReset();
		break;

	case 'R':
		Config_vSetDefaults(&sConfig_UnSaved);
		u16HoldUpdateScreen = 1;
		break;

	case '$':
		sAppData.u8DebugLevel++;
		if (sAppData.u8DebugLevel > 5)
			sAppData.u8DebugLevel = 0;

		V_PRINTF("* set App debug level to %d." LB, sAppData.u8DebugLevel);
		break;

	case '@':
		_C {
			static uint8 u8DgbLvl;

			u8DgbLvl++;
			if (u8DgbLvl > 5)
				u8DgbLvl = 0;
			ToCoNet_vDebugLevel(u8DgbLvl);

			V_PRINTF("* set NwkCode debug level to %d." LB, u8DgbLvl);
		}
		break;

	case '!':
		// リセット
		vResetWithMsg(&sSerStream, "!INF RESET SYSTEM.");
		break;

	case '#': // info
		_C {
			V_PRINTF("*** TWELITE NET(ver%08X) ***" LB, ToCoNet_u32GetVersion());
			V_PRINTF("* AppID %08x, LongAddr, %08x, ShortAddr %04x, Tk: %d" LB,
					sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(),
					sToCoNet_AppContext.u16ShortAddress, u32TickCount_ms);
			V_PRINTF("* Ch=%d"LB,
					sToCoNet_AppContext.u8Channel);
			if (sAppData.bFlashLoaded) {
				V_PRINTF("** Conf "LB);
				V_PRINTF("* AppId = %08x"LB, sAppData.sFlash.sData.u32appid);
				V_PRINTF("* ChMsk = %08x"LB, sAppData.sFlash.sData.u32chmask);
			} else {
				V_PRINTF("** Conf: none"LB);
			}
		}
		break;

	case 'V':
		vSerInitMessage();
		V_PRINTF("---"LB);
		V_PRINTF("TWELITE NET lib version Core: %08x, Ext: %08x, Utils: %08x"LB,
				ToCoNet_u32GetVersion(), ToCoNet_u32GetVersion_LibEx(),
				ToCoNet_u32GetVersion_LibUtils());
		V_PRINTF("TWELITE NET Tick Counter: %d"LB, u32TickCount_ms);
		V_PRINTF(""LB);
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

/** @ingroup MASTER
 * 文字列入力モードの処理を行います。
 *
 */
static void vProcessInputString(tsInpStr_Context *pContext) {
	uint8 *pu8str = pContext->au8Data;
	uint8 u8idx = pContext->u8Idx;

	switch (pContext->u32Opt) {
	case E_APPCONF_APPID:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);

			uint16 u16h, u16l;
			u16h = u32val >> 16;
			u16l = u32val & 0xFFFF;

			if (u16h == 0x0000 || u16h == 0xFFFF || u16l == 0x0000
					|| u16l == 0xFFFF) {
				V_PRINTF(
						"(ignored: 0x0000????,0xFFFF????,0x????0000,0x????FFFF can't be set.)");
			} else {
				sConfig_UnSaved.u32appid = u32val;
			}

			V_PRINTF(LB"-> %08X"LB, u32val);
		}
		break;

	case E_APPCONF_CHMASK:
		_C {
			// チャネルマスク（リスト）を入力解釈する。
			//  11,15,19 のように１０進数を区切って入力する。
			//  以下では区切り文字は任意で MAX_CHANNELS 分処理したら終了する。

			uint8 b = 0, e = 0, i = 0, n_ch = 0;
			uint32 u32chmask = 0; // 新しいチャネルマスク

			V_PRINTF(LB"-> ");

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
							V_PRINTF("%d", u8ch);
							u32chmask |= (1UL << u8ch);

							sConfig_UnSaved.u8ch = u8ch; // CHANNEL

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
				V_PRINTF("(ignored)");
			} else {
				sConfig_UnSaved.u32chmask = u32chmask;
			}

			V_PRINTF(LB);
		}
		break;

	case E_APPCONF_ID:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);
			V_PRINTF(LB"-> ");
			if (u32val <= 0x7F) {
				sConfig_UnSaved.u8id = u32val;
				V_PRINTF("%d(0x%02x)"LB, u32val, u32val);
			} else {
				V_PRINTF("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_TX_POWER:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);
			uint8 u8Pow = (u32val&0x000F);
			V_PRINTF(LB"-> ");
			if (u8Pow <= 3) {
				sConfig_UnSaved.u8pow = u32val;
				V_PRINTF("%x"LB, u32val);
			} else {
				V_PRINTF("POW = %d"LB, u8Pow);
				V_PRINTF("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_SLEEP_DUR:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINTF(LB"-> ");

			if (u32val <= 160000000) {
				sConfig_UnSaved.u32Slp = u32val;
				V_PRINTF("%d"LB, u32val);
			} else {
				V_PRINTF("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_SER_PARAM:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);
			V_PRINTF(LB"-> ");
			sConfig_UnSaved.u32param = u32val;
			V_PRINTF("%08X"LB, u32val);
		}
		break;

	case E_APPCONF_OPT:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);

			V_PRINTF(LB"-> ");

			sConfig_UnSaved.u32Opt = u32val;
			V_PRINTF("0x%08X"LB, u32val);
		}
		break;

	case E_APPCONF_ENC_KEY:
		_C {
			uint32 u32val = u32string2hex(pu8str, u8idx);

			V_PRINTF(LB"-> ");

			sConfig_UnSaved.u32EncKey = u32val;
			V_PRINTF("0x%08X"LB, u32val);
		}
		break;

	case E_APPCONF_BAUD_SAFE:
		_C {
			uint32 u32val = 0;

			if (pu8str[0] == '0' && pu8str[1] == 'x') {
				u32val = u32string2hex(pu8str + 2, u8idx - 2);
			}
			if (u8idx <= 6) {
				u32val = u32string2dec(pu8str, u8idx);
			}

			V_PRINTF(LB"-> ");

			if (u32val) {
				sConfig_UnSaved.u32baud_safe = u32val;
				if (u32val & 0x80000000) {
					V_PRINTF("%x"LB, u32val);
				} else {
					V_PRINTF("%d"LB, u32val);
				}
			} else {
				V_PRINTF("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_BAUD_PARITY:
		_C {
			V_PRINTF(LB"-> ");
			uint8 u8len = strlen((void*)pu8str);
			int i;

			// 既存の設定値を保存する
			uint8 u8parity = sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_PARITY_MASK;
			uint8 u8stop = sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_STOPBIT_MASK;
			uint8 u8wordlen = sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_WORDLEN_MASK;

			// １文字ずつ処理する（順番は任意でもよい）
			for (i = 0; i < u8len; i++) {
				switch(pu8str[i]) {
				case 'N': case 'n': u8parity = 0; break;
				case 'O': case 'o': u8parity = 1; break;
				case 'E': case 'e': u8parity = 2; break;
				case '1': u8stop = 0; break;
				case '2': u8stop = APPCONF_UART_CONF_STOPBIT_MASK; break;
				case '8': u8wordlen = 0; break;
				case '7': u8wordlen = APPCONF_UART_CONF_WORDLEN_MASK; break;
				default:
					break;
				}
			}

			// 新しい設定を計算する
			uint8 u8new = u8parity | u8stop | u8wordlen;
			if (u8new == sAppData.sFlash.sData.u8parity) {
				// 変化が無ければ無視
				V_PRINTF("(ignored)");
			} else {
				sConfig_UnSaved.u8parity = u8new;
				vSerPrintUartOpt(u8new);
			}
		}
		break;

#ifndef USE_CUE
	case E_APPCONF_EVENT:
		_C{
			uint8 u8len = strlen((void*)pu8str);
			if( (u8len&0x07) != 0 ){
				// 8の倍数じゃなければ無視
				V_PRINTF("(ignored)");
			}else{
				memset( sConfig_UnSaved.au8Event, 0, 137 );
				memcpy( sConfig_UnSaved.au8Event, pu8str, u8len );
				sConfig_UnSaved.u8EventNum = u8len>>3;
			}

		}
		break;
#endif

	default:
		break;
	}

	// 一定時間待って画面を再描画
	u16HoldUpdateScreen = 96; // 1.5sec
}

/** @ingroup FLASH
 * フラッシュ構造体の読み出し
 * バージョン等のチェックを行い、問題が有れば FALSE を返す
 *
 * @param p
 * @return
 */
bool_t Config_bLoad(tsFlash *p) {
	// フラッシュの読み出し
	bool_t bRet = TRUE;
	bRet &= bFlash_Read(p, 0, 0);

	// Version String のチェック
	if (bRet && APP_ID == p->sData.u32appkey  && VERSION_U32 == p->sData.u32ver) {
		bRet = TRUE;
	} else {
		// チェックが違っているとデフォルト値を採用する
		Config_vSetDefaults(&p->sData);
	}

	return bRet;
}

/** @ingroup FLASH
 * フラッシュ設定構造体を全て未設定状態に設定します。未設定状態は全て 0xFF と
 * なり、逆にいえば 0xFF, 0xFFFF といった設定値を格納できます。
 *
 * @param p 構造体へのアドレス
 */
static void vConfig_UnSetAll(tsFlashApp *p) {
	memset(p, 0xFF, sizeof(tsFlashApp));
}

/**
 * フラッシュ(またはEEPROM)に保存し、モジュールをリセットする
 */

/** @ingroup FLASH
 * シリアライズされたデータを EEPROM に保存する
 *
 * @param pu8dat セーブデータの構造体
 * @param u16len データ長
 * @param u8fmt データフォーマット（RAWのみ）
 * @return
 */
bool_t Config_bUnSerialize(uint8 *pu8dat, uint16 u16len, uint8 u8fmt) {
	bool_t bRet = FALSE;

	if (u16len == sizeof(tsFlashApp)) {
		memcpy((void*)(&sConfig_UnSaved), pu8dat, u16len);
		bRet = TRUE;
	}

	return bRet;
}

/** @ingroup FLASH
 * 設定値（未セーブも含め）をシリアライズする
 *
 * @param pu8dat バッファ領域
 * @param u16len バッファ領域の最大サイズ
 * @param u8fmt データフォーマット（RAWのみ）
 * @return 0:失敗 以外:データサイズ
 */
uint16 Config_u16Serialize(uint8 *pu8dat, uint16 u16len, uint8 u8fmt) {
	uint16 u16ret = 0;

	// EEPROM 領域のコピーと sConfig_UnSaved の未セーブデータを合成
	tsFlashApp sTemp = sAppData.sFlash.sData;
	vConfig_Update(&sTemp);

	if (u8fmt == 0 && u16len >= sizeof(tsFlashApp)) {
		memcpy(pu8dat, &sTemp, sizeof(tsFlashApp));
		u16ret = sizeof(tsFlashApp);
	}

	return u16ret;
}

/** @ingroup FLASH
 * UnSaved に格納している暫定データを本体に反映させる。
 */
static void vConfig_Update(tsFlashApp *pTemp) {
	if (sConfig_UnSaved.u32appid != 0xFFFFFFFF) {
		pTemp->u32appid = sConfig_UnSaved.u32appid;
	}
	if (sConfig_UnSaved.u32chmask != 0xFFFFFFFF) {
		pTemp->u32chmask = sConfig_UnSaved.u32chmask;
	}
	if (sConfig_UnSaved.u8id != 0xFF) {
		pTemp->u8id = sConfig_UnSaved.u8id;
	}
	if (sConfig_UnSaved.u8ch != 0xFF) {
		pTemp->u8ch = sConfig_UnSaved.u8ch;
	}
	if (sConfig_UnSaved.u8pow != 0xFF) {
		pTemp->u8pow = sConfig_UnSaved.u8pow;
	}
	if (sConfig_UnSaved.u32baud_safe != 0xFFFFFFFF) {
		pTemp->u32baud_safe = sConfig_UnSaved.u32baud_safe;
	}
	if (sConfig_UnSaved.u8parity != 0xFF) {
		pTemp->u8parity = sConfig_UnSaved.u8parity;
	}
	if (sConfig_UnSaved.u32Opt != 0xFFFFFFFF) {
		pTemp->u32Opt = sConfig_UnSaved.u32Opt;
	}
	if (sConfig_UnSaved.u32EncKey != 0xFFFFFFFF) {
		pTemp->u32EncKey = sConfig_UnSaved.u32EncKey;
	}
	if (sConfig_UnSaved.u32Slp != 0xFFFFFFFF) {
		pTemp->u32Slp = sConfig_UnSaved.u32Slp;
	}
	if ( sConfig_UnSaved.u32param != 0xFFFFFFFF) {
		pTemp->u32param = sConfig_UnSaved.u32param;
	}
#ifndef USE_CUE
	if( sConfig_UnSaved.au8Event[136] != 0xFF ){
		memset(pTemp->au8Event, 0, 137);
		memcpy(pTemp->au8Event, sConfig_UnSaved.au8Event, sConfig_UnSaved.u8EventNum*8);
		pTemp->u8EventNum = sConfig_UnSaved.u8EventNum;
	}
#endif
}

/** @ingroup FLASH
 * フラッシュ(またはEEPROM)に保存する。
 */
bool_t Config_bSave() {
	tsFlash sFlash = sAppData.sFlash;
	vConfig_Update(&sFlash.sData);

	sFlash.sData.u32appkey = APP_ID;
	sFlash.sData.u32ver = VERSION_U32;

	bool_t bRet = TRUE;
	bRet &= bFlash_Write(&sFlash, 0, 0);

	// sAppData へ反映
	sAppData.sFlash.sData = sFlash.sData;

	vConfig_UnSetAll(&sConfig_UnSaved);
	vWait(100000);

	return bRet;
}

/** @ingroup MASTER
 * インタラクティブモードの画面を再描画する。
 * - 本関数は TIMER_0 のイベント処理時に u16HoldUpdateScreen カウンタがデクリメントされ、
 *   これが0になった時に呼び出される。
 *
 * - 設定内容、設定値、未セーブマーク*を出力している。FL_... のマクロ記述を参照。
 *
 */
static void vSerUpdateScreen() {
	V_PRINTF("%c[2J%c[H", 27, 27); // CLEAR SCREEN

	V_PRINTF(
			"--- CONFIG/" APP_NAME " V%d-%02d-%d/SID=0x%08x/LID=0x%02x",
			VERSION_MAIN, VERSION_SUB, VERSION_VAR, ToCoNet_u32GetSerial(),
			sAppData.u8LID);
//	V_PRINTF( "/RC=%d", sAppData.sFlash.sData.u16RcClock);
//	V_PRINTF( "/ST=%d", sAppData.u8SettingsID );

	V_PRINTF(" ---"LB);

	// Application ID
	V_PRINTF(" a: set Application ID (0x%08x)%c" LB,
			FL_IS_MODIFIED_u32(appid) ? FL_UNSAVE_u32(appid) : FL_MASTER_u32(appid),
			FL_IS_MODIFIED_u32(appid) ? '*' : ' ');

	// Device ID
	{
		uint8 u8DevID =
				FL_IS_MODIFIED_u8(id) ? FL_UNSAVE_u8(id) : FL_MASTER_u8(id);

		if (u8DevID == 0x00) { // unset
			V_PRINTF(" i: set Device ID (--)%c"LB,
					FL_IS_MODIFIED_u8(id) ? '*' : ' ');
		} else {
			V_PRINTF(" i: set Device ID (%d=0x%02x)%c"LB, u8DevID, u8DevID,
					FL_IS_MODIFIED_u8(id) ? '*' : ' ');
		}
	}
	V_PRINTF(" c: set Channels (");
	{
		// find channels in ch_mask
		uint8 au8ch[MAX_CHANNELS], u8ch_idx = 0;
		int i;
		memset(au8ch, 0, MAX_CHANNELS);
		uint32 u32mask =
				FL_IS_MODIFIED_u32(chmask) ?
				FL_UNSAVE_u32(chmask) : FL_MASTER_u32(chmask);
		for (i = 11; i <= 26; i++) {
			if (u32mask & (1UL << i)) {
				if (u8ch_idx) {
					V_PUTCHAR(',');
				}
				V_PRINTF("%d", i);
				au8ch[u8ch_idx++] = i;
			}

			if (u8ch_idx == MAX_CHANNELS) {
				break;
			}
		}
	}
	V_PRINTF(")%c" LB, FL_IS_MODIFIED_u32(chmask) ? '*' : ' ');

	V_PRINTF(" x: set Tx Power (%x)%c" LB,
			FL_IS_MODIFIED_u8(pow) ? FL_UNSAVE_u8(pow) : FL_MASTER_u8(pow),
			FL_IS_MODIFIED_u8(pow) ? '*' : ' ');

	{
		uint32 u32baud =
				FL_IS_MODIFIED_u32(baud_safe) ?
						FL_UNSAVE_u32(baud_safe) : FL_MASTER_u32(baud_safe);
		if (u32baud & 0x80000000) {
			V_PRINTF(" b: set UART baud (%x)%c" LB, u32baud,
					FL_IS_MODIFIED_u32(baud_safe) ? '*' : ' ');
		} else {
			V_PRINTF(" b: set UART baud (%d)%c" LB, u32baud,
					FL_IS_MODIFIED_u32(baud_safe) ? '*' : ' ');
		}
	}

	{
		uint8 u8dat = FL_IS_MODIFIED_u8(parity) ? FL_UNSAVE_u8(parity) : FL_MASTER_u8(parity);

		V_PRINTF(" B: set UART option (");
		vSerPrintUartOpt(u8dat);
		V_PRINTF(")%c" LB,
					FL_IS_MODIFIED_u8(parity) ? '*' : ' ');
	}

	V_PRINTF(" k: set Enc Key (0x%08X)%c" LB,
			FL_IS_MODIFIED_u32(EncKey) ? FL_UNSAVE_u32(EncKey) : FL_MASTER_u32(EncKey),
			FL_IS_MODIFIED_u32(EncKey) ? '*' : ' ');

	V_PRINTF(" o: set Option Bits (0x%08X)%c" LB,
			FL_IS_MODIFIED_u32(Opt) ? FL_UNSAVE_u32(Opt) : FL_MASTER_u32(Opt),
			FL_IS_MODIFIED_u32(Opt) ? '*' : ' ');

	V_PRINTF(" t: set Transmission Interval (%d)%c" LB,
			FL_IS_MODIFIED_u32(Slp) ? FL_UNSAVE_u32(Slp) : FL_MASTER_u32(Slp),
			FL_IS_MODIFIED_u32(Slp) ? '*' : ' ');

	V_PRINTF(" p: set Senser Parameter (0x%08X)%c" LB,
			FL_IS_MODIFIED_u32(param) ? FL_UNSAVE_u32(param) : FL_MASTER_u32(param),
			FL_IS_MODIFIED_u32(param) ? '*' : ' ');
#ifndef USE_CUE
	V_PRINTF(" e: set Event Parameter(s) (%s)%c" LB,
			(sConfig_UnSaved.au8Event[136] != 0xFF) ? sConfig_UnSaved.au8Event : sAppData.sFlash.sData.au8Event,
			(sConfig_UnSaved.au8Event[136] != 0xFF) ? '*' : ' '
			);
#endif

	V_PRINTF("---"LB);

	V_PRINTF(" S: save Configuration" LB " R: reset to Defaults" LB);
#ifdef OTA
	V_PRINTF(" *** POWER ON TWELITE CUE NEAR THIS CONFIGURATOR ***" LB LB);
#else
	V_PRINTF(LB);
#endif
	//       0123456789+123456789+123456789+1234567894123456789+123456789+123456789+123456789
}

/** @ingroup MASTER
 * UART のオプションを表示する
 */
static void vSerPrintUartOpt(uint8 u8conf) {
	const uint8 au8name_bit[] = { '8', '7' };
	const uint8 au8name_parity[] = { 'N', 'O', 'E' };
	const uint8 au8name_stop[] = { '1', '2' };

	V_PRINTF("%c%c%c",
				au8name_bit[u8conf & APPCONF_UART_CONF_WORDLEN_MASK ? 1 : 0],
				au8name_parity[u8conf & APPCONF_UART_CONF_PARITY_MASK],
				au8name_stop[u8conf & APPCONF_UART_CONF_STOPBIT_MASK ? 1 : 0]);
}

// アプリケーション依存度が高い関数を読み込み
#include "../Source_User/Interactive_User.c"
