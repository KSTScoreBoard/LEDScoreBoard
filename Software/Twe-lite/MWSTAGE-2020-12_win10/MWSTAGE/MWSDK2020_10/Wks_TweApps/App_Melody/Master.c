/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>
#include <AppHardwareApi.h>

#include "Master.h"

#include "ccitt8.h"
#include "Interrupt.h"

#include "utils.h"
#include "flash.h"

#include "common.h"
#include "config.h"

// IO Read Options
#include "btnMgr.h"
// ADC
#include "adc.h"
// I2C
#include "SMBus.h"

// MML 対応
#ifdef MML
#include "mml.h"
#include "melody_defs.h"
#endif

// 重複チェッカ
#include "duplicate_checker.h"

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "modbus_ascii.h"
#include "input_string.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_RXQUEUE_BIG
#define ToCoNet_USE_MOD_CHANNEL_MGR

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vProcessEvCoreSlp(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vProcessEvCorePwr(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);

static void vSerialInit(uint32, tsUartOpt *);
static void vProcessSerialCmd(tsModbusCmd *pSer);
static void vProcessInputByte(uint8 u8Byte);
static void vProcessInputString(tsInpStr_Context *pContext);
static void vHandleSerialInput();
static void vSerUpdateScreen();
static void vSerInitMessage();

static void vReceiveSerMsg(tsRxDataApp *pRx);
//static void vReceiveSerMsgAck(tsRxDataApp *pRx);
static void vReceiveIoData(tsRxDataApp *pRx);
static void vReceiveIoSettingRequest(tsRxDataApp *pRx);

static bool_t bCheckDupPacket(tsDupChk_Context *pc, uint32 u32Addr, uint16 u16TimeStamp);

static int16 i16TransmitIoData();
static int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq);
static int16 i16TransmitRepeat(tsRxDataApp *pRx);
static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, uint32 u32AddrSrc, uint8 u8AddrSrc, uint8 u8AddrDst, bool_t bRelay, uint8 u8Req);

static void vProcessI2CCommand(uint8 *p, uint16 u16len, uint8 u8AddrSrc);

static uint16 u16GetAve(uint16 *pu16k, uint8 u8Scale);
static bool_t bUpdateAdcValues();

static void vConfig_SetDefaults(tsFlashApp *p);
static void vConfig_UnSetAll(tsFlashApp *p);

static void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
static tsAppData sAppData; //!< アプリケーションデータ  @ingroup MASTER

tsFILE sSerStream; //!< シリアル出力ストリーム  @ingroup MASTER
tsSerialPortSetup sSerPort; //!< シリアルポートの設定  @ingroup MASTER

tsModbusCmd sSerCmd; //!< シリアル入力系列のパーサー (modbus もどき)  @ingroup MASTER
tsSerSeq sSerSeqTx; //!< 分割パケット管理構造体（送信用）  @ingroup MASTER
uint8 au8SerBuffTx[(SERCMD_MAXPAYLOAD+32)*2]; //!< sSerSeqTx 用に確保  @ingroup MASTER
tsSerSeq sSerSeqRx; //!< 分割パケット管理構造体（受信用）  @ingroup MASTER
uint8 au8SerBuffRx[SERCMD_MAXPAYLOAD+32]; //!< sSerSeqRx 用に確保  @ingroup MASTER
tsInpStr_Context sSerInpStr; //!< 文字列入力  @ingroup MASTER
static uint16 u16HoldUpdateScreen = 0; //!< スクリーンアップデートを行う遅延カウンタ  @ingroup MASTER

tsTimerContext sTimerApp; //!< タイマー管理構造体  @ingroup MASTER
tsTimerContext sTimerPWM[4]; //!< タイマー管理構造体  @ingroup MASTER

uint8 au8SerOutBuff[128]; //!< シリアルの出力書式のための暫定バッファ  @ingroup MASTER

tsDupChk_Context sDupChk_IoData; //!< 重複チェック(IO関連のデータ転送)  @ingroup MASTER
tsDupChk_Context sDupChk_SerMsg; //!< 重複チェック(シリアル関連のデータ転送)  @ingroup MASTER

#ifdef MML
tsMML sMML; //!< MML 関連 @ingroup MASTER

// 以下の定義は melody_defs.[ch] に移動しました。
// const uint8 au8MML[4][256] = { ... }
#endif

/****************************************************************************/
/***        FUNCTIONS                                                     ***/
/****************************************************************************/

/** @ingroup MASTER
 * アプリケーションの基本制御状態マシン。
 * - 特別な処理は無いが、ネットワーク層を利用したコードに改造する場合には、ここでネットワークの初期化など
 *   を実行する。
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch (pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			if (IS_APPCONF_ROLE_SILENT_MODE()) {
				vfPrintf(&sSerStream, LB"!Note: launch silent mode."LB);
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			} else {
				// LayerNetwork で無ければ、特別な動作は不要。
				// run as default...

				// 始動メッセージの表示
				if (!(u32evarg & EVARG_START_UP_WAKEUP_MASK)) {
					vSerInitMessage();
				}

				// RUNNING 状態へ遷移
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}

			break;
		}

		break;

	case E_STATE_RUNNING:
		break;
	default:
		break;
	}
}

/** @ingroup MASTER
 * アプリケーション制御（電源常時 ON モード）
 * - 起動時にランダムで処理を保留する（同時起動による送信パケットの競合回避のため）
 * - 初回のDI/AD状態確定まで待つ
 * - 実行状態では E_EVENT_APP_TICK_A (64fps タイマーイベント) を起点に処理する。
 *   - 32fps のタイミングで送信判定を行う
 *   - 定期パケット送信後は、次回のタイミングを乱数によってブレを作る。
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg1
 */
void vProcessEvCorePwr(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch(pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			sAppData.u16CtRndCt = 0;
		}

		if (eEvent == E_EVENT_TICK_TIMER) {
			static bool_t bStarted = FALSE;

			if (!sAppData.u16CtRndCt) {
				sAppData.u8AdcState = 0; // ADC の開始
				bStarted = TRUE;
				sAppData.u16CtRndCt = (ToCoNet_u16GetRand() & 0xFF) + 10; // 始動時にランダムで少し待つ（同時電源投入でぶつからないように）
			}
		}

		// 始動時ランダムな待ちを置く
		if (sAppData.u16CtRndCt && PRSEV_u32TickFrNewState(pEv) > sAppData.u16CtRndCt) {
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_IO_FIRST_CAPTURE);
			sAppData.u16CtRndCt = 32; // この変数は定期送信のタイミング用に再利用する。
		}

		break;

	case E_STATE_APP_WAIT_IO_FIRST_CAPTURE:
		// 起動直後の未確定状態
		if (eEvent == E_EVENT_APP_TICK_A) {
			if (sAppData.u8IOFixState == 0x03) {
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			} else {
				int i;

				// 起動後から最初の定期送信までの間 au16InputADC_LastTx[] に値が入らない。
				for (i = 0; i < 4; i++) {
					sAppData.sIOData_now.au16InputADC_LastTx[i] = sAppData.sIOData_now.au16InputADC[i];
				}
			}
		}
		break;

	case E_STATE_RUNNING:
		if (sAppData.u8Mode == E_IO_MODE_ROUTER) break; // リピーターは何もしない。

#ifdef ON_PRESS_TRANSMIT
		if (u32TickCount_ms - sAppData.sIOData_now.u32RxLastTick > ON_PRESS_TRANSMIT_RESET_ms) { // 500ms で自動停止
			int i;

			// ポートの値を設定する（変更フラグのあるものだけ）
			for (i = 0; i < 4; i++) {
				vPortSetHi(au8PortTbl_DOut[i]);
				sAppData.sIOData_now.au8Output[i] = 0;
			}
			vPortSetHi(12);
			vPortSetHi(13);
		}

		// 最後に停止してからの Tick
		static uint32 u32TxLastDiClear = 0x80000000;
		if (sAppData.sIOData_now.u32BtmChanged && !sAppData.sIOData_now.u32BtmBitmap && sAppData.sIOData_now.u32BtmUsed) {
			u32TxLastDiClear = u32TickCount_ms;
		}
#endif

		if (eEvent == E_EVENT_APP_TICK_A && (sAppData.u32CtTimer0 & 1)) {
			// 変更が有った場合は送信する
			int i;

			if (sAppData.u16CtRndCt) sAppData.u16CtRndCt--; // 定期パケットのカウントダウン

			if (0
#ifndef IGNORE_ADC_CHANGE
				|| sAppData.bUpdatedAdc // ADC に変化あり
#endif
				|| sAppData.sIOData_now.u32BtmChanged // ボタンに変化あり（電源投入後初回はこのフラグが生きている）
#ifdef ON_PRESS_TRANSMIT
				|| (sAppData.sIOData_now.u32BtmBitmap && sAppData.sIOData_now.u32BtmBitmap != 0xFFFFFFFF) // どれかボタンが押されているときは送信を続ける
				|| ((!sAppData.sIOData_now.u32BtmBitmap) && (u32TickCount_ms - u32TxLastDiClear < ON_PRESS_TRANSMIT_KEEP_TX_ms)) // ボタンが離されてから 1000ms 間
#endif
				|| (sAppData.u16CtRndCt == 0) // 約１秒置き
				|| (sAppData.u8Mode == E_IO_MODE_CHILD_CONT_TX
						&& ((sAppData.u32CtTimer0 & sAppData.u8FpsBitMask) == sAppData.u8FpsBitMask)) // 打って打って打ちまくれ！のモード
				) {
				DBGOUT(5, "A(%02d/%04d)%d%d: v=%04d A1=%04d/%04d A2=%04d/%04d B=%d%d%d%d %08x"LB,
					sAppData.u32CtTimer0,
					u32TickCount_ms & 8191,
					sAppData.bUpdatedAdc ? 1 : 0, sAppData.sIOData_now.u32BtmChanged ? 1 : 0,
					sAppData.sIOData_now.u16Volt,
					sAppData.sIOData_now.au16InputADC[0],
					sAppData.sIOData_now.au16InputPWMDuty[0] >> 2,
					sAppData.sIOData_now.au16InputADC[1],
					sAppData.sIOData_now.au16InputPWMDuty[1] >> 2,
					sAppData.sIOData_now.au8Input[0],
					sAppData.sIOData_now.au8Input[1],
					sAppData.sIOData_now.au8Input[2],
					sAppData.sIOData_now.au8Input[3],
					sAppData.sIOData_now.u32BtmBitmap
				);

				// 送信条件の判定
				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData();

				// 変更フラグのクリア
				sAppData.bUpdatedAdc = FALSE;
				sAppData.sIOData_now.u32BtmChanged = 0;

				// AD履歴の保存
				for (i = 0; i < 4; i++) {
					sAppData.sIOData_now.au16InputADC_LastTx[i] = sAppData.sIOData_now.au16InputADC[i];
				}

				// 次の定期パケットのタイミングを仕込む
				sAppData.u16CtRndCt = (ToCoNet_u16GetRand() & 0xF) + 24;
			}
		}
		break;

	default:
		break;
	}
}

/**  @ingroup MASTER
 * アプリケーション制御（スリープ稼動モード）
 * - ADやDIの状態が確定するまで待つ。
 * - 送信する。
 * - 送信完了を待つ。
 * - スリープする。
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCoreSlp(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch(pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			// vfPrintf(&sSerStream, "START_UP"LB, eEvent);
			if (u32evarg & EVARG_START_UP_WAKEUP_MASK) {
				// スリープからの復帰時の場合
				vfPrintf(&sSerStream, "!INF %s WAKE UP."LB, sAppData.bWakeupByButton ? "DI" : "TIMER");
			}
		}
		if (eEvent == E_EVENT_TICK_TIMER) {
			sAppData.u8AdcState = 0; // ADC の開始
			sAppData.u32AdcLastTick = u32TickCount_ms;

			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}
		break;

	case E_STATE_RUNNING:
		DBGOUT(3, "%d", sAppData.u8IOFixState);
		if (sAppData.u8IOFixState == 0x3) {
			vfPrintf(&sSerStream, "!INF DI1-4:%d%d%d%d A1-4:%04d/%04d/%04d/%04d"LB,
					sAppData.sIOData_now.au8Input[0],
					sAppData.sIOData_now.au8Input[1],
					sAppData.sIOData_now.au8Input[2],
					sAppData.sIOData_now.au8Input[3],
					sAppData.sIOData_now.au16InputADC[0] == 0xFFFF ? 9999 : sAppData.sIOData_now.au16InputADC[0],
					sAppData.sIOData_now.au16InputADC[1] == 0xFFFF ? 9999 : sAppData.sIOData_now.au16InputADC[1],
					sAppData.sIOData_now.au16InputADC[2] == 0xFFFF ? 9999 : sAppData.sIOData_now.au16InputADC[2],
					sAppData.sIOData_now.au16InputADC[3] == 0xFFFF ? 9999 : sAppData.sIOData_now.au16InputADC[3]
					);

			sAppData.sIOData_now.i16TxCbId = i16TransmitIoData();

			ToCoNet_Event_SetState(pEv, E_STATE_WAIT_TX);
		}
		break;

	case E_STATE_WAIT_TX:
		if (eEvent == E_EVENT_APP_TX_COMPLETE) {
			ToCoNet_Event_SetState(pEv, E_STATE_FINISHED);
		}
		if (PRSEV_u32TickFrNewState(pEv) > 100) {
			ToCoNet_Event_SetState(pEv, E_STATE_FINISHED);
		}
		break;

	case E_STATE_FINISHED:
		_C {
			static uint8 u8GoSleep = 0;
			if (eEvent == E_EVENT_NEW_STATE) {
				u8GoSleep = sAppData.bWakeupByButton ? 0 : 1;
			}

			// ボタンでウェイクアップしたときはチャタリングが落ち着くのを待つのにしばらく停滞する
			if (PRSEV_u32TickFrNewState(pEv) > 20) {
				u8GoSleep = 1;
			}

			if (u8GoSleep == 1) {
				ToCoNet_Event_SetState(pEv, E_STATE_FINISHED);

				vfPrintf(&sSerStream, "!INF SLEEP %dms."LB, sAppData.u32SleepDur);
				SERIAL_vFlush(sSerStream.u8Device);

				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEPING);
			}
		}
		break;

	case E_STATE_APP_SLEEPING:
		if (eEvent == E_EVENT_NEW_STATE) {
			vSleep(sAppData.u32SleepDur, TRUE, FALSE);
		}
		break;

	default:
		break;
	}
}


/** @ingroup MASTER
 * 電源投入時・リセット時に最初に実行される処理。本関数は２回呼び出される。初回は u32AHI_Init()前、
 * ２回目は AHI 初期化後である。
 *
 * - 各種初期化
 * - ToCoNet ネットワーク設定
 * - 設定IO読み取り
 * - 設定値の計算
 * - ハードウェア初期化
 * - イベントマシンの登録
 * - 本関数終了後は登録したイベントマシン、および cbToCoNet_vMain() など各種コールバック関数が
 *   呼び出される。
 *
 * @param bStart TRUE:u32AHI_Init() 前の呼び出し FALSE: 後
 */
void cbAppColdStart(bool_t bStart) {
	if (!bStart) {
		// before AHI initialization (very first of code)

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// clear application context
		memset(&sAppData, 0x00, sizeof(sAppData));
		memset(&sAppData.sIOData_now, 0xFF, sizeof(tsIOData));
		memset(&sAppData.sIOData_reserve, 0xFF, sizeof(tsIOData));

		vConfig_UnSetAll(&sAppData.sConfig_UnSaved);

		// load flash value
		sAppData.bFlashLoaded = bFlash_Read(&sAppData.sFlash, FLASH_SECTOR_NUMBER - 1, 0);

		// configure network
		sToCoNet_AppContext.u8TxMacRetry = 3; // MAC再送回数（JN516x では変更できない）
		//sToCoNet_AppContext.bRxOnIdle = TRUE; // TRUE:受信回路をオープンする(ここでは設定しない)
		sToCoNet_AppContext.u32AppId = APP_ID; // アプリケーションID
		sToCoNet_AppContext.u32ChMask = CHMASK; // 利用するチャネル群（最大３つまで）
		sToCoNet_AppContext.u8Channel = CHANNEL; // デフォルトのチャネル

		//sToCoNet_AppContext.u16TickHz = 1000; // 1Khz タイマーで高速化

		if (sAppData.bFlashLoaded) {
			sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
			// sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch; // チャネルマネージャで決定するので設定不要
			sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;

			if (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_MAC_NODE) {
				sAppData.eNwkMode = E_NWKMODE_MAC_DIRECT;
			} else
			if (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_SILENT) {
				sAppData.eNwkMode = E_NWKMODE_MAC_DIRECT;
			} else {
				sAppData.bFlashLoaded = 0;
			}

			sToCoNet_AppContext.u16TickHz = sAppData.sFlash.sData.u16Sys_Hz; // 1000Hzまで設定可能
		}

		if (sAppData.bFlashLoaded != TRUE) {
			// デフォルト値を格納する
			vConfig_SetDefaults(&(sAppData.sFlash.sData));
		}
		// ヘッダの１バイト識別子を AppID から計算
		sAppData.u8AppIdentifier = u8CCITT8((uint8*)&sToCoNet_AppContext.u32AppId, 4); // APP ID の CRC8

		// IOより状態を読み取る (ID など)
		sAppData.u32DIO_startup = ~u32PortReadBitmap(); // この時点では全部入力ポート

		// 緊急のフラッシュ消去モード
		if (    (0 == (sAppData.u32DIO_startup & (1UL << PORT_CONF1)))
			&&  (sAppData.u32DIO_startup & (1UL << PORT_CONF2))
			&&  (sAppData.u32DIO_startup & (1UL << PORT_CONF3))
			&&  (sAppData.u32DIO_startup & (1UL << 15))
			&&  (sAppData.u32DIO_startup & (1UL << PORT_INPUT4))
			) {
			//中継機設定で、I2C ポートが Lo で起動する
			uint32 u32ct = 0;

			vPortAsOutput(PORT_OUT1);

			for(;;) {
				sAppData.u32DIO_startup = ~u32PortReadBitmap();

				if (   (sAppData.u32DIO_startup & (1UL << 15))
					&& (sAppData.u32DIO_startup & (1UL << PORT_INPUT4)) ) {
					u32ct++;

					vPortSet_TrueAsLo(PORT_OUT1, u32ct & 0x8000);

					if (u32ct > 800000) { // some seconds
						bFlash_Erase(FLASH_SECTOR_NUMBER - 1); // SECTOR ERASE

						vPortSetHi(PORT_OUT1);

						while(1) {
							u32ct++;
							vPortSet_TrueAsLo(PORT_OUT1, u32ct & 0x80000);
						}
					}
				} else {
					// may launch as normal mode
					break;
				}
			}
		}

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// ToCoNet の制御 Tick [ms]
		sAppData.u16ToCoNetTickDelta_ms = 1000 / sToCoNet_AppContext.u16TickHz;

		// other hardware
		vInitHardware(FALSE);

		// 論理IDの設定チェック、その他設定値のチェック
		//  IO の設定を優先し、フラッシュ設定で矛盾するものについてはデフォルト値を書き直す。
		if (IS_LOGICAL_ID_CHILD(au8IoModeTbl_To_LogicalID[sAppData.u8Mode])) {
			// 子機IDはフラッシュ値が設定されていれば、これを採用
			if (sAppData.bFlashLoaded) {
				sAppData.u8AppLogicalId = sAppData.sFlash.sData.u8id;
			}

			if (!IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId )) {
				sAppData.u8AppLogicalId = au8IoModeTbl_To_LogicalID[sAppData.u8Mode];
			}
		}

		// 論理IDを121に保存した場合、親機で起動する
		if (sAppData.bFlashLoaded && sAppData.sFlash.sData.u8id == 121) {
			sAppData.u8AppLogicalId = 0; // 論理IDは０
			sAppData.u8Mode = 1; // 親機のモード番号
		}

		// 各モードによる諸設定
		switch(sAppData.u8Mode) {
		case E_IO_MODE_PARNET:
			sAppData.u8AppLogicalId = LOGICAL_ID_PARENT;
			break;

		case E_IO_MODE_ROUTER:
			sAppData.u8AppLogicalId = LOGICAL_ID_REPEATER;
			break;

		case E_IO_MODE_CHILD_SLP_1SEC:
			if (!sAppData.u32SleepDur) {
				if (sAppData.bFlashLoaded) {
					sAppData.u32SleepDur = sAppData.sFlash.sData.u16SleepDur_ms;
				} else {
					sAppData.u32SleepDur = MODE4_SLEEP_DUR_ms;
				}
			}
			break;

		case E_IO_MODE_CHILD_SLP_10SEC:
			if (!sAppData.u32SleepDur) {
				if (sAppData.bFlashLoaded && sAppData.sFlash.sData.u16SleepDur_s  != 0) {
					sAppData.u32SleepDur = sAppData.sFlash.sData.u16SleepDur_s * 1000L;
				} else {
					sAppData.u32SleepDur = MODE7_SLEEP_DUR_ms;
				}
			}
			break;

		case E_IO_MODE_CHILD_CONT_TX:
			sAppData.u8FpsBitMask = 1;
			if (sAppData.bFlashLoaded) {
				// 4fps: 1111
				// 8fps:  111 (64/8 -1)
				// 16pfs:  11 (64/16-1)
				// 32fps:   1 (64/32-1)
				sAppData.u8FpsBitMask = 64 / sAppData.sFlash.sData.u8Fps - 1;
				// DBGOUT(0, "fps mask = %x"LB, sAppData.u8FpsBitMask);
			}
			break;

		case E_IO_MODE_CHILD:
			break;

		default: // 未定義機能なので、SILENT モードにする。
			sAppData.u8AppLogicalId = 255;
			sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_SILENT;
			break;
		}

		// ショートアドレスの設定(決めうち)
		sToCoNet_AppContext.u16ShortAddress = SERCMD_ADDR_CONV_TO_SHORT_ADDR(sAppData.u8AppLogicalId);

		// UART の初期化
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);

		// その他の初期化
		DUPCHK_vInit(&sDupChk_IoData);
		DUPCHK_vInit(&sDupChk_SerMsg);

		if (!(IS_APPCONF_ROLE_SILENT_MODE())) {

			// メインアプリケーション処理部
			switch(sAppData.u8Mode) {
			case E_IO_MODE_PARNET:
			case E_IO_MODE_ROUTER:
			case E_IO_MODE_CHILD:
			case E_IO_MODE_CHILD_CONT_TX:
				ToCoNet_Event_Register_State_Machine(vProcessEvCorePwr); // application state machine
				sAppData.prPrsEv = (void*)vProcessEvCorePwr;
				sToCoNet_AppContext.bRxOnIdle = TRUE;
				break;
			case E_IO_MODE_CHILD_SLP_1SEC:
			case E_IO_MODE_CHILD_SLP_10SEC:
				ToCoNet_Event_Register_State_Machine(vProcessEvCoreSlp); // application state machine
				sAppData.prPrsEv = (void*)vProcessEvCoreSlp;
				sToCoNet_AppContext.bRxOnIdle = FALSE;
				break;
			default: // 未定義機能なので、SILENT モードにする。
				sToCoNet_AppContext.bRxOnIdle = FALSE;
				break;
			}

			// MAC start
			ToCoNet_vMacStart();

			// event machine
			ToCoNet_Event_Register_State_Machine(vProcessEvCore); // main state machine

		}
	}
}

/** @ingroup MASTER
 * スリープ復帰後に呼び出される関数。本関数も cbAppColdStart() と同様に２回呼び出され、u32AHI_Init() 前の
 * 初回呼び出しに於いて、スリープ復帰要因を判定している。u32AHI_Init() 関数はこれらのレジスタを初期化してしまう。
 *
 * - 変数の初期化（必要なもののみ）
 * - ハードウェアの初期化（スリープ後は基本的に再初期化が必要）
 * - イベントマシンは登録済み。
 *
 * @param bStart TRUE:u32AHI_Init() 前の呼び出し FALSE: 後
 */
void cbAppWarmStart(bool_t bStart) {
	if (!bStart) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
			;
		} else
		if(u32AHI_DioWakeStatus() & ((1UL << PORT_INPUT1) | (1UL << PORT_INPUT2) | (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4)) ) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}

	} else {
		// Initialize the necessary values
		memset(&sAppData.sIOData_now, 0xFF, sizeof(tsIOData));

		// いくつかのデータは復元
		sAppData.sIOData_now.u32BtmUsed = sAppData.sIOData_reserve.u32BtmUsed;
		memcpy(sAppData.sIOData_now.au16InputADC_LastTx,
			   sAppData.sIOData_reserve.au16InputADC_LastTx,
			   sizeof(sAppData.sIOData_now.au16InputADC_LastTx));

		// 変数の初期化（必要なものだけ）
		sAppData.u16CtTimer0 = 0; // このカウンタは、起動時からのカウントとする
		sAppData.u8IOFixState = FALSE; // IO読み取り状態の確定待ちフラグ
		sAppData.bUpdatedAdc = 0; // ADCの変化フラグ

		// other hardware
		vInitHardware(TRUE);

		// UART の初期化
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);

		// その他の初期化
		DUPCHK_vInit(&sDupChk_IoData);
		DUPCHK_vInit(&sDupChk_SerMsg);

		// MAC start
		ToCoNet_vMacStart();
	}
}

/** @ingroup MASTER
 * 本関数は ToCoNet のメインループ内で必ず１回は呼び出される。
 * ToCoNet のメインループでは、CPU DOZE 命令を発行しているため、割り込みなどが発生した時に呼び出され、
 * かつ TICK TIMER の割り込みは定期的に発生しているため、定期処理としても使用可能である。
 *
 * - シリアルの入力チェック
 */
void cbToCoNet_vMain(void) {
	/* handle serial input */
	vHandleSerialInput();
}

/** @ingroup MASTER
 * パケットの受信処理。パケットの種別によって具体的な処理関数にディスパッチしている。
 * @param psRx 受信パケット
 */
void cbToCoNet_vRxEvent(tsRxDataApp *psRx) {
	//uint8 *p = pRx->auData;

	DBGOUT(3, "Rx packet (cm:%02x, fr:%08x, to:%08x)"LB, psRx->u8Cmd, psRx->u32SrcAddr, psRx->u32DstAddr);

	if (IS_APPCONF_ROLE_SILENT_MODE()
		|| sAppData.u8Mode == E_IO_MODE_CHILD_SLP_1SEC
		|| sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC) {
		// SILENT, 1秒スリープ, 10秒スリープでは受信処理はしない。
		return;
	}

	switch (psRx->u8Cmd) {
	case TOCONET_PACKET_CMD_APP_DATA: // シリアルメッセージのパケット
		DBGOUT(1, "Rx Data", psRx->u8Cmd, psRx->u32SrcAddr, psRx->u32DstAddr);
		vReceiveSerMsg(psRx);
		break;
	case TOCONET_PACKET_CMD_APP_CMD: // アプリケーションACK
#if 0
		// APP ACK の実装は実験的
		if (p[0] == 0x01)
			vReceiveSerMsgAck(psRx);
#endif
		break;
	case TOCONET_PACKET_CMD_APP_USER_IO_DATA: // IO状態の伝送
		vReceiveIoData(psRx);
		break;
	case TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT: // IO状態の伝送
		vReceiveIoSettingRequest(psRx);
		break;
	}
}


/** @ingroup MASTER
 * 送信完了時に発生する。
 *
 * - IO 送信完了イベントはイベントマシンにイベントを伝達する。
 * - シリアルメッセージの一連のパケット群の送信完了も検出している。
 *
 * @param u8CbId 送信時に設定したコールバックID
 * @param bStatus 送信ステータス
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	//uint8 *q = au8SerOutBuff;
	if (IS_APPCONF_ROLE_SILENT_MODE()) {
		return;
	}

	// IO 関連の送信が完了した
	if (sAppData.sIOData_now.i16TxCbId >= 0
		&& u8CbId == sAppData.sIOData_now.i16TxCbId) {
		// スリープを行う場合は、このイベントを持ってスリープ遷移
		ToCoNet_Event_Process(E_EVENT_APP_TX_COMPLETE, u8CbId, sAppData.prPrsEv);
	}

	// UART 送信の完了チェック
	if (sSerSeqTx.bWaitComplete) {
		if (u8CbId >= sSerSeqTx.u8Seq && u8CbId < sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum) {
			uint8 idx = u8CbId - sSerSeqTx.u8Seq;
			if (bStatus) {
				sSerSeqTx.bPktStatus[idx] = 1;
			} else {
				if (sSerSeqTx.bPktStatus[idx] == 0) {
					sSerSeqTx.bPktStatus[idx] = -1;
				}
			}
		}

		int i, isum = 0;
		for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
			if (sSerSeqTx.bPktStatus[i] == 0) break;
			isum += sSerSeqTx.bPktStatus[i];
		}

		if (i == sSerSeqTx.u8PktNum) {
			/* 送信完了 (MAC レベルで成功した) */
			sSerSeqTx.bWaitComplete = FALSE;

			// VERBOSE MESSAGE
			DBGOUT(3, "* >>> MacAck%s(tick=%d,req=#%d) <<<" LB,
					(isum == sSerSeqTx.u8PktNum) ? "" : "Fail",
					u32TickCount_ms & 65535,
					sSerSeqTx.u8ReqNum
					);
		}
	}

	return;
}

/** @ingroup MASTER
 * ネットワーク層などのイベントが通達される。
 * 本アプリケーションでは特別な処理は行っていない。
 *
 * @param ev
 * @param u32evarg
 */
void cbToCoNet_vNwkEvent(teEvent ev, uint32 u32evarg) {
	if (IS_APPCONF_ROLE_SILENT_MODE()) {
		return;
	}

	switch(ev) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
		break;

	default:
		break;
	}
}

/** @ingroup MASTER
 * ハードウェア割り込み時に呼び出される。本処理は割り込みハンドラではなく、割り込みハンドラに登録された遅延実行部による処理で、長い処理が記述可能である。
 * 本アプリケーションに於いては、ADC/DIの入力状態のチェック、64fps のタイマーイベントの処理などを行っている。
 *
 * - E_AHI_DEVICE_TICK_TIMER
 *   - ADC の完了確認
 *   - DI の変化のチェック
 *   - イベントマシンに TIMER0 イベントを発行
 *   - インタラクティブモード時の画面再描画
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE: //ADC完了時にこのイベントが発生する。
		break;

	case E_AHI_DEVICE_TICK_TIMER: //頻繁なので、TICKTIMER は処理しない。
		break;

	case E_AHI_DEVICE_TIMER0:
		// タイマーカウンタをインクリメントする (64fps なので 64カウントごとに１秒)
		sAppData.u32CtTimer0++;
		sAppData.u16CtTimer0++;

		// ADC の完了確認
		if (sAppData.u8AdcState == 2) {
			sAppData.u8AdcState = 0; // ADC の開始
		}

		// 重複チェックのタイムアウト処理
		if ((sAppData.u32CtTimer0 & 0xF) == 0) {
			DUPCHK_bFind(&sDupChk_IoData, 0, NULL);
			DUPCHK_bFind(&sDupChk_SerMsg, 0, NULL);
		}

		// 送信処理のタイムアウト処理
		if (sSerSeqTx.bWaitComplete) {
			if (u32TickCount_ms - sSerSeqTx.u32Tick > 1000) {
				// タイムアウトとして、処理を続行
				memset(&sSerSeqTx, 0, sizeof(sSerSeqTx));
			}
		}

		// IO の変化判定を行う
		{
			uint32 bmPorts;
			uint32 bmChanged;

			if (bBTM_GetState(&bmPorts, &bmChanged)) {
				if (bmChanged) {
					// 入力ポートの値が確定したか、変化があった
					sAppData.sIOData_now.au8Input[0] = (bmPorts & (1UL << PORT_INPUT1)) ? 1 : 0;
					sAppData.sIOData_now.au8Input[1] = (bmPorts & (1UL << PORT_INPUT2)) ? 1 : 0;
					sAppData.sIOData_now.au8Input[2] = (bmPorts & (1UL << PORT_INPUT3)) ? 1 : 0;
					sAppData.sIOData_now.au8Input[3] = (bmPorts & (1UL << PORT_INPUT4)) ? 1 : 0;

					// 利用入力ポートの判定
					sAppData.sIOData_now.u32BtmBitmap = bmPorts;
					if (sAppData.sIOData_now.u32BtmUsed == 0xFFFFFFFF) {
						sAppData.sIOData_now.u32BtmUsed = bmPorts; // 一番最初の確定時に Lo のポートは利用ポート
					} else {
						sAppData.sIOData_now.u32BtmUsed |= bmPorts; // 利用ポートのビットマップは一度でもLo確定したポート
					}

					// 変化ポートの判定
					if (sAppData.u8IOFixState & 1) {
						// 初回確定後
						sAppData.sIOData_now.u32BtmChanged |= bmChanged; // 変化フラグはアプリケーションタスクに変更させる
					} else {
						// 初回確定時(スリープ復帰後も含む)
						sAppData.sIOData_now.u32BtmChanged = bmChanged; // 初回は変化を報告
					}

					// IO確定ビットを立てる
					sAppData.u8IOFixState |= 0x1;
				}
			}
		}

		// イベント処理部分にイベントを送信
		if (sAppData.prPrsEv && (sAppData.u32CtTimer0 & 1)) {
			ToCoNet_Event_Process(E_EVENT_APP_TICK_A, 0, sAppData.prPrsEv);
		}

		// シリアル画面制御のためのカウンタ
		if (!(--u16HoldUpdateScreen)) {
			if (sSerCmd.bverbose) {
				vSerUpdateScreen();
			}
		}
		break;

	default:
		break;
	}
}

/** @ingroup MASTER
 * 割り込みハンドラ。ここでは長い処理は記述してはいけない。
 *
 * - TICK_TIMER 起点で ADC の稼働、ボタンの連照処理を実行する
 */
PUBLIC uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TIMER0:
#ifdef MML
		// MML の割り込み処理
		MML_vInt(&sMML);
#endif
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		if (sAppData.u8AdcState != 0xFF)  {
			switch(sAppData.u8AdcState) {
			case 0:
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
				sAppData.u8AdcState = 1;
				break;

			case 1:
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

				if (bSnsObj_isComplete(&sAppData.sADC)) {
					// ADC値を処理する。
					bool_t bUpdated = bUpdateAdcValues();

					if ((sAppData.u8IOFixState & 2) == 0) {
						sAppData.bUpdatedAdc = 0;

						// 確定情報
						sAppData.u8IOFixState |= 0x2;
					} else {
#ifndef IGNORE_ADC_CHANGE
						sAppData.bUpdatedAdc |= bUpdated;
#else
						(void)bUpdated; // コンパイルワーニング対策
#endif
					}

					sAppData.u8AdcState = 2;
				}
			default:
				break;
			}
		}

		// ボタンハンドラの駆動
		if (sAppData.pr_BTM_handler) {
			// ハンドラを稼働させる
			(*sAppData.pr_BTM_handler)(sAppData.u16ToCoNetTickDelta_ms);
		}
		break;

	default:
		break;
	}

	return u8handled;
}

/** @ingroup MASTER
 * * ハードウェアの初期化
 * @param f_warm_start TRUE:スリープ復帰時
 */
PRIVATE void vInitHardware(int f_warm_start) {
	int i;

	// メモリのクリア
	memset(&sTimerApp, 0, sizeof(tsTimerContext));
	memset(&sTimerPWM[0], 0, sizeof(tsTimerContext));
	memset(&sTimerPWM[1], 0, sizeof(tsTimerContext));
	memset(&sTimerPWM[2], 0, sizeof(tsTimerContext));
	memset(&sTimerPWM[3], 0, sizeof(tsTimerContext));

	// 出力の設定
	for (i = 0; i < 4; i++) {
		vPortAsOutput(au8PortTbl_DOut[i]);
		if (sAppData.sIOData_reserve.au8Output[i] != 0xFF) {
			vPortSet_TrueAsLo(au8PortTbl_DOut[i], sAppData.sIOData_reserve.au8Output[i]);
		} else {
			vPortSetHi(au8PortTbl_DOut[i]);
		}
	}
#ifdef ON_PRESS_TRANSMIT
	vPortAsOutput(12);
	vPortAsOutput(13);
	vPortSetHi(12);
	vPortSetHi(13);
#endif

	// 入力の設定
	for (i = 0; i < 4; i++) {
		vPortAsInput(au8PortTbl_DIn[i]);
	}
	// モード設定
	vPortAsInput(PORT_CONF1);
	vPortAsInput(PORT_CONF2);
	vPortAsInput(PORT_CONF3);
	sAppData.u8Mode = (bPortRead(PORT_CONF1) | (bPortRead(PORT_CONF2) << 1) | (bPortRead(PORT_CONF3) << 2));

	// UART 設定
	{
		vPortAsInput(PORT_BAUD);

		uint32 u32baud = bPortRead(PORT_BAUD) ? UART_BAUD_SAFE : UART_BAUD;
		tsUartOpt sUartOpt;

		memset(&sUartOpt, 0, sizeof(tsUartOpt));

		// BAUD ピンが GND になっている場合、かつフラッシュの設定が有効な場合は、設定値を採用する (v1.0.3)
		if (sAppData.bFlashLoaded && bPortRead(PORT_BAUD)) {
			u32baud = sAppData.sFlash.sData.u32baud_safe;
			sUartOpt.bHwFlowEnabled = FALSE;
			sUartOpt.bParityEnabled = UART_PARITY_ENABLE;
			sUartOpt.u8ParityType = UART_PARITY_TYPE;
			sUartOpt.u8StopBit = UART_STOPBITS;

			// 設定されている場合は、設定値を採用する (v1.0.3)
			switch(sAppData.sFlash.sData.u8parity) {
			case 0:
				sUartOpt.bParityEnabled = FALSE;
				break;
			case 1:
				sUartOpt.bParityEnabled = TRUE;
				sUartOpt.u8ParityType = E_AHI_UART_ODD_PARITY;
				break;
			case 2:
				sUartOpt.bParityEnabled = TRUE;
				sUartOpt.u8ParityType = E_AHI_UART_EVEN_PARITY;
				break;
			}

			vSerialInit(u32baud, &sUartOpt);
		} else {
			vSerialInit(u32baud, NULL);
		}

	}

	// ADC3/4 のピンのプルアップを廃止する
	vPortDisablePullup(0);
	vPortDisablePullup(1);

	// モード設定ピンで Lo になっているポートはプルアップ停止
	// Lo でない場合は、プルアップ停止をするとリーク電流が発生する
	// ※ 暗電流に神経質な mode4, 7 のみ設定する。
	if (sAppData.u8Mode == 4) {
		vPortDisablePullup(PORT_CONF3);
	} else if (sAppData.u8Mode == 7) {
		vPortDisablePullup(PORT_CONF1);
		vPortDisablePullup(PORT_CONF2);
		vPortDisablePullup(PORT_CONF3);
	}

	// タイマの未使用ポートの解放（汎用ＩＯとして使用するため）
	vAHI_TimerFineGrainDIOControl(0x7); // bit 0,1,2 をセット (TIMER0 の各ピンを解放する, PWM1..4 は使用する)

	// メイン(64fps)タイマ管理構造体の初期化
	memset(&sTimerApp, 0, sizeof(sTimerApp));

	// activate tick timers
	sTimerApp.u8Device = E_AHI_DEVICE_TIMER0;
	sTimerApp.u16Hz =64;
	sTimerApp.u8PreScale = 4; // 15625ct@2^4
#if 0 //debug setting
	sTimerApp.u16Hz = 1;
	sTimerApp.u8PreScale = 10; // 15625ct@2^4
#endif
	vTimerConfig(&sTimerApp);
	vTimerStart(&sTimerApp);

	// button Manager (for Input)
	sAppData.sBTM_Config.bmPortMask = (1UL << PORT_INPUT1) | (1UL << PORT_INPUT2) | (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4);
	sAppData.sBTM_Config.u16Tick_ms = 8;
	sAppData.sBTM_Config.u8MaxHistory = 5;
	sAppData.sBTM_Config.u8DeviceTimer = 0xFF; // TickTimer を流用する。
	sAppData.pr_BTM_handler = prBTM_InitExternal(&sAppData.sBTM_Config);
	vBTM_Enable();

	// ADC
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	sAppData.u8AdcState = 0xFF; // 初期化中
	sAppData.sObjADC.u8SourceMask = TEH_ADC_SRC_VOLT | TEH_ADC_SRC_TEMP
			| TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2 | TEH_ADC_SRC_ADC_3 | TEH_ADC_SRC_ADC_4;

	// PWM
    uint16 u16PWM_Hz = sAppData.sFlash.sData.u32PWM_Hz; // PWM周波数
    uint8 u8PWM_prescale = 0; // prescaleの設定
#ifdef MML
     u8PWM_prescale = 1; // 130Hz 位まで使用したいので。
#else
    if (u16PWM_Hz < 10) u8PWM_prescale = 9;
    else if (u16PWM_Hz < 100) u8PWM_prescale = 6;
    else if (u16PWM_Hz < 1000) u8PWM_prescale = 3;
    else u8PWM_prescale = 0;
#endif

    for (i = 0; i < 4; i++) {
    	sTimerPWM[i].u16Hz = u16PWM_Hz;
    	sTimerPWM[i].u8PreScale = u8PWM_prescale;
    	sTimerPWM[i].u16duty = sAppData.sIOData_reserve.au16InputPWMDuty[i] == 0xFFFF ? 1024
    			: sAppData.sIOData_reserve.au16InputPWMDuty[i];
    	sTimerPWM[i].bPWMout = TRUE;
    }

	vAHI_TimerSetLocation(E_AHI_TIMER_1, TRUE, TRUE);

    sTimerPWM[0].u8Device = E_AHI_DEVICE_TIMER1;
    sTimerPWM[1].u8Device = E_AHI_DEVICE_TIMER2;
    sTimerPWM[2].u8Device = E_AHI_DEVICE_TIMER3;
    sTimerPWM[3].u8Device = E_AHI_DEVICE_TIMER4;

    for (i = 0; i < 4; i++) {
		vTimerConfig(&sTimerPWM[i]);
		vTimerStart(&sTimerPWM[i]);
    }

    // I2C
    vSMBusInit();

#ifdef MML
    // PWM1 を使用する。
    MML_vInit(&sMML, &sTimerPWM[0]); // sTimerPWM[0] 構造体は、sMML 構造体中にコピーされる。
    sTimerPWM[0].bStarted = FALSE; // 本ルーチンから制御されないように、稼働フラグを FALSE にする。（実際は稼働している）
#endif
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[2560];
	static uint8 au8SerialRxBuffer[2560];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT_MASTER;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	/* prepare stream for vfPrintf */
	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_MASTER;

	/* other initialization */
	INPSTR_vInit(&sSerInpStr, &sSerStream);
	memset(&sSerCmd, 0x00, sizeof(sSerCmd));
	memset(&sSerSeqTx, 0x00, sizeof(sSerSeqTx));
	memset(&sSerSeqRx, 0x00, sizeof(sSerSeqRx));

	sSerCmd.au8data = au8SerBuffTx;
	sSerCmd.u16maxlen = sizeof(au8SerBuffTx);
}

/** @ingroup MASTER
 * 始動時メッセージの表示
 */
static void vSerInitMessage() {
	vfPrintf(&sSerStream, LB"*** App_Melody %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	vfPrintf(&sSerStream, LB"* App ID:%08X Long Addr:%08X Short Addr:%04X LID:%02d"LB,
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
			sAppData.u8AppLogicalId );
	if (sAppData.bFlashLoaded == 0) {
		vfPrintf(&sSerStream, "!INF Default config (no save info). .." LB);
	}
	vfPrintf(&sSerStream, "!INF DIO --> %020b"LB, sAppData.u32DIO_startup);
	if (IS_APPCONF_ROLE_SILENT_MODE()) {
		vfPrintf(&sSerStream, "!ERR SILENT MODE" LB);
	}
}

/** @ingroup MASTER
 * インタラクティブモードの画面を再描画する。
 */
static void vSerUpdateScreen() {
	vfPrintf(&sSerStream, "%c[2J%c[H", 27, 27); // CLEAR SCREEN
	vfPrintf(&sSerStream,
			"--- CONFIG/App_Melody V%d-%02d-%d/SID=0x%08x/LID=0x%02x",
			VERSION_MAIN, VERSION_SUB, VERSION_VAR, ToCoNet_u32GetSerial(),
			sAppData.sFlash.sData.u8id);
	vfPrintf(&sSerStream, " ---"LB);

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
		uint8 au8ch[MAX_CHANNELS], u8ch_idx = 0;
		int i;
		memset(au8ch,0,MAX_CHANNELS);
		uint32 u32mask = FL_IS_MODIFIED_u32(chmask) ? FL_UNSAVE_u32(chmask) : FL_MASTER_u32(chmask);
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
	V_PRINT(")%c" LB,
			FL_IS_MODIFIED_u32(chmask) ? '*' : ' ');

	V_PRINT(" t: set mode4 sleep dur (%dms)%c" LB,
			FL_IS_MODIFIED_u16(SleepDur_ms) ? FL_UNSAVE_u16(SleepDur_ms) : FL_MASTER_u16(SleepDur_ms),
			FL_IS_MODIFIED_u16(SleepDur_ms) ? '*' : ' ');

	V_PRINT(" y: set mode7 sleep dur (%ds)%c" LB,
			FL_IS_MODIFIED_u16(SleepDur_s) ? FL_UNSAVE_u16(SleepDur_s) : FL_MASTER_u16(SleepDur_s),
			FL_IS_MODIFIED_u16(SleepDur_s) ? '*' : ' ');

	V_PRINT(" f: set mode3 fps (%d)%c" LB,
			FL_IS_MODIFIED_u8(Fps) ? FL_UNSAVE_u8(Fps) : FL_MASTER_u8(Fps),
			FL_IS_MODIFIED_u8(Fps) ? '*' : ' ');

	V_PRINT(" z: set PWM HZ (%d)%c" LB,
			FL_IS_MODIFIED_u32(PWM_Hz) ? FL_UNSAVE_u32(PWM_Hz) : FL_MASTER_u32(PWM_Hz),
			FL_IS_MODIFIED_u32(PWM_Hz) ? '*' : ' ');

	V_PRINT(" x: set system HZ (%d)%c" LB,
			FL_IS_MODIFIED_u16(Sys_Hz) ? FL_UNSAVE_u16(Sys_Hz) : FL_MASTER_u16(Sys_Hz),
			FL_IS_MODIFIED_u16(Sys_Hz) ? '*' : ' ');

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

	V_PRINT("---"LB);

	V_PRINT(" S: save Configuration" LB " R: reset to Defaults" LB LB);

#ifdef MML
	V_PRINT("---"LB);
	V_PRINT(" M: try MML play." LB);
#endif
	//       0123456789+123456789+123456789+1234567894123456789+123456789+123456789+123456789
}

/** @ingroup MASTER
 * シリアルポートからの入力を処理する。
 * @param i16CharExt アプリケーション中から、本関数を呼びたい時に入力系列をパラメータ渡しする（ボタンにUARTと共通の機能を割りつけたい場合など）
 */
void vHandleSerialInput() {
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		if (!sSerSeqTx.bWaitComplete) {
			//前のコマンド処理中は、UART バッファから取り出さない
			i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);
		} else {
			break;
		}

		// process
		if (i16Char >=0 && i16Char <= 0xFF) {
			//DBGOUT(0, "[%02x]", i16Char);
			if (INPSTR_bActive(&sSerInpStr)) {
				// 文字列入力モード
				uint8 u8res = INPSTR_u8InputByte(&sSerInpStr, (uint8)i16Char);

				if (u8res == E_INPUTSTRING_STATE_COMPLETE) {
					vProcessInputString(&sSerInpStr);
				} else if (u8res == E_INPUTSTRING_STATE_CANCELED) {
					V_PRINT("(canceled)");
					u16HoldUpdateScreen = 64;
				}
				continue;
			}

			{
				// コマンド書式の系列解釈、および verbose モードの判定
				uint8 u8res = ModBusAscii_u8Parse(&sSerCmd, (uint8)i16Char);

				if (u8res != E_MODBUS_CMD_EMPTY) {
					V_PUTCHAR(i16Char);
				}

				if (u8res == E_MODBUS_CMD_COMPLETE || u8res == E_MODBUS_CMD_LRCERROR) {
					// 解釈完了

					if (u8res == E_MODBUS_CMD_LRCERROR) {
						// command complete, but CRC error
						V_PRINT(LB "!INF LRC_ERR? (might be %02X)" LB, sSerCmd.u8lrc);
					}

					if (u8res == E_MODBUS_CMD_COMPLETE) {
						// process command
						vProcessSerialCmd(&sSerCmd);
					}

					continue;
				} else
				if (u8res != E_MODBUS_CMD_EMPTY) {
					if (u8res == E_MODBUS_CMD_VERBOSE_ON) {
						// verbose モードの判定があった
						vSerUpdateScreen();
					}

					if (u8res == E_MODBUS_CMD_VERBOSE_OFF) {
						vfPrintf(&sSerStream, "!INF EXIT INTERACTIVE MODE."LB);
					}

					// still waiting for bytes.
					continue;
				} else {
					; // コマンド解釈モードではない
				}
			}

			// Verbose モードのときは、シングルコマンドを取り扱う
			if (sSerCmd.bverbose) {
				vProcessInputByte(i16Char);
			}
		}
	}
}

/** @ingroup MASTER
 * １バイト入力コマンドの処理
 * @param u8Byte 入力バイト
 */
static void vProcessInputByte(uint8 u8Byte) {
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

	case 't': // set application role
		V_PRINT("Input mode4 sleep dur[ms] (DEC:100-10000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_SLEEP4);
		break;

	case 'y': // set application role
		V_PRINT("Input mode7 sleep dur[ms] (DEC:1-10000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_SLEEP7);
		break;

	case 'f': // set application role
		V_PRINT("Input mode3 fps (DEC:4,8,16,32): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 2, E_APPCONF_FPS);
		break;

	case 'z': // システムの駆動周波数の変更
		V_PRINT("Input PWM Hz (DEC:1-12800): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_PWM_HZ);
		break;

	case 'x': // システムの駆動周波数の変更
		V_PRINT("Input system base Hz (DEC:250,500,1000): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_DEC, 5, E_APPCONF_SYS_HZ);
		break;

	case 'b': // ボーレートの変更
		V_PRINT("Input baud rate (DEC:9600-230400): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 10, E_APPCONF_BAUD_SAFE);
		break;

	case 'p': // パリティの変更
		V_PRINT("Input parity (N, E, O): ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 1, E_APPCONF_BAUD_PARITY);
		break;

	case 'S':
		// フラッシュへのデータ保存
		_C {
			tsFlash sFlash = sAppData.sFlash;

			if (sAppData.sConfig_UnSaved.u32appid != 0xFFFFFFFF) {
				sFlash.sData.u32appid = sAppData.sConfig_UnSaved.u32appid;
			}
			if (sAppData.sConfig_UnSaved.u32chmask != 0xFFFFFFFF) {
				sFlash.sData.u32chmask = sAppData.sConfig_UnSaved.u32chmask;
			}
			if (sAppData.sConfig_UnSaved.u8id != 0xFF) {
				sFlash.sData.u8id = sAppData.sConfig_UnSaved.u8id;
			}
			if (sAppData.sConfig_UnSaved.u8ch != 0xFF) {
				sFlash.sData.u8ch = sAppData.sConfig_UnSaved.u8ch;
			}
			if (sAppData.sConfig_UnSaved.u8layer != 0xFF) {
				sFlash.sData.u8layer = sAppData.sConfig_UnSaved.u8layer;
			}
			if (sAppData.sConfig_UnSaved.u8role != 0xFF) {
				sFlash.sData.u8role = sAppData.sConfig_UnSaved.u8role;
			}
			if (sAppData.sConfig_UnSaved.u16SleepDur_ms != 0xFFFF) {
				sFlash.sData.u16SleepDur_ms = sAppData.sConfig_UnSaved.u16SleepDur_ms;
			}
			if (sAppData.sConfig_UnSaved.u16SleepDur_s != 0xFFFF) {
				sFlash.sData.u16SleepDur_s = sAppData.sConfig_UnSaved.u16SleepDur_s;
			}
			if (sAppData.sConfig_UnSaved.u8Fps != 0xFF) {
				sFlash.sData.u8Fps = sAppData.sConfig_UnSaved.u8Fps;
			}
			if (sAppData.sConfig_UnSaved.u32PWM_Hz != 0xFFFFFFFF) {
				sFlash.sData.u32PWM_Hz = sAppData.sConfig_UnSaved.u32PWM_Hz;
			}
			if (sAppData.sConfig_UnSaved.u16Sys_Hz != 0xFFFF) {
				sFlash.sData.u16Sys_Hz = sAppData.sConfig_UnSaved.u16Sys_Hz;
			}
			if (sAppData.sConfig_UnSaved.u32baud_safe != 0xFFFFFFFF) {
				sFlash.sData.u32baud_safe = sAppData.sConfig_UnSaved.u32baud_safe;
			}
			if (sAppData.sConfig_UnSaved.u8parity != 0xFF) {
				sFlash.sData.u8parity = sAppData.sConfig_UnSaved.u8parity;
			}

			bool_t bRet = bFlash_Write(&sFlash, FLASH_SECTOR_NUMBER - 1, 0);
			V_PRINT("!INF FlashWrite %s"LB, bRet ? "Success" : "Failed");
			vConfig_UnSetAll(&sAppData.sConfig_UnSaved);
			vWait(100000);

			V_PRINT("!INF RESET SYSTEM...");
			vWait(1000000);
			vAHI_SwReset();
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

#ifdef MML
	case '1':
	case '2':
	case '3':
	case '4':
		// テスト再生
		MML_vPlay(&sMML, au8MML[u8Byte - '1']);
		break;

	case 'M':
		// MML入力によるテスト再生
		V_PRINT(LB"MML: ");
		INPSTR_vStart(&sSerInpStr, E_INPUTSTRING_DATATYPE_STRING, 120, E_APPCONF_TEST);
		break;
#endif

	default:
		break;
	}
}

/** @ingroup MASTER
 * 文字列入力モードの処理
 */
static void vProcessInputString(tsInpStr_Context *pContext) {
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
			if (u32val >= 1 && u32val <= 65534) {
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

	case E_APPCONF_PWM_HZ:
		_C {
			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINT(LB"-> ");

			if (u32val > 0 && u32val <= 12800) {
				sAppData.sConfig_UnSaved.u32PWM_Hz = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
		}
		break;

	case E_APPCONF_SYS_HZ:
		_C {
			int i = 0;
			const uint16 u8tbl[] = {
					100, 200, 250, 400, 500, 1000, 0
			};

			uint32 u32val = u32string2dec(pu8str, u8idx);

			V_PRINT(LB"-> ");

			while(u8tbl[i]) {
				if (u8tbl[i] == u32val) {
					break;
				}
				i++;
			}

			if (u8tbl[i]) {
				sAppData.sConfig_UnSaved.u16Sys_Hz = u32val;
				V_PRINT("%d"LB, u32val);
			} else {
				V_PRINT("(ignored)"LB);
			}
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

#ifdef MML
	// MMLデバッグ再生用
	case E_APPCONF_TEST:
		_C {
			MML_vPlay(&sMML, pu8str);
			u16HoldUpdateScreen = 0; // 画面の再描画がうっとおしいので。
			return;
		}
		break;
#endif

	default:
		break;
	}

	// 一定時間待って画面を再描画
	u16HoldUpdateScreen = 96; // 1.5sec
}

/** @ingroup MASTER
 * シリアルから入力されたコマンド形式の電文を処理する。
 * @param pSer
 */
static void vProcessSerialCmd(tsModbusCmd *pSer) {
	uint8 *p = pSer->au8data;

	uint8 u8addr; // 送信先論理アドレス
	uint8 u8cmd; // コマンド

	uint8 *p_end;
	p_end = &(pSer->au8data[pSer->u16len]); // the end points 1 byte after the data end.

	bool_t bTransmitRfPacket = FALSE;

	// COMMON FORMAT
	OCTET(u8addr); // [1] OCTET : 論理ID
	OCTET(u8cmd); // [1] OCTET : 要求番号

	DBGOUT(3, "* UARTCMD ln=%d cmd=%02x req=%02x %02x%0x2%02x%02x..." LB,
			pSer->u16len,
			u8addr,
			u8cmd,
			*p,
			*(p+1),
			*(p+2),
			*(p+3)
			);

	if (u8addr == SERCMD_ADDR_TO_MODULE) {
		/*
		 * モジュール自身へのコマンド (0xDB)
		 */
		switch(u8cmd) {
		case SERCMD_ID_GET_MODULE_ADDRESS:
			vModbOut_MySerial(&sSerStream);
			break;

		case SERCMD_ID_I2C_COMMAND:
			vProcessI2CCommand(pSer->au8data, pSer->u16len, SERCMD_ADDR_TO_MODULE);
			break;

#if 0 // 各種設定コマンド(未実装)
		case SERCMD_ID_SET_NETWORK_CONFIG:
			/*
			 * 設定を読み出して Flash に書き込む。
			 */
			_C {
				bool_t bRet = FALSE;

				bRet = bModbIn_Config(pSer->au8data, &sAppData.sFlash.sData);

				// フラッシュへの書き込み
				if (bRet && bFlash_Write(&sAppData.sFlash, FLASH_SECTOR_NUMBER - 1, 0)) {
					vModbOut_AckNack(&sSerStream, TRUE);
					vWait(100000);
					vAHI_SwReset(); // リセットする
				} else {
					V_PRINT(LB "Failed to flash write...");
					vModbOut_AckNack(&sSerStream, FALSE);
				}
			}
			break;

		case SERCMD_ID_GET_NETWORK_CONFIG:
			if (sAppData.bFlashLoaded) {
				vModbOut_Config(&sSerStream, &sAppData.sFlash.sData);
			} else {
				vModbOut_AckNack(&sSerStream, FALSE);
			}
			break;
#endif

		default:
			break;
		}
	} else {
		/*
		 * 外部アドレスへの送信(IO情報の設定要求)
		 */
		if (u8cmd == SERCMD_ID_REQUEST_IO_DATA) {
			/*
			 * OCTET: 書式 (0x01)
			 * OCTET: 出力状態
			 * OCTET: 出力状態マスク
			 * BE_WORD: PWM1
			 * BE_WORD: PWM2
			 * BE_WORD: PWM3
			 * BE_WORD: PWM4
			 */
			uint8 u8format = G_OCTET();

			if (u8format == 0x01) {
				tsIOSetReq sIOreq;
				memset(&sIOreq, 0, sizeof(tsIOSetReq));

				sIOreq.u8IOports = G_OCTET();
				sIOreq.u8IOports_use_mask = G_OCTET();

				int i;

				for (i = 0; i < 4; i++) {
					sIOreq.au16PWM_Duty[i] = G_BE_WORD();
				}

				if (p_end < p) return; // v1.1.3 (終端チェック)

				DBGOUT(1, "SERCMD:IO REQ: %02x %02x %04x:%04x:%04x:%04x"LB,
						sIOreq.u8IOports,
						sIOreq.u8IOports_use_mask,
						sIOreq.au16PWM_Duty[0],
						sIOreq.au16PWM_Duty[1],
						sIOreq.au16PWM_Duty[2],
						sIOreq.au16PWM_Duty[3]
						);

				i16TransmitIoSettingRequest(u8addr, &sIOreq);
			}

			return;
		}

		/*
		 * 書式なしデータ送信
		 */
		if (sAppData.u8AppLogicalId != u8addr) {
			// 自分宛でないなら送る
			bTransmitRfPacket = TRUE;
		}
	}

	// 無線パケットを送信する
	if (bTransmitRfPacket) {
		bool_t bDoNotTransmit = FALSE;

		p = pSer->au8data; // バッファの先頭からそのまま送る
		uint16 u16len = p_end - p;

		DBGOUT(3, "* len = %d" LB, u16len);

		if (u16len > SERCMD_SER_PKTLEN * SERCMD_SER_PKTNUM || u16len <= 1) {
			// パケットが長過ぎる、または短すぎる
			bDoNotTransmit = TRUE;
		}

		if (!bDoNotTransmit) {
			// 送信リクエスト
			i16TransmitSerMsg(p, u16len, ToCoNet_u32GetSerial(), sAppData.u8AppLogicalId, p[0], FALSE, sAppData.u8UartReqNum++);
		}
	}
}

/** @ingroup MASTER
 * I2C のコマンドを実行して、応答を返す。
 *
 * - 入力フォーマット
 *   OCTET: ネットワークアドレス
 *   OCTET: 0x88
 *
 *   OCTET: 要求番号
 *   OCTET: コマンド (0x1: Write, 0x2: Read, 0x3: Write and Increment, 0x4: Write and Read)
 *   OCTET: I2Cアドレス
 *   OCTET: I2Cコマンド
 *   OCTET: データサイズ (無い時は 0)
 *   OCTET[N]: データ (データサイズが0のときは、本フィールドは無し)
 *
 * ― 出力フォーマット
 *   OCTET: ネットワークアドレス
 *   OCTET: 0x89
 *
 *   OCTET: 要求番号
 *   OCTET: コマンド (0x1: Write, 0x2: Read)
 *   OCTET: 0:FAIL, 1:SUCCESS
 *   OCTET: データサイズ (無い時は 0)
 *   OCTET[N]: データ (データサイズが0のときは、本フィールドは無し)
 *
 * @param p
 * @param u16len
 * @param u32AddrSrc
 * @param u8AddrSrc
 */
static void vProcessI2CCommand(uint8 *p, uint16 u16len, uint8 u8AddrSrc) {
	//uint8 *p_end = p + u16len;
	uint8 au8OutBuf[256 + 32];
	uint8 *q = au8OutBuf;

	bool_t bOk = TRUE;
	uint8 n;
	static volatile uint16 x;


	// 入力データの解釈
	uint8 u8Addr = G_OCTET(); (void)u8Addr;

	uint8 u8Command = G_OCTET();
	if(u8Command != SERCMD_ID_I2C_COMMAND) {
		return;
	}

	uint8 u8ReqNum = G_OCTET();
	uint8 u8I2C_Oper = G_OCTET();
	uint8 u8I2C_Addr = G_OCTET();
	uint8 u8I2C_Cmd = G_OCTET();
	uint8 u8DataSize = G_OCTET();

	uint8 *pu8data = p;
	//uint8 *pu8data_end = p + u8DataSize;

#if 0
	if (pu8data_end != p_end) {
		DBGOUT(1, "I2CCMD: incorrect data."LB);
		return;
	}
#endif

	// 出力用のバッファを用意しておく
	S_OCTET(sAppData.u8AppLogicalId);
	S_OCTET(SERCMD_ID_I2C_COMMAND_RESP);
	S_OCTET(u8ReqNum);
	S_OCTET(u8I2C_Oper);
	//ここで q[0] 成功失敗フラグ, q[1] データサイズ, q[2]... データ
	q[0] = FALSE;
	q[1] = 0;

	DBGOUT(1, "I2CCMD: req#=%d Oper=%d Addr=%02x Cmd=%02x Siz=%d"LB,
			u8ReqNum, u8I2C_Oper, u8I2C_Addr, u8I2C_Cmd, u8DataSize);


	switch (u8I2C_Oper) {
	case 1:
		bOk &= bSMBusWrite(u8I2C_Addr, u8I2C_Cmd, u8DataSize, u8DataSize == 0 ? NULL : pu8data);
		break;

	case 2:
		if (u8DataSize > 0) {
			bOk &= bSMBusSequentialRead(u8I2C_Addr, u8DataSize, &(q[2]));
			if (bOk) q[1] = u8DataSize;
		} else {
			bOk = FALSE;
		}
		break;

	case 3:
		for(n = 0; n < u8DataSize; n++){
			bOk &= bSMBusWrite(u8I2C_Addr, u8I2C_Cmd + n, 1, &pu8data[n]);
			for(x = 0; x < 16000; x++); //wait (e.g. for memory device)
		}
		break;

	case 4:
		if (u8DataSize > 0) {
			bOk &= bSMBusWrite(u8I2C_Addr, u8I2C_Cmd, 0, NULL);
			if (bOk) bOk &= bSMBusSequentialRead(u8I2C_Addr, u8DataSize, &(q[2]));
			if (bOk) q[1] = u8DataSize;
		} else {
			bOk = FALSE;
		}
		break;

	default:
		DBGOUT(1, "I2CCMD: unknown operation(%d). "LB, u8I2C_Oper);
		return;
	}

	q[0] = bOk; // 成功失敗フラグを書き込む
	q = q + 2 + q[1]; // ポインタ q を進める（データ末尾+1)

	if (u8AddrSrc == SERCMD_ADDR_TO_MODULE) {
		vSerOutput_ModbusAscii(&sSerStream, u8AddrSrc,
				au8OutBuf[1], au8OutBuf + 2, q - au8OutBuf - 2);
	} else {
		i16TransmitSerMsg(au8OutBuf, q - au8OutBuf, ToCoNet_u32GetSerial(),
				sAppData.u8AppLogicalId, u8AddrSrc, FALSE, sAppData.u8UartReqNum++);
	}
}

/** @ingroup MASTER
 * 重複パケットの判定
 *
 * - DUPCHK_vInin() の初期化を行うこと
 * - DUPCHK_bFIND(0,NULL) を一定周期で呼び出すこと
 *
 * @param pc 管理構造体
 * @param u32Addr
 * @param u16TimeStamp
 * @return
 */
static bool_t bCheckDupPacket(tsDupChk_Context *pc, uint32 u32Addr, uint16 u16TimeStamp) {
	uint32 u32Key;
	if (DUPCHK_bFind(pc, u32Addr, &u32Key)) {
		// 最後に受けたカウンタの値が得られるので、これより新しい
		uint16 u16Delta = (uint16)u32Key - u16TimeStamp;
		if (u16Delta < 32) { // 32count=500ms, 500ms の遅延は想定外だが。
			// すでに処理したパケット
			return TRUE;
		}
	}

	// 新しいパケットである（時間情報を格納する）
	DUPCHK_vAdd(pc, u32Addr, u16TimeStamp);
	return FALSE;
}

/** @ingroup MASTER
 * IO 情報の送信
 *
 * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 送信フラグ
 *
 *   - BE_WORD: 電圧
 *   - OCTET: 温度 (int8型)
 *   - OCTET: ボタン (LSB から順に SW1 ... SW4, 1=Lo)
 *   - OCTET: ボタン変化 (LSB から順に SW1 ... SW4, 1=変化)
 *   - OCTET: ADC1 (MSB から 8bit)
 *   - OCTET: ADC2 (MSB から 8bit)
 *   - OCTET: ADC3 (MSB から 8bit)
 *   - OCTET: ADC4 (MSB から 8bit)
 *   - OCTET: ADC 詳細部 (MSB b8b7b6b5b4b3b2b1 LSB とすると b2b1 が ADC1 の LSB 2bit, 以下同様)
 *
 * @returns -1:ERROR, 0..255 CBID
 */
static int16 i16TransmitIoData() {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret = -1;
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	int i;

	// ペイロードを構成
	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId) ? LOGICAL_ID_PARENT : LOGICAL_ID_CHILDREN); // 宛先
	S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ
	S_OCTET(0); // 中継フラグ

	S_BE_WORD(sAppData.sIOData_now.u16Volt);
	S_OCTET((uint8)((sAppData.sIOData_now.i16Temp + 50)/100));
	S_OCTET(
		(  (sAppData.sIOData_now.u32BtmBitmap & (1UL << PORT_INPUT1) ? 1 : 0)
		 | (sAppData.sIOData_now.u32BtmBitmap & (1UL << PORT_INPUT2) ? 2 : 0)
		 | (sAppData.sIOData_now.u32BtmBitmap & (1UL << PORT_INPUT3) ? 4 : 0)
		 | (sAppData.sIOData_now.u32BtmBitmap & (1UL << PORT_INPUT4) ? 8 : 0)
		)
	);
	S_OCTET(
		(  (sAppData.sIOData_now.u32BtmUsed & (1UL << PORT_INPUT1) ? 1 : 0)
		 | (sAppData.sIOData_now.u32BtmUsed & (1UL << PORT_INPUT2) ? 2 : 0)
		 | (sAppData.sIOData_now.u32BtmUsed & (1UL << PORT_INPUT3) ? 4 : 0)
		 | (sAppData.sIOData_now.u32BtmUsed & (1UL << PORT_INPUT4) ? 8 : 0)
		)
	);

	// ADC 部のエンコード
	uint8 u8LSBs = 0;
	for (i = 0; i < 4; i++) {
		// MSB 部分 (10bit目～3bit目まで)
		uint16 u16v = sAppData.sIOData_now.au16InputADC[i];
		u16v >>= 2; // ADC 値は 0...2400mV

		uint8 u8MSB = (u16v >> 2) & 0xFF;
		S_OCTET(u8MSB);

		// 下2bitを u8LSBs に詰める
		u8LSBs>>=2;
		u8LSBs |= ((u16v << 6) & 0xC0); //
	}
	S_OCTET(u8LSBs); // 詳細ビット部分を記録

	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_IO_DATA; // パケット種別

	// 送信する
	sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry     = 0x81; // 1回再送

	// フレームカウントとコールバック識別子の指定
	sAppData.u16TxFrame++;
	sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
	sTx.u8CbId = sTx.u8Seq;

	{
		/* MAC モードでは細かい指定が可能 */
		sTx.bAckReq = FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 4; // 再送間隔
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			i16Ret = sTx.u8CbId;
			sAppData.sIOData_now.u32TxLastTick = u32TickCount_ms;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * IOデータを中継送信する。
 *
 * @param pRx 受信したときのデータ
 * @return -1:Error, 0..255:CbId
 */
int16 i16TransmitRepeat(tsRxDataApp *pRx) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret = -1;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	// Payload
	memcpy(sTx.auData, pRx->auData, pRx->u8Len);
	sTx.u8Len = pRx->u8Len;

	// コマンド設定
	sTx.u8Cmd = pRx->u8Cmd; // パケット種別

	// 送信する
	sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry     = 0x81; // ２回再送

	// フレームカウントとコールバック識別子の指定
	sAppData.u16TxFrame++;
	sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
	sTx.u8CbId = sTx.u8Seq;

	/* MAC モードでは細かい指定が可能 */
	sTx.bAckReq = FALSE;
	sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
	sTx.u16RetryDur = 8; // 再送間隔

	sTx.u16DelayMin = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大8ms)
	sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大8ms)

	// 送信API
	if (ToCoNet_bMacTxReq(&sTx)) {
		i16Ret = sTx.u8CbId;
	}

	return i16Ret;
}


/** @ingroup MASTER
 *
 *  * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 送信フラグ
 *
 *   - OCTET: 形式 (1固定)
 *   - OCTET: ボタン (LSB から順に SW1 ... SW4, 1=Lo)
 *   - OCTET: ボタン使用フラグ (LSB から順に SW1 ... SW4, 1=変化)
 *   - BE_WORD: PWM1 (0..1024 or 0xffff)
 *   - BE_WORD: PWM2 (0..1024 or 0xffff)
 *   - BE_WORD: PWM3 (0..1024 or 0xffff)
 *   - BE_WORD: PWM4 (0..1024 or 0xffff)
 *
 * @param u8DstAddr 送信先
 * @param pReq 設定データ
 * @return -1:Error, 0..255:CbId
 */
int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret, i;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(u8DstAddr); // 宛先
	S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ
	S_OCTET(0); // 中継フラグ

	S_OCTET(1); // 形式

	// DIO の設定
	S_OCTET(pReq->u8IOports);
	S_OCTET(pReq->u8IOports_use_mask);

	// PWM の設定
	for (i = 0; i < 4; i++) {
		S_BE_WORD(pReq->au16PWM_Duty[i]);
	}

	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT; // パケット種別

	// 送信する
	sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry     = 0x81; // 1回再送

	{
		/* 送信設定 */
		sTx.bAckReq = FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 4; // 再送間隔
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			i16Ret = sTx.u8CbId;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * シリアルメッセージの送信要求。
 * パケットを分割して送信する。
 *
 *  - Packet 構造
 *   - [1] OCTET    : 識別ヘッダ(APP ID より生成)
 *   - [1] OCTET    : プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - [1] OCTET    : 送信元個体識別論理ID
 *   - [1] BE_DWORD : 送信元シリアル番号
 *   - [1] OCTET    : 送信先シリアル番号
 *   - [1] OCTET    : 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - [1] OCTET    : 送信フラグ(リピータ用)
 *   - [1] OCTET    : 要求番号
 *   - [1] OCTET    : パケット数(total)
 *   - [1] OCTET    : パケット番号 (0...total-1)
 *   - [2] BE_WORD  : 本パケットのバッファ位置
 *   - [1] OCTET    : 本パケットのデータ長
 *
 * @param p ペイロード
 * @param u16len ペイロード長
 * @param bRelay 中継フラグ TRUE:中継する
 * @return -1:失敗
 */
static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, uint32 u32AddrSrc, uint8 u8AddrSrc, uint8 u8AddrDst, bool_t bRelay, uint8 u8Req) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	// パケットを分割して送信する。
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));
	uint8 *q; // for S_??? macros

	// 処理中のチェック（処理中なら送信せず失敗）
	if (sSerSeqTx.bWaitComplete) {
		return -1;
	}

	// sSerSeqTx は分割パケットの管理構造体
	sSerSeqTx.u8IdSender = sAppData.u8AppLogicalId;
	sSerSeqTx.u8IdReceiver = u8AddrDst;

	sSerSeqTx.u8PktNum = (u16len - 1) / SERCMD_SER_PKTLEN + 1;
	sSerSeqTx.u16DataLen = u16len;
	sSerSeqTx.u8Seq = sSerSeqTx.u8SeqNext; // パケットのシーケンス番号（アプリでは使用しない）
	sSerSeqTx.u8SeqNext = sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum; // 次のシーケンス番号（予め計算しておく）
	sSerSeqTx.u8ReqNum = u8Req; // パケットの要求番号（この番号で送信系列を弁別する）
	sSerSeqTx.bWaitComplete = TRUE;
	sSerSeqTx.u32Tick = u32TickCount_ms;
	memset(sSerSeqTx.bPktStatus, 0, sizeof(sSerSeqTx.bPktStatus));

	DBGOUT(3, "* >>> Transmit(req=%d) Tick=%d <<<" LB, sSerSeqTx.u8ReqNum , u32TickCount_ms & 65535);

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA; // data packet.
	sTx.u8Retry = 0x81;
	sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // application retry

	int i;
	for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
		q = sTx.auData;
		sTx.u8Seq = sSerSeqTx.u8Seq + i;
		sTx.u8CbId = sTx.u8Seq; // callback will reported with this ID

		// ペイロードを構成
		S_OCTET(sAppData.u8AppIdentifier);
		S_OCTET(APP_PROTOCOL_VERSION);
		S_OCTET(u8AddrSrc); // アプリケーション論理アドレス
		S_BE_DWORD(u32AddrSrc);  // シリアル番号
		//S_OCTET(IS_LOGICAL_ID_PARENT(sAppData.u8Mode) ? LOGICAL_ID_PARENT : LOGICAL_ID_CHILDREN); // 宛先
		S_OCTET(sSerSeqTx.u8IdReceiver); // 宛先
		S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ

		S_OCTET(bRelay ? 1 : 0); // 中継フラグ
		//S_OCTET(sSerSeqTx.u8ReqNum & 1 ? 1 : 0); // 中継フラグ（テスト）

		S_OCTET(sSerSeqTx.u8ReqNum); // request number
		S_OCTET(sSerSeqTx.u8PktNum); // total packets
		S_OCTET(i); // packet number
		S_BE_WORD(i * SERCMD_SER_PKTLEN); // offset

		uint8 u8len_data = (u16len >= SERCMD_SER_PKTLEN) ? SERCMD_SER_PKTLEN : u16len;
		S_OCTET(u8len_data);

		memcpy (q, p, u8len_data);
		q += u8len_data;

		sTx.u8Len = q - sTx.auData;

		// あて先など
		sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト

		if (sAppData.eNwkMode == E_NWKMODE_MAC_DIRECT) {
			sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
			sTx.bAckReq = FALSE;
			sTx.u8Retry = 0x81; // ２回再送

			ToCoNet_bMacTxReq(&sTx);
		}

		p += u8len_data;
		u16len -= SERCMD_SER_PKTLEN;
	}

	return 0;
}

/** @ingroup MASTER
 * データ送受信パケットの処理。
 * - 受信したデータを UART に出力
 * - 中継機の場合、中継パケットを送信
 * - 受信データを基に DO/PWM 出力を設定する
 * @param pRx 受信したときのデータ
 */
static void vReceiveIoData(tsRxDataApp *pRx) {
	int i, j;
	uint8 *p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET(); (void)u8AppLogicalId_Dest;

	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	if (bCheckDupPacket(&sDupChk_IoData, u32Addr, u16TimeStamp)) {
		return;
	}

	uint8 u8TxFlag = G_OCTET();

	// 中継の判定 (このフラグが１なら中継済みのパケット）
	if (u8TxFlag == 0 && sAppData.u8Mode == E_IO_MODE_ROUTER) {
		// リピータの場合はここで中継の判定
		*(p-1) = 1; // 中継済みフラグのセット

		i16TransmitRepeat(pRx);
		return;
	}

	// 親機子機の判定
	if (	(IS_LOGICAL_ID_PARENT(u8AppLogicalId) && IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId))
		||	(IS_LOGICAL_ID_CHILD(u8AppLogicalId) && IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) ) {
		; // 親機⇒子機、または子機⇒親機への伝送
	} else {
		return;
	}

	/* 電圧 */
	uint16 u16Volt = G_BE_WORD();

	/* 温度 */
	int8 i8Temp = (int8)G_OCTET();
	(void)i8Temp;

	/* BUTTON */
	uint8 u8ButtonState = G_OCTET();
	uint8 u8ButtonChanged = G_OCTET();
	// ポートの値を設定する（変更フラグのあるものだけ）
	for (i = 0, j = 1; i < 4; i++, j <<= 1) {
		if (u8ButtonChanged & j) {
#ifdef MML
			// ボタンが押し下げられた時に、再生を開始する
			// 注：このコードだけでは以下の振る舞いを行います
			//   送り側のボタンが複数押された場合、一番最後のボタン指定が有効になります
			if (u8ButtonState & j) {
				// ボタンの出力状態が Hi の場合のみ処理を行う。
				// Lo が継続している場合(ボタン長押し時)は無視。
				if (sAppData.sIOData_now.au8Output[i] == 0 || sAppData.sIOData_now.au8Output[i] == 0xFF) {
					MML_vPlay(&sMML, au8MML[i]);
				}
			}
#endif
			vPortSet_TrueAsLo(au8PortTbl_DOut[i], u8ButtonState & j);
			sAppData.sIOData_now.au8Output[i] = u8ButtonState & j;

/* 記事中はこの位置にコードを挿入していましたが、sAppData.sIOData_now.au8Output[i]
 * による判定を追加するため、同変数値更新前の位置を移動しました。
#ifdef MML
			if (u8ButtonState & j) {
				MML_vPlay(&sMML, au8MML[j]);
			}
#endif
*/

#ifdef ON_PRESS_TRANSMIT
			if (i == 0) vPortSet_TrueAsLo(12, u8ButtonState & j);
			if (i == 1) vPortSet_TrueAsLo(13, u8ButtonState & j);
#endif
		}
	}

	/* ADC 値 */
	for (i = 0; i < 4; i++) {
		// 8bit scale
		sAppData.sIOData_now.au16OutputDAC[i] = G_OCTET();
		if (sAppData.sIOData_now.au16OutputDAC[i] == 0xFF) {
			sAppData.sIOData_now.au16OutputDAC[i] = 0xFFFF;
		} else {
			// 10bit scale に拡張
			sAppData.sIOData_now.au16OutputDAC[i] <<= 2;
		}
	}
	uint8 u8DAC_Fine = G_OCTET();
	for (i = 0; i < 4; i++) {
		if (sAppData.sIOData_now.au16OutputDAC[i] != 0xFFFF) {
			// 下２ビットを復旧
			sAppData.sIOData_now.au16OutputDAC[i] |= (u8DAC_Fine & 0x03);
		}
		u8DAC_Fine >>= 2;
	}

	// ADC 値を PWM の DUTY 比に変換する
	// 下は 5%, 上は 10% を不感エリアとする。
	for (i = 0; i < 4; i++) {
		uint16 u16Adc = sAppData.sIOData_now.au16OutputDAC[i];
		u16Adc <<= 2; // 以下の計算は 12bit 精度
		if (u16Adc > ADC_MAX_THRES) { // 最大レンジの 98% 以上なら、未定義。
			sAppData.sIOData_now.au16OutputPWMDuty[i] = 0xFFFF;
		} else {
			// 10bit+1 スケール正規化
			int32 iR = (uint32)u16Adc * 2 * 1024 / u16Volt;

			// y = 1.15x - 0.05 の線形変換
			//   = (115x-5)/100 = (23x-1)/20 = 1024*(23x-1)/20/1024 = 51.2*(23x-1)/1024 ~= 51*(23x-1)/1024
			// iS/1024 = 51*(23*iR/1024-1)/1024
			// iS      = (51*23*iR - 51*1024)/1024
			int32 iS = 51*23*iR - 51*1024;
			if (iS <= 0) {
				iS = 0;
			} else {
				iS >>= 10; // 1024での割り算
				if (iS >= 1024) { // DUTY は 0..1024 で正規化するので最大値は 1024。
					iS = 1024;
				}
			}
			sAppData.sIOData_now.au16OutputPWMDuty[i] = iS;
		}
	}

	// PWM の再設定
	for (i = 0; i < 4; i++) {
		if (sAppData.sIOData_now.au16OutputPWMDuty[i] != 0xFFFF) {
			sTimerPWM[i].u16duty = sAppData.sIOData_now.au16OutputPWMDuty[i];
			if (sTimerPWM[i].bStarted) {
				vTimerStart(&sTimerPWM[i]); // DUTY比だけ変更する
			}
		}
	}

	/* タイムスタンプ */
	sAppData.sIOData_now.u32RxLastTick = u32TickCount_ms;

	/* UART 出力 */
	if (!sSerCmd.bverbose) {
		// 以下のようにペイロードを書き換えて UART 出力
		pRx->auData[0] = pRx->u8Len; // １バイト目はバイト数
		pRx->auData[2] = pRx->u8Lqi; // ３バイト目(もともとは送信元の LogicalID) は LQI

		vSerOutput_ModbusAscii(&sSerStream, u8AppLogicalId, SERCMD_ID_INFORM_IO_DATA, pRx->auData, pRx->u8Len);
	}
}

/** @ingroup MASTER
 * データ送受信パケットの処理。
 * - 受信したデータを UART に出力
 * - 中継機の場合、中継パケットを送信
 * - 受信データを基に DO/PWM 出力を設定する
 * @param pRx 受信したときのデータ
 */
static void vReceiveIoSettingRequest(tsRxDataApp *pRx) {
	int i, j;
	uint8 *p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();

	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	if (bCheckDupPacket(&sDupChk_IoData, u32Addr, u16TimeStamp)) {
		return;
	}

	uint8 u8TxFlag = G_OCTET();

	// 中継の判定 (このフラグが１なら中継済みのパケット）
	if (u8TxFlag == 0 && sAppData.u8Mode == E_IO_MODE_ROUTER) {
		// リピータの場合はここで中継の判定
		*(p-1) = 1; // 中継済みフラグのセット

		i16TransmitRepeat(pRx);
		return;
	}

	// 親機子機の判定
	if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		// 子機の場合は、任意の送り主から受けるが、送り先が CHILDREN(120) またはアドレスが一致している事
		if (!(u8AppLogicalId_Dest == sAppData.u8AppLogicalId || u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN)) {
			return;
		}
	} else if(IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		// 親機の場合は、子機からの送信である事
		if (!(u8AppLogicalId_Dest == LOGICAL_ID_PARENT && IS_LOGICAL_ID_CHILD(u8AppLogicalId))) {
			return;
		}
	} else {
		// それ以外は処理しない
		return;
	}

	/* 書式 */
	uint8 u8Format = G_OCTET();

	if (u8Format == 1) {
		/* BUTTON */
		uint8 u8ButtonState = G_OCTET();
		uint8 u8ButtonChanged = G_OCTET();
		// ポートの値を設定する（変更フラグのあるものだけ）
		for (i = 0, j = 1; i < 4; i++, j <<= 1) {
			if (u8ButtonChanged & j) {
				vPortSet_TrueAsLo(au8PortTbl_DOut[i], u8ButtonState & j);
				sAppData.sIOData_now.au8Output[i] = u8ButtonState & j;
			}
		}

		for (i = 0; i < 4; i++) {
			uint16 u16Duty = G_BE_WORD();
			if (u16Duty <= 1024) {
				sAppData.sIOData_now.au16OutputPWMDuty[i] = u16Duty;
			} else {
				sAppData.sIOData_now.au16OutputPWMDuty[i] = 0xFFFF;
			}
		}

		DBGOUT(1, "RECV:IO REQ: %02x %02x %04x:%04x:%04x:%04x"LB,
				u8ButtonState,
				u8ButtonChanged,
				sAppData.sIOData_now.au16OutputPWMDuty[0],
				sAppData.sIOData_now.au16OutputPWMDuty[1],
				sAppData.sIOData_now.au16OutputPWMDuty[2],
				sAppData.sIOData_now.au16OutputPWMDuty[3]
				);

		// PWM の再設定
		for (i = 0; i < 4; i++) {
			if (sAppData.sIOData_now.au16OutputPWMDuty[i] != 0xFFFF) {
				sTimerPWM[i].u16duty = sAppData.sIOData_now.au16OutputPWMDuty[i];
				if (sTimerPWM[i].bStarted) vTimerStart(&sTimerPWM[i]); // DUTY比だけ変更する
			}
		}

	}

	/* UART 出力 */
#if 0
	if (!sSerCmd.bverbose) {
		// 以下のようにペイロードを書き換えて UART 出力
		pRx->auData[0] = pRx->u8Len; // １バイト目はバイト数
		pRx->auData[2] = pRx->u8Lqi; // ３バイト目(もともとは送信元の LogicalID) は LQI

		vSerOutput_ModbusAscii(&sSerStream, u8AppLogicalId, SERCMD_ID_INFORM_IO_DATA, pRx->auData, pRx->u8Len);
	}
#endif
}

/** @ingroup MASTER
 * シリアルメッセージの受信処理。分割パケットがそろった時点で UART に出力。
 * - 中継機なら新たな系列として中継。
 *   - 中継機と送信元が両方見える場合は、２つの系列を受信を行う事になる。
 * tsRxDataApp *pRx
 */
static void vReceiveSerMsg(tsRxDataApp *pRx) {
	uint8 *p = pRx->auData;

	/* ヘッダ情報の読み取り */
	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) { return; }
	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) { return; }
	uint8 u8AppLogicalId = G_OCTET(); (void)u8AppLogicalId;
	uint32 u32Addr = G_BE_DWORD();
	uint8 u8AppLogicalId_Dest = G_OCTET();
	uint16 u16TimeStamp = G_BE_WORD(); (void)u16TimeStamp;
	uint8 u8TxFlag = G_OCTET();

	/* ここから中身 */
	uint8 u8req = G_OCTET();
	uint8 u8pktnum = G_OCTET();
	uint8 u8idx = G_OCTET();
	uint16 u16offset = G_BE_WORD();
	uint8 u8len = G_OCTET();

	/* 宛先によって処理するか決める */
	if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		if (!(u8AppLogicalId_Dest == sAppData.u8AppLogicalId || u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN)) {
			return;
		}
	} else if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		if (!(u8AppLogicalId_Dest == LOGICAL_ID_PARENT && IS_LOGICAL_ID_CHILD(u8AppLogicalId))) {
			return;
		}
	} else if (IS_LOGICAL_ID_REPEATER(sAppData.u8AppLogicalId)) {
		// リピータ機は一旦受け取る。
		;
	} else {
		return;
	}

	// 受信パケットのチェック。
	//  - 分割パケットが混在したような場合は、新しい系列で始める。
	//    複数のストリームを同時受信できない！
	bool_t bNew = FALSE;
	if (sSerSeqRx.bWaitComplete) {
		// exceptional check
		if(u32TickCount_ms - sSerSeqRx.u32Tick > 2000) {
			// time out check
			bNew = TRUE;
		}
		if (u8req != sSerSeqRx.u8ReqNum) {
			// different request number is coming.
			bNew = TRUE;
		}
		if (u32Addr != sSerSeqRx.u32SrcAddr) {
			// packet comes from different nodes. (discard this one!)
			bNew = TRUE;
		}
	} else {
		// 待ち状態ではないなら新しい系列
		bNew = TRUE;
	}

	if(bNew) {
		// treat this packet as new, so clean control buffer.
		memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
	}

	if (!sSerSeqRx.bWaitComplete) {
		// 新しいパケットを受信した

		// 最初にリクエスト番号が適切かどうかチェックする。
		uint32 u32key;
		if (DUPCHK_bFind(&sDupChk_SerMsg, u32Addr, &u32key)) {
			int iPrev = u32key, iNow = u8req;

			if (iNow == iPrev || (uint8)(iNow - iPrev) > 0x80) { //v1.0.4 iPrev == 255 で正しく判定していなかった
				// 最近受信したものより新しくないリクエスト番号の場合は、処理しない
				bNew = FALSE;
			}
		}

		if (bNew) {
			sSerSeqRx.bWaitComplete = TRUE;
			sSerSeqRx.u32Tick = u32TickCount_ms;
			sSerSeqRx.u32SrcAddr = u32Addr;
			sSerSeqRx.u8PktNum = u8pktnum;
			sSerSeqRx.u8ReqNum = u8req;

			sSerSeqRx.u8IdSender = u8AppLogicalId;
			sSerSeqRx.u8IdReceiver = u8AppLogicalId_Dest;

			DUPCHK_vAdd(&sDupChk_SerMsg, sSerSeqRx.u32SrcAddr, u8req);
		}
	}

	if (sSerSeqRx.bWaitComplete) {
		if (u16offset + u8len <= sizeof(au8SerBuffRx) && u8idx < sSerSeqRx.u8PktNum) {
			// check if packet size and offset information is correct,
			// then copy data to buffer and mark it as received.
			if (!sSerSeqRx.bPktStatus[u8idx]) {
				sSerSeqRx.bPktStatus[u8idx] = 1;
				memcpy (au8SerBuffRx + u16offset, p, u8len);
			}

			// the last packet indicates full data length.
			if (u8idx == sSerSeqRx.u8PktNum - 1) {
				sSerSeqRx.u16DataLen = u16offset + u8len;
			}

			// 中継パケットのフラグを格納する
			if (u8TxFlag) {
				sSerSeqRx.bRelayPacket = TRUE;
			}
		}

		// check completion
		int i;
		for (i = 0; i < sSerSeqRx.u8PktNum; i++) {
			if (sSerSeqRx.bPktStatus[i] == 0) break;
		}
		if (i == sSerSeqRx.u8PktNum) {
			// 分割パケットが全て届いた！
#if 0
			i16TransmitSerMsgAck(); // アプリケーションACKを返す
#endif

			if (IS_LOGICAL_ID_REPEATER(sAppData.u8AppLogicalId)) {
				// 中継
				if (!sSerSeqRx.bRelayPacket) {
					i16TransmitSerMsg(
							au8SerBuffRx,
							sSerSeqRx.u16DataLen,
							u32Addr,
							u8AppLogicalId,
							au8SerBuffRx[0],
							TRUE,
							sSerSeqRx.u8ReqNum
						);
				}
			} else {

				if (au8SerBuffRx[1] == SERCMD_ID_I2C_COMMAND) {
					// I2C の処理
					vProcessI2CCommand(au8SerBuffRx, sSerSeqRx.u16DataLen, sSerSeqRx.u8IdSender);
				} else {
					// 受信データの出力
					vSerOutput_ModbusAscii(
							&sSerStream,
							sSerSeqRx.u8IdSender, // v1.2.1 １バイト目は送り主に変更。他のコマンドなどとの整合性のため。
							au8SerBuffRx[1],
							au8SerBuffRx + 2,
							sSerSeqRx.u16DataLen - 2
							);
				}
			}

			memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
		}
	}
}

/** @ingroup MASTER
 * サンプルの平均化処理
 * @param pu16k 平均化データを格納した配列
 * @param u8Scale 平均化数スケーラ (2^u8Scale ヶで平均化する)
 */
static uint16 u16GetAve(uint16 *pu16k, uint8 u8Scale) {
	int k, kmax;
	uint32 u32Ave = 0;
	kmax = 1 << u8Scale;

	for (k = 0; k < kmax; k++) {
		uint16 v = pu16k[k];

		if (v == 0xFFFF) {
			// 入力NGデータなので処理しない。
			u32Ave = 0xFFFFFFFF;
			break;
		}

		u32Ave += v;
	}
	if (u32Ave != 0xFFFFFFFF) {
		// 平均値が得られる。
		u32Ave >>= u8Scale; // ４で割る
	}
	return u32Ave & 0xFFFF;
}

/** @ingroup MASTER
 * ADC の入力値の処理を行う。
 * - 戻り値は au16InputADC_LastTx[] に対して変化があった場合に TRUE をとる
 * - sAppData.sIOData_now.u8HistIdx >= 4 になれば、最初のサンプルの平均化が終了している。
 * @return TRUE:変化判定あり FALSE:変化なし
 */
static bool_t bUpdateAdcValues() {
	bool_t bUpdated = FALSE;
	int i, k;

	// ADC が完了したので内部データに保存する
	sAppData.sIOData_now.au16InputADC[0] = (uint16)sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.sIOData_now.au16InputADC[1] = (uint16)sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_3];
	sAppData.sIOData_now.au16InputADC[2] = (uint16)sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2];
	sAppData.sIOData_now.au16InputADC[3] = (uint16)sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_4];
	sAppData.sIOData_now.i16Temp = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_TEMP];
	sAppData.sIOData_now.u16Volt = (uint16)sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT];

	// 履歴情報を保存する
	//  初期値は 0xFF なので、最初は ++ を実行して 0 からカウントする。
	//  途中で 127 で 63 に巻き戻す。4以上なら初回実行済みである判定のため。
	sAppData.sIOData_now.u8HistIdx++;
	if (sAppData.sIOData_now.u8HistIdx >= 127) {
		sAppData.sIOData_now.u8HistIdx = 63;
	}

	k = sAppData.sIOData_now.u8HistIdx & 0x3;
	for (i = 0; i < 4; i++) {
		if (IS_ADC_RANGE_OVER(sAppData.sIOData_now.au16InputADC[i])) {
			sAppData.sIOData_now.au16InputADC[i] = 0xFFFF;
			sAppData.sIOData_now.au16InputADC_History[i][k] = 0xFFFF;
		} else {
			sAppData.sIOData_now.au16InputADC_History[i][k] = sAppData.sIOData_now.au16InputADC[i];
		}
	}
	sAppData.sIOData_now.au16Volt_History[sAppData.sIOData_now.u8HistIdx & (HIST_VOLT_COUNT-1)] = sAppData.sIOData_now.u16Volt;

	// 電圧を先に平均化しておく
	if (sAppData.sIOData_now.u8HistIdx >= HIST_VOLT_COUNT) {
		sAppData.sIOData_now.u16Volt = u16GetAve(sAppData.sIOData_now.au16Volt_History, HIST_VOLT_SCALE);
	}

	// ADC1...4 の平均化処理
	for (i = 0; i < 4; i++) {
		// 過去４サンプルの平均値を保存する
		if (sAppData.sIOData_now.u8HistIdx >= 4) {
			sAppData.sIOData_now.au16InputADC[i] = u16GetAve(sAppData.sIOData_now.au16InputADC_History[i], 2);
		}

		// 判定0:そもそもADC値が適正でないなら処理しない。
		if (sAppData.sIOData_now.au16InputADC[i] == 0xFFFF) {
			continue;
		}

		// 判定1：送信前データが存在しない場合。
		if (sAppData.sIOData_now.au16InputADC_LastTx[i] == 0xFFFF) {
			bUpdated = FALSE;
			continue;
		}

		// 判定2：最終送信データとの差(粗)
		int iDelta = abs((int)sAppData.sIOData_now.au16InputADC_LastTx[i] - (int)sAppData.sIOData_now.au16InputADC[i]);
		if (iDelta > ADC_DELTA_COARSE) {
			bUpdated = TRUE;
			continue;
		}

		// 判定3:最終送信データとの差(細), 経過時間が 300ms 以上
		if (iDelta > ADC_DELTA_FINE && (u32TickCount_ms - sAppData.sIOData_now.u32TxLastTick > ADC_TIMEOUT_TO_FINE_CHECK)) {
			bUpdated = TRUE;
			continue;
		}
	}

	return bUpdated;
}

/** @ingroup FLASH
 * フラッシュ設定構造体をデフォルトに巻き戻す。
 * @param p 構造体へのアドレス
 */
static void vConfig_SetDefaults(tsFlashApp *p) {
	p->u32appid = APP_ID;
	p->u32chmask = CHMASK;
	p->u8ch = CHANNEL;
	p->u8id = 0;
	p->u8role = E_APPCONF_ROLE_MAC_NODE;
	p->u8layer = 1;

	p->u16SleepDur_ms = 1000;
	p->u16SleepDur_s = 10;
	p->u8Fps = 32;

	p->u32PWM_Hz = 1000;

	p->u32baud_safe = UART_BAUD_SAFE;
	p->u8parity = 0; // none
	p->u16Sys_Hz = 250;
}

/** @ingroup FLASH
 * フラッシュ設定構造体を全て未設定状態に巻き戻す。
 * @param p 構造体へのアドレス
 */
static void vConfig_UnSetAll(tsFlashApp *p) {
	memset (p, 0xFF, sizeof(tsFlashApp));
}

/** @ingroup MASTER
 * スリープの実行
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
static void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep) {
	// print message.

	// IO 情報の保存
	memcpy(&sAppData.sIOData_reserve, &sAppData.sIOData_now, sizeof(tsIOData));

	// stop interrupt source, if interrupt source is still running.
	;

	// set UART Rx port as interrupt source
	vAHI_DioSetDirection(PORT_INPUT_MASK, 0); // set as input

	(void)u32AHI_DioInterruptStatus(); // clear interrupt register
	vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
	vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
	// vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE

	// wake up using wakeup timer as well.
	ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32SleepDur_ms, bPeriodic, bDeep); // PERIODIC RAM OFF SLEEP USING WK0
}


/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
