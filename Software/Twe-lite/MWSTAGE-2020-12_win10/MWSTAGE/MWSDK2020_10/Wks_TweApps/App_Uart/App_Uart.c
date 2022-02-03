/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>
#include <AppHardwareApi.h>

#include "App_Uart.h"

#include "config.h"

#include <ccitt8.h>
#include <Interrupt.h>
#include <ByteQueue.h>

#include <utils.h>
#include "input_string.h"

#include "flash.h"

#include "common.h"
#include "config.h"

// IO Read Options
#include "btnMgr.h"

// 重複チェッカ
#include "duplicate_checker.h"

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "sercmd_plus3.h"
#include "sercmd_gen.h"


#include "Interactive.h"
#include "cmd_gen.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_TXRXQUEUE_BIG
#define ToCoNet_USE_MOD_CHANNEL_MGR

#ifdef NWK_LAYER
#define ToCoNet_USE_MOD_NWK_LAYERTREE
#define ToCoNet_USE_MOD_NBSCAN
#define ToCoNet_USE_MOD_NBSCAN_SLAVE
#define ToCoNet_USE_MOD_DUPCHK
#endif

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/** TOCONET の DUPCHK を使用する */
#define USE_TOCONET_DUPCHK
//#define FIX_DUPCHK_L106S // ToCoNet 1.0.6 での振る舞いに対応する

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
// DEBUG RTS
#undef DEBUG_RTS
#ifdef DEBUG_RTS
#warning "DEBUG_RTS"
#define DEBUG_RTS_LED 15
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);

static void vSerialInit(uint32, tsUartOpt *);
static void vProcessSerialCmd(tsSerCmd_Context *pSer);
static void vProcessSerialCmd_TransmitEx(tsSerCmd_Context *pSer);
static void vProcessInputByte_Transparent(tsSerCmd_Context *pSerCmd, uint16 u16Byte);
static void vProcessInputByte_Chat(tsSerCmd_Context *pSerCmd, uint16 u16Byte);
static void vProcessInputByte_FormatCmd(tsSerCmd_Context *pSerCmd, uint16 u16Byte);
static void vHandleSerialInput();
static void vSerChatPrompt();

static void vReceiveSerMsg(tsRxDataApp *pRx);
//static void vReceiveSerMsgAck(tsRxDataApp *pRx);

static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, tsTxDataApp *pTxTemplate,
		uint32 u32AddrSrc, uint8 u8AddrSrc, uint32 u32AddrDst, uint8 u8AddrDst,
		uint8 u8Relay, uint8 u8Req, uint8 u8RspId, uint8 u8Opt);
static int16 i16Transmit_Transparent();

static void vSleep0(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep);
static void vSleep();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
tsAppData sAppData; //!< アプリケーションデータ  @ingroup MASTER

uint8 u8dev_uart_master; // UART主ポート(UART0)
uint8 u8dev_uart_salve; // UART副ポート(UART1)

tsFILE sSerStream; //!< シリアル出力ストリーム  @ingroup MASTER

tsSerCmdPlus3_Context sSerCmd_P3; //!< シリアル入力系列のパーサー (+ + +)  @ingroup MASTER
tsSerCmd_Context sSerCmd; //!< シリアル入力系列のパーサー   @ingroup MASTER
tsSerCmd_Context sSerCmd_Slave; //!< シリアル入力系列のパーサー   @ingroup MASTER
uint16 u16SerTimeout; //!< 入力タイムアウト

static void (*pf_vProcessInputByte)(tsSerCmd_Context *, uint16);
static void (*pf_vProcessInputByte_Slave)(tsSerCmd_Context *, uint16);

tsSerSeq sSerSeqTx; //!< 分割パケット管理構造体（送信用）  @ingroup MASTER
uint8 au8SerBuffTx[SERCMD_MAXPAYLOAD+32]; //!< sSerSeqTx 用に確保  @ingroup MASTER

tsSerSeq sSerSeqRx; //!< 分割パケット管理構造体（受信用）  @ingroup MASTER
uint8 _au8SerBuffRx[SERCMD_MAXPAYLOAD+32]; //!< sSerSeqRx 用に確保  @ingroup MASTER
uint8 * au8SerBuffRx = _au8SerBuffRx + 16; //!< 後ろに書き込めるようにバッファの先頭をずらす  @ingroup MASTER
#define SIZEOF_au8SerBuffRx (sizeof(_au8SerBuffRx) - 16) //!< sizeof が使えないので、マクロ定義

tsSerCmd_Context sSerCmdOut; //!< シリアル出力用   @ingroup MASTER

tsInpStr_Context sSerInpStr; //!< 文字列入力  @ingroup MASTER
uint16 u16HoldUpdateScreen = 0; //!< スクリーンアップデートを行う遅延カウンタ  @ingroup MASTER

tsTimerContext sTimerApp; //!< タイマー管理構造体  @ingroup MASTER

uint8 au8SerOutBuff[128]; //!< シリアルの出力書式のための暫定バッファ  @ingroup MASTER
tsSerCmd_Context sSerCmdTemp; //!< シリアル出力用 @ingroup MASTER

#ifdef USE_TOCONET_DUPCHK
static void vInitDupChk();
static tsToCoNet_DupChk_Context* psDupChk = NULL; //!< 重複チェック (TOCONET ライブラリ版)
#else
tsDupChk_Context sDupChk_SerMsg; //!< 重複チェック(シリアル関連のデータ転送)  @ingroup MASTER
#endif

uint8 au8TxCbId_to_RespID[256]; //!< 送信パケットのコールバックIDから RespID を紐づける表 @ingroup MASTER

static bool_t bPendingTxOnTransparent = 0; //!< 透過モードで送信待ち (送信待ちのバッファが送信完了されるまではバッファからの取り出しもしない) @ingroup MASTER

/** @ingroup MASTER
 * UARTモードによる送信種別仕訳
 * (Transparent, chat, binary/ascii が混在しないように)
 */
const uint8 au8UartModeToTxCmdId[] = {
	0,    // TRANSPARENT
	1, 1, // ASCII, BINARY
	2, 3  // チャットモード
};

/****************************************************************************/
/***        FUNCTIONS                                                     ***/
/****************************************************************************/

#ifdef USE_TOCONET_DUPCHK
/**
 * TOCONET DUPCHK の初期化
 */
static void vInitDupChk() {
//	TOCONET_DUPCHK_TICK_SCALE = 0x85;
	TOCONET_DUPCHK_TIMEOUT_ms = 1024;
	TOCONET_DUPCHK_DECLARE_CONETXT(DUPCHK,40); //!< 重複チェック
	psDupChk = ToCoNet_DupChk_psInit(DUPCHK);
}
#endif

/** @ingroup MASTER
 * 始動時メッセージの表示
 */
static void vSerInitMessage() {
	// 始動メッセージ
	if (sAppData.u8uart_mode == UART_MODE_CHAT) {
		vfPrintf(&sSerStream, LB"!INF TWE UART APP V%d-%02d-%d, SID=0x%08X, LID=0x%02x",
				VERSION_MAIN, VERSION_SUB, VERSION_VAR, ToCoNet_u32GetSerial(), sAppData.u8AppLogicalId);
		vSerChatPrompt();
	} else
	if (sAppData.u8uart_mode == UART_MODE_ASCII || sAppData.u8uart_mode == UART_MODE_BINARY) {
		// ASCII, BINARY モードでは自分のアドレスを表示する
		vSerResp_GetModuleAddress();
	}
}

/**  @ingroup MASTER
 * Chatモードのプロンプト表示
 */
static void vSerChatPrompt() {
	if (sAppData.u8uart_mode == UART_MODE_CHAT) {
		if (sAppData.sFlash.sData.au8ChatHandleName[0] == 0x00) {
			vfPrintf(&sSerStream, LB"%08X:%d> ", ToCoNet_u32GetSerial(), sAppData.u8UartReqNum);
		} else {
			vfPrintf(&sSerStream, LB"%s:%d> ", sAppData.sFlash.sData.au8ChatHandleName, sAppData.u8UartReqNum);
		}

		// 入力中なら表示を回復する
		if (sSerCmd.u8state < 0x80 && sSerCmd.u8state != E_SERCMD_EMPTY) {
			sSerCmd.vOutput(&sSerCmd, &sSerStream);
		} else
		if (sSerCmd_Slave.u8state < 0x80 && sSerCmd_Slave.u8state != E_SERCMD_EMPTY) {
			sSerCmd_Slave.vOutput(&sSerCmd_Slave, &sSerStream);
		}
	}
}

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
			// ネットワーク稼働状態を FALSE にする
			sAppData.bNwkUp = FALSE;
			sAppData.bSilent = FALSE;

			// 暗号化キーを登録する
#ifdef USE_AES
			if (IS_CRYPT_MODE()) {
				ToCoNet_bRegisterAesKey((void*)(sAppData.sFlash.sData.au8AesKey), NULL);
			}
#endif

			if (IS_APPCONF_ROLE_SILENT_MODE() && (sAppData.u8uart_mode == UART_MODE_ASCII || sAppData.u8uart_mode == UART_MODE_BINARY)) {
				// ASCII, BINARY モード時は SILENT 設定を有効にする
				sAppData.bSilent = TRUE;
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}

			// 始動メッセージの表示
			if (!(u32evarg & EVARG_START_UP_WAKEUP_MASK)) {
				vSerInitMessage();
			}
		} else if (eEvent == E_ORDER_KICK) {
			sAppData.bSilent = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}

		break;

	case E_STATE_RUNNING:
		if (eEvent == E_EVENT_NEW_STATE) {
			if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
#ifdef NWK_LAYER
				// layer tree の実装
				switch(APPCONF_ROLE()) {
				case E_APPCONF_ROLE_PARENT:
					sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_PARENT;
					break;
				case E_APPCONF_ROLE_ROUTER:
					sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ROUTER;
					sAppData.sNwkLayerTreeConfig.u8Layer = sAppData.sFlash.sData.u8layer;

					sAppData.sNwkLayerTreeConfig.u8Second_To_Rescan = 15;
					sAppData.sNwkLayerTreeConfig.u8Second_To_Relocate = 5;
					sAppData.sNwkLayerTreeConfig.u8Ct_To_Relocate = 5;

					if (IS_APPCONF_OPT_LAYER_NWK_CONNET_ONE_LEVEL_HIGHER_ONLY()) {
						// 上１レベルに限定して接続する
						sAppData.sNwkLayerTreeConfig.u8LayerOptions = 0x00000001;
					}
					break;
				case E_APPCONF_ROLE_ENDDEVICE:
					sAppData.sNwkLayerTreeConfig.u8Second_To_Relocate = 0xFF;
					sAppData.sNwkLayerTreeConfig.u8Second_To_Rescan = 60;
					sAppData.sNwkLayerTreeConfig.u8Ct_To_Relocate = 10;
					sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
					break;
				default: break;
				}

				//sAppData.sNwkLayerTreeConfig.u16TxMaxDelayDn_ms = 30;
					// 親機から子機へのパケットは Ack 無しブロードキャスト４回送信を行うが、
					// 中継機もこのパケットを同様に４回送信する。このため、複数の中継機が
					// 同一電波範囲内にある事を想定してデフォルトは大きな値になっている。
					// 本アプリは中継機無しを想定しているので、値を小さくし応答性を重視する。
#ifndef OLDNET
				sAppData.sNwkLayerTreeConfig.u8StartOpt = TOCONET_MOD_LAYERTREE_STARTOPT_NB_BEACON;				// ビーコン方式のネットワークを使用する
				sAppData.sNwkLayerTreeConfig.u8Second_To_Beacon = TOCONET_MOD_LAYERTREE_DEFAULT_BEACON_DUR;		// set NB beacon interval
#endif
				// 設定を行う
				if (!sAppData.pContextNwk) {
					sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);

					// ネットワークの開始
					ToCoNet_Nwk_bInit(sAppData.pContextNwk);
					ToCoNet_Nwk_bStart(sAppData.pContextNwk);
				} else {
					ToCoNet_Nwk_bResume(sAppData.pContextNwk);
				}
#endif
			} else if (sAppData.eNwkMode == E_NWKMODE_MAC_DIRECT) {
				// MAC start
				sToCoNet_AppContext.bRxOnIdle = TRUE;
				ToCoNet_vMacStart();

				sAppData.bNwkUp = TRUE; // ネットワーク稼働状態
			}
		} else
		if (eEvent == E_EVENT_TICK_SECOND) {
			// DBGOUT(0, "*"); // １秒置きに * を表示する (デバッグ用)
#ifdef USE_TOCONET_DUPCHK
			// 重複チェッカのクリーンアップ
			ToCoNet_DupChk_vClean(psDupChk);
#endif
		} else
		if (eEvent == E_EVENT_TOCONET_ON_SLEEP) {
#ifdef NWK_LAYER
			if (sAppData.pContextNwk) {
				// ネットワークをポーズする
				ToCoNet_Nwk_bPause(sAppData.pContextNwk);
			}
#endif
			sAppData.bNwkUp = FALSE;
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

		// configure network
		sToCoNet_AppContext.u8TxMacRetry = 3; // MAC再送回数（JN516x では変更できない）
		sToCoNet_AppContext.bRxOnIdle = FALSE; // TRUE:受信回路は起動時には FALSE とする
		sToCoNet_AppContext.u32AppId = APP_ID; // アプリケーションID
		sToCoNet_AppContext.u32ChMask = CHMASK; // 利用するチャネル群（最大３つまで）
		sToCoNet_AppContext.u8Channel = CHANNEL;

		sToCoNet_AppContext.u16TickHz = 500; // 2ms TickCount (System Timer)

#if 0
		sToCoNet_AppContext.u8CCA_Level = 1;
		sToCoNet_AppContext.u8CCA_Retry = 0;
#endif

		// configuration
		vConfig_UnSetAll(&sAppData.sConfig_UnSaved);

		// デフォルト値を格納する
		vConfig_SetDefaults(&(sAppData.sFlash.sData));

		// load flash value
		tsFlash sSettingFlash;
		sAppData.bFlashLoaded = bFlash_Read(&sSettingFlash, FLASH_SECTOR_NUMBER - 1, 0);
		if (sAppData.bFlashLoaded &&
			(   sSettingFlash.sData.u32appkey != APP_ID
			 || (sSettingFlash.sData.u32ver !=
					((VERSION_MAIN<<16) | (VERSION_SUB<<8) | (VERSION_VAR))))
			) {
			sAppData.bFlashLoaded = FALSE;
		}

		if (sAppData.bFlashLoaded) {
			sAppData.sFlash = sSettingFlash; // フラッシュからロードした設定を採用
		}

		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		// sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch; // チャネルマネージャで決定するので設定不要
		sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;
		sToCoNet_AppContext.u8TxPower = sAppData.sFlash.sData.u16power & 0x000F; // 出力設定

		// ROLE から eNwkMode を設定
		if (APPCONF_ROLE() <= E_APPCONF_ROLE_MAC_NODE_MAX) {
			sAppData.eNwkMode = E_NWKMODE_MAC_DIRECT;
		} else
		if (APPCONF_ROLE() & E_APPCONF_ROLE_NWK_MASK) {
			sAppData.eNwkMode = E_NWKMODE_LAYERTREE;
		}

		// ヘッダの１バイト識別子を AppID から計算
		sAppData.u8AppIdentifier = u8CCITT8((uint8*)&sToCoNet_AppContext.u32AppId, 4) & 0xFE;
			// APP ID の CRC8 (下１ビットは中継ビット)

		// IOより状態を読み取る (ID など)
		//sAppData.u32DIO_startup = ~(u32PortReadBitmap()); // この時点では全部入力ポート

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// ToCoNet の制御 Tick [ms]
		sAppData.u16ToCoNetTickDelta_ms = 1000 / sToCoNet_AppContext.u16TickHz;

		// チャネル数のカウント
		int i;
		sAppData.u8NumChannels = 0;
		for (i = 11; i < 26; i++) {
			if (sToCoNet_AppContext.u32ChMask & (1UL << i)) {
				sAppData.u8NumChannels++;
			}
		}

		// 他のハードウェアの初期化
		vInitHardware(FALSE);

		/// 論理IDをフラッシュ設定した場合
		// 子機IDを設定した場合
		//if (IS_LOGICAL_ID_CHILD(sAppData.sFlash.sData.u8id)) {
		//	sAppData.u8Mode = E_IO_MODE_CHILD; // 子機に強制する場合はコメントアウト
		//}
		// 親機
		if (sAppData.sFlash.sData.u8id == 121) {
			sAppData.u8Mode = E_IO_MODE_PARNET; // 親機のモード番号
		}
		// 中継機
		if (sAppData.sFlash.sData.u8id == LOGICAL_ID_REPEATER
			|| sAppData.sFlash.sData.u8id == 122) { // 他のアプリでは 122 にしているため、統一
			sAppData.u8Mode = E_IO_MODE_REPEATER; // 親機のモード番号
		}

#ifdef NWK_LAYER_FORCE
		// 強制的に Layer Tree で起動する
		sAppData.eNwkMode = E_NWKMODE_LAYERTREE;
#endif

		// モードごとの独自設定
		sAppData.u8AppLogicalId = 0; // 最初に０にしておく
		switch(sAppData.u8Mode) {
		case E_IO_MODE_PARNET:
			sAppData.u8AppLogicalId = LOGICAL_ID_PARENT;
#ifdef NWK_LAYER_FORCE
			sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_PARENT;
#endif
			break;

		case E_IO_MODE_REPEATER:
			sAppData.u8AppLogicalId = LOGICAL_ID_REPEATER;

			// 中継子機の設定 (ピンを優先する)
			if (!(sAppData.sFlash.sData.u8role & E_APPCONF_ROLE_NWK_MASK)) {
				sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_MAC_NODE_REPEATER;
			} else {
				sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_ROUTER;
			}
			break;

		case E_IO_MODE_CHILD:
		case E_IO_MODE_REPEAT_CHILD:
			// 子機IDはフラッシュ値が設定されていれば、これを採用
			sAppData.u8AppLogicalId = sAppData.sFlash.sData.u8id;

			// 値が子機のID範囲外ならデフォルト値を設定する (120=0x78)
			if (!IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
				sAppData.u8AppLogicalId = au8IoModeTbl_To_LogicalID[E_IO_MODE_CHILD];
			}

			// 中継子機の設定(ピンを優先する)
			if (sAppData.u8Mode == E_IO_MODE_REPEAT_CHILD) {
				if (!(sAppData.sFlash.sData.u8role & E_APPCONF_ROLE_NWK_MASK)) {
					// NWK なし (MAC アプリ)
					if (!IS_REPEATER()) {
						sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_MAC_NODE_REPEATER; // 1リピートに設定
					}
				} else {
					// NWK あり (ROUTER)
					sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_ROUTER;
				}
			}

#ifdef NWK_LAYER_FORCE
			sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_ROUTER;
#endif
			break;

		default: // 未定義機能なので、SILENT モードにする。
			sAppData.u8AppLogicalId = 255;
			sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_SILENT_MASK;
			break;
		}

		// ショートアドレスの設定(決めうち)
		sToCoNet_AppContext.u16ShortAddress = SERCMD_ADDR_CONV_TO_SHORT_ADDR(sAppData.u8AppLogicalId);

		// UART の初期化
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);

		// その他の初期化
#ifdef USE_TOCONET_DUPCHK
		vInitDupChk();
#else
		 DUPCHK_vInit(&sDupChk_SerMsg);
#endif

		// イベント処理関数を稼働させる
		ToCoNet_Event_Register_State_Machine(vProcessEvCore); // main state machine
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
		// 変数の初期化（必要なものだけ）
		sAppData.u16CtTimer0 = 0; // このカウンタは、起動時からのカウントとする

		// other hardware
		vInitHardware(TRUE);

		// UART の初期化
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);

		// その他の初期化

#ifdef USE_TOCONET_DUPCHK
		vInitDupChk();
#else
		DUPCHK_vInit(&sDupChk_SerMsg);
#endif

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
	DBGOUT(5, "Rx packet (cm:%02x, fr:%08x, to:%08x)"LB, psRx->u8Cmd, psRx->u32SrcAddr, psRx->u32DstAddr);

	if (sAppData.bSilent) {
		// SILENT, 1秒スリープ, 10秒スリープでは受信処理はしない。
		return;
	}

	// 暗号化モードで平文は無視する
	if (IS_CRYPT_MODE()) {
		if (!psRx->bSecurePkt) {
			DBGOUT(5, LB"Recv Plain Pkt!");
			return;
		}
	}

	// UART モードに合致するパケット以外は受け付けない
	DBGOUT(3, "<Rx: %d[%d],%08x,%08x>"LB,
		psRx->u8Cmd,
		au8UartModeToTxCmdId[sAppData.u8uart_mode],
		psRx->u32SrcAddr,
		psRx->u32DstAddr);

	if (psRx->u8Cmd == au8UartModeToTxCmdId[sAppData.u8uart_mode]) {
		vReceiveSerMsg(psRx);
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
	if (sAppData.bSilent) {
		return;
	}

	// 透過モードでは bWaitComplete 状態は設定せず、送信完了も処理しない
	if (sAppData.u8uart_mode == UART_MODE_TRANSPARENT) {
		sSerSeqTx.bWaitComplete = FALSE;
		return;
	}

	// UART 送信の完了チェック
	bool_t bHandleMsg = FALSE;
	if (sSerSeqTx.bWaitComplete) {
		uint8 idx = (u8CbId & CBID_MASK_BASE) - sSerSeqTx.u8Seq;

		if (idx < sSerSeqTx.u8PktNum) {
			bHandleMsg = TRUE;

			if (bStatus) {
				sSerSeqTx.bPktStatus[idx] = 1;
			} else {
				if (sSerSeqTx.bPktStatus[idx] == 0) {
					sSerSeqTx.bPktStatus[idx] = -1;
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
				DBGOUT(3, "* >>> MacTxFin%s(tick=%d,req=#%d,cb=%02X) <<<" LB,
						(isum == sSerSeqTx.u8PktNum) ? "" : "Fail",
						u32TickCount_ms & 65535,
						sSerSeqTx.u8ReqNum,
						u8CbId
						);

				// 応答を返す
				if ((sAppData.u8uart_mode == UART_MODE_ASCII || sAppData.u8uart_mode == UART_MODE_BINARY) && !(u8CbId & CBID_MASK_SILENT)) {
					if (!IS_APPCONF_OPT_NO_TX_RESULT()) {
						vSerResp_TxEx(sSerSeqTx.u8RespID, isum == sSerSeqTx.u8PktNum);
					}
				}

				// スリープする場合
				if (sSerSeqTx.bSleepOnFinish) {
					vSleep();
				}
			}
		}
	}

	// 単独パケットで併行送信する場合(bWaitCompleteフラグは立たない)の応答
	if (!bHandleMsg && (sAppData.u8uart_mode == UART_MODE_ASCII || sAppData.u8uart_mode == UART_MODE_BINARY)) {
		// 応答を返す
		if (!(u8CbId & CBID_MASK_SPLIT_PKTS) && !(u8CbId & CBID_MASK_SILENT)) { // 分割パケットおよびリピートパケットは処理しない
			if (!IS_APPCONF_OPT_NO_TX_RESULT()) {
				vSerResp_TxEx(au8TxCbId_to_RespID[u8CbId], bStatus);
			}
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
	if (sAppData.bSilent) {
		return;
	}

	switch(ev) {
	case E_EVENT_TOCONET_NWK_START:
		sAppData.bNwkUp = TRUE;
		DBGOUT(1, LB"!Note: nwk started"LB);
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
		sAppData.bNwkUp = FALSE;
		DBGOUT(1, LB"!Note: nwk disconnected"LB);
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
		/*
		 * TickTimer 起点で送信を開始する
		 */
		if (	sAppData.u8uart_mode == UART_MODE_TRANSPARENT // 送信モードが透過モード
			&&	sAppData.u16ShAddr_Pair != 0 // 初期化が終わっている事
			&&	sSerCmd.u16len // データが有る事
			&&	!sSerSeqTx.bWaitComplete // 送信中でない事
		) {
			// 送信要求を MAC 層に伝える
			if (!IS_APPCONF_OPT_TX_TRIGGER_CHAR() || bPendingTxOnTransparent) {
				i16Transmit_Transparent();
			}
		}

		if( IS_APPCONF_OPT_M3_SLEEP_AT_ONCE() ){
			if( bPortRead(PORT_SLEEP) ){
				vSleep();
			}
		}
		break;

	case E_AHI_DEVICE_TIMER0:
		// タイマーカウンタをインクリメントする (64fps なので 64カウントごとに１秒)
		sAppData.u32CtTimer0++;
		sAppData.u16CtTimer0++;

		// 重複チェックのタイムアウト処理
		if ((sAppData.u32CtTimer0 & 0xF) == 0) {
#ifndef USE_TOCONET_DUPCHK
			DUPCHK_bFind(&sDupChk_SerMsg, 0, NULL);
#endif
		}

		// 送信処理のタイムアウト処理
		if (sSerSeqTx.bWaitComplete) {
			if (u32TickCount_ms - sSerSeqTx.u32Tick > 1000) {
				// タイムアウトとして、処理を続行
				bool_t bSleep = sSerSeqTx.bSleepOnFinish;
				memset(&sSerSeqTx, 0, sizeof(sSerSeqTx));
				if (bSleep) {
					vSleep();
				}
			}
		}

		// ボタンの変化
		if (u32TickCount_ms - sAppData.u32BTM_Tick_LastChange > 100) // 前回変化から 100ms 以上経過している事
		{
			uint32 bmPorts, bmChanged;
			if (bBTM_GetState(&bmPorts, &bmChanged)) {
				// PORT_INPUT* の変化検出
				if (   (sAppData.u8uart_mode == UART_MODE_TRANSPARENT) // 送信モードが透過モード
					&& (bmChanged & PORT_INPUT_MASK)) // I1-I4の入力が有った
				{
					sAppData.u8PortNow =
								((bmPorts & (1UL << PORT_INPUT1)) ? 1 : 0)
							|	((bmPorts & (1UL << PORT_INPUT2)) ? 2 : 0)
							|	((bmPorts & (1UL << PORT_INPUT3)) ? 4 : 0)
							|	((bmPorts & (1UL << PORT_INPUT4)) ? 8 : 0);

					DBGOUT(5, LB"PortNow = %02X/%032b", sAppData.u8PortNow, bmPorts );

					if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
						// ショートアドレスの再設定
						sToCoNet_AppContext.u16ShortAddress =
								SERCMD_ADDR_CONV_TO_SHORT_ADDR_PARENT_IN_PAIR(sAppData.u8PortNow);
						ToCoNet_vRfConfig();

						// 相手方のアドレスを設定
						sAppData.u8AppLogicalId_Pair = sAppData.u8PortNow + 101;
						sAppData.u16ShAddr_Pair =
								SERCMD_ADDR_CONV_TO_SHORT_ADDR_CHILD_IN_PAIR(sAppData.u8PortNow);
					} else if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
						// ショートアドレスの再設定
						sToCoNet_AppContext.u16ShortAddress =
								SERCMD_ADDR_CONV_TO_SHORT_ADDR_CHILD_IN_PAIR(sAppData.u8PortNow);
						ToCoNet_vRfConfig();

						// 自分のアプリケーションアドレスを設定
						sAppData.u8AppLogicalId = sAppData.u8PortNow + 101;

						// 相手方のアドレスを設定
						sAppData.u8AppLogicalId_Pair = 0;
						sAppData.u16ShAddr_Pair =
								SERCMD_ADDR_CONV_TO_SHORT_ADDR_PARENT_IN_PAIR(sAppData.u8PortNow);
					} else {
						// do nothing. should not be here!
					}

				}

#ifdef USE_DIO_SLEEP
				// PORT_SLEEPの変化検出 (チャタリング期間が過ぎてから）
				if (bmChanged & (1UL << PORT_SLEEP)) {
					if (bmPorts & (1UL << PORT_SLEEP)) {
						vSleep();
					}
				}
#endif

				sAppData.u32BTM_Tick_LastChange = u32TickCount_ms;
			}
		}

		// シリアル画面制御のためのカウンタ
		if (sSerCmd_P3.bverbose && u16HoldUpdateScreen) {
			if (!(--u16HoldUpdateScreen)) {
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
		break;

	case E_AHI_DEVICE_TICK_TIMER:
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

	// 入力の設定
	for (i = 0; i < 4; i++) {
		vPortAsInput(au8PortTbl_DIn[i]);
		if (IS_APPCONF_OPT_DISABLE_PULL_UP_DI_PIN()) {
			vPortDisablePullup(au8PortTbl_DIn[i]);
		}
	}

	// 出力ピン
	vPortSetHi(PORT_UART_RTS); // RTS を HI にしておく
	vPortAsOutput(PORT_UART_RTS);

#ifdef DEBUG_RTS
	vPortAsOutput(DEBUG_RTS_LED);
#endif

	// 設定ピン (M1-M3)
#ifdef USE_MODE_PIN
	vPortAsInput(PORT_CONF1);
	vPortAsInput(PORT_CONF2);
	sAppData.u8Mode = (bPortRead(PORT_CONF1) | (bPortRead(PORT_CONF2) << 1)); // M1,M2 を設定用として読みだす。

	if (bPortRead(PORT_CONF1)) {
		vPortDisablePullup(PORT_CONF1); // LO 判定ならプルアップ停止しておく
	}
	if (bPortRead(PORT_CONF2)) {
		vPortDisablePullup(PORT_CONF2); // LO 判定ならプルアップ停止しておく
	}
#else
	sAppData.u8Mode = 0; // 0 決め打ち
#endif

	// 設定ピン (EX1,EX2)
#ifdef PORT_CONF_EX1
	vPortAsInput(PORT_CONF_EX1);
	if (bPortRead(PORT_CONF_EX1)) {
		vPortDisablePullup(PORT_CONF_EX1); // LO 判定ならプルアップ停止しておく
		sAppData.u8ModeEx |= 1;
	}
#endif

#ifdef PORT_CONF_EX2
	vPortAsInput(PORT_CONF_EX2);
	if (bPortRead(PORT_CONF_EX2)) {
		vPortDisablePullup(PORT_CONF_EX2); // LO 判定ならプルアップ停止しておく
		sAppData.u8ModeEx |= 2;
	}
#endif

	// IOピンの状態によって独自設定を行う
	if (sAppData.u8ModeEx & 0x01) { // バイナリモード強制
#if 1
		sAppData.u8uart_mode = UART_MODE_BINARY;
#else
# warning "FOR DEBUG!"
		sAppData.u8uart_mode = UART_MODE_ASCII;
#endif
	} else {
		sAppData.u8uart_mode = sAppData.sFlash.sData.u8uart_mode;
	}

	// スリープピン
	vPortAsInput(PORT_CONF3);
	if (IS_APPCONF_OPT_DISABLE_PULL_UP_SLEEP_PIN()) {
		vPortDisablePullup(PORT_CONF3); // プルアップ停止
	}

	// UART 設定
	{
#ifdef USE_BPS_PIN
		uint32 u32baud = UART_BAUD;
		vPortAsInput(PORT_BAUD);
		if (bPortRead(PORT_BAUD)) {
			vPortDisablePullup(PORT_BAUD); // LO 判定ならプルアップ停止しておく
			u32baud = UART_BAUD_SAFE;
		}
#else
		uint32 u32baud = UART_BAUD;
#endif
		tsUartOpt sUartOpt;

		memset(&sUartOpt, 0, sizeof(tsUartOpt));

		// BAUD ピンが GND になっている場合、かつフラッシュの設定が有効な場合は、設定値を採用する (v1.0.3)
#ifdef USE_BPS_PIN
		if (bPortRead(PORT_BAUD) || IS_APPCONF_OPT_UART_FORCE_SETTINGS()) {
			u32baud = sAppData.sFlash.sData.u32baud_safe;
			sUartOpt.bHwFlowEnabled = FALSE;
			sUartOpt.bParityEnabled = UART_PARITY_ENABLE;
			sUartOpt.u8ParityType = UART_PARITY_TYPE;
			sUartOpt.u8StopBit = UART_STOPBITS;

			// 設定されている場合は、設定値を採用する (v1.0.3)
			switch(sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_PARITY_MASK) {
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

			// ストップビット
			if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_STOPBIT_MASK) {
				sUartOpt.u8StopBit = E_AHI_UART_2_STOP_BITS;
			} else {
				sUartOpt.u8StopBit = E_AHI_UART_1_STOP_BIT;
			}

			// 7bitモード
			if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_WORDLEN_MASK) {
				sUartOpt.u8WordLen = 7;
			} else {
				sUartOpt.u8WordLen = 8;
			}

			vSerialInit(u32baud, &sUartOpt);
		} else
#endif
		{
			vSerialInit(u32baud, NULL);
		}

		// RTS ピンの設定
		vPortSetLo(PORT_UART_RTS);
	}

	// タイマの未使用ポートの解放（汎用ＩＯとして使用するため）
	vAHI_TimerFineGrainDIOControl(0x7F); // タイマー関連のピンを使わない

	// メイン(64fps)タイマ管理構造体の初期化
	memset(&sTimerApp, 0, sizeof(sTimerApp));

	// activate tick timers
	sTimerApp.u8Device = E_AHI_DEVICE_TIMER0;
	sTimerApp.u16Hz = 64;
	sTimerApp.u8PreScale = 4; // 15625ct@2^4
	vTimerConfig(&sTimerApp);
	vTimerStart(&sTimerApp);

	// button Manager (for Input)
#ifdef USE_DIO_SLEEP
	sAppData.sBTM_Config.bmPortMask =
			  (1UL << PORT_INPUT1) | (1UL << PORT_INPUT2)
			| (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4)
			| (1UL << PORT_SLEEP);
#else
	sAppData.sBTM_Config.bmPortMask =
			  (1UL << PORT_INPUT1) | (1UL << PORT_INPUT2)
			| (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4);
#endif
	sAppData.sBTM_Config.u16Tick_ms = 8;
	sAppData.sBTM_Config.u8MaxHistory = 5;
	sAppData.sBTM_Config.u8DeviceTimer = 0xFF; // TickTimer を流用する。
	sAppData.pr_BTM_handler = prBTM_InitExternal(&sAppData.sBTM_Config);
	vBTM_Enable();
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	static tsSerialPortSetup sSerPort;
	tsSerialPortSetup *pSerPort;

	if (IS_APPCONF_OPT_UART_SWAP_PORT()) {
		// 主ポートと副ポートを入れ替える
		u8dev_uart_master = UART_PORT_SLAVE;
		u8dev_uart_salve = UART_PORT_MASTER;
	} else {
		u8dev_uart_master = UART_PORT_MASTER;
		u8dev_uart_salve = UART_PORT_SLAVE;
	}

	/* 入出力のバッファ(FIFOキュー) */
	static uint8 au8SerialTxBuffer[UART_BUFFER_TX];
	static uint8 au8SerialRxBuffer[UART_BUFFER_RX];

	uint16 u16len_buff_tx;
	uint16 u16len_buff_rx;
	if (IS_APPCONF_OPT_UART_SLAVE_OUT()) {
		// 副出力する場合は１つのバッファを分割して利用する
		u16len_buff_tx = UART_BUFFER_TX / 2;
		u16len_buff_rx = UART_BUFFER_RX / 2;
	} else {
		u16len_buff_tx = UART_BUFFER_TX;
		u16len_buff_rx = UART_BUFFER_RX;
	}

	// 主ポートの UART の初期化
	pSerPort = &sSerPort;
	pSerPort->pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	pSerPort->pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	pSerPort->u32BaudRate = u32Baud;
	pSerPort->u16AHI_UART_RTS_LOW = 0xffff;
	pSerPort->u16AHI_UART_RTS_HIGH = 0xffff;
	pSerPort->u16SerialRxQueueSize = u16len_buff_rx;
	pSerPort->u16SerialTxQueueSize = u16len_buff_tx;
	pSerPort->u8SerialPort = u8dev_uart_master;
	pSerPort->u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(pSerPort, pUartOpt);

	if (IS_APPCONF_OPT_UART_SLAVE_OUT()) {
		// 副ポートへの出力する場合、UART を初期化する
		static tsSerialPortSetup sSerPort_Slave;
		pSerPort = &sSerPort_Slave;

		/* Initialise the serial port to be used for debug output */
		pSerPort->pu8SerialRxQueueBuffer = au8SerialRxBuffer + u16len_buff_rx;
		pSerPort->pu8SerialTxQueueBuffer = au8SerialTxBuffer + u16len_buff_tx;
		pSerPort->u32BaudRate = u32Baud;
		pSerPort->u16AHI_UART_RTS_LOW = 0xffff;
		pSerPort->u16AHI_UART_RTS_HIGH = 0xffff;
		pSerPort->u16SerialRxQueueSize = u16len_buff_rx;
		pSerPort->u16SerialTxQueueSize = u16len_buff_tx;
		pSerPort->u8SerialPort = u8dev_uart_salve;
		pSerPort->u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
		SERIAL_vInitEx(pSerPort, pUartOpt);

		sSerStream.bPutChar = SERIAL_bTxCharDuo; // 主ポートと副ポート両方に出力する
	} else {
		sSerStream.bPutChar = SERIAL_bTxChar;
	}

	sSerStream.u8Device = u8dev_uart_master;

	// メモリの初期化
	INPSTR_vInit(&sSerInpStr, &sSerStream);
	memset(&sSerCmd_P3, 0x00, sizeof(sSerCmd));
	memset(&sSerSeqTx, 0x00, sizeof(sSerSeqTx));
	memset(&sSerSeqRx, 0x00, sizeof(sSerSeqRx));

	memset(au8TxCbId_to_RespID, 0x00, sizeof(au8TxCbId_to_RespID));

	// シリアルコマンド解釈部分の初期化
	switch(sAppData.u8uart_mode) {
	case UART_MODE_CHAT:
		SerCmdChat_vInit(&sSerCmd, au8SerBuffTx, sizeof(au8SerBuffTx));
		SerCmdChat_vInit(&sSerCmdOut, au8SerBuffRx, SIZEOF_au8SerBuffRx);
		SerCmdChat_vInit(&sSerCmdTemp, au8SerOutBuff, sizeof(au8SerOutBuff));
		pf_vProcessInputByte = vProcessInputByte_Chat;
		pf_vProcessInputByte_Slave = NULL;
		u16SerTimeout = 0;
		break;
	case UART_MODE_CHAT_NO_PROMPT:
		SerCmdTimeout_vInit(&sSerCmd, au8SerBuffTx, sizeof(au8SerBuffTx));
		SerCmdTimeout_vInit(&sSerCmdOut, au8SerBuffRx, SIZEOF_au8SerBuffRx);
		SerCmdTimeout_vInit(&sSerCmdTemp, au8SerOutBuff, sizeof(au8SerOutBuff));
		pf_vProcessInputByte = vProcessInputByte_Chat;
		pf_vProcessInputByte_Slave = NULL;
		u16SerTimeout = sAppData.sFlash.sData.u8uart_txtrig_delay; // UART 未入力区間を基にした送信トリガー
		break;
	case UART_MODE_BINARY:
		SerCmdBinary_vInit(&sSerCmd, au8SerBuffTx, sizeof(au8SerBuffTx));
		SerCmdBinary_vInit(&sSerCmdOut, au8SerBuffRx, SIZEOF_au8SerBuffRx);
		SerCmdBinary_vInit(&sSerCmdTemp, au8SerOutBuff, sizeof(au8SerOutBuff));
		pf_vProcessInputByte = vProcessInputByte_FormatCmd;
		u16SerTimeout = 1000;
		break;
	case UART_MODE_ASCII:
		SerCmdAscii_vInit(&sSerCmd, au8SerBuffTx, sizeof(au8SerBuffTx));
		SerCmdAscii_vInit(&sSerCmdOut, au8SerBuffRx, SIZEOF_au8SerBuffRx);
		SerCmdAscii_vInit(&sSerCmdTemp, au8SerOutBuff, sizeof(au8SerOutBuff));
		pf_vProcessInputByte = vProcessInputByte_FormatCmd;
		pf_vProcessInputByte_Slave = vProcessInputByte_FormatCmd;
		u16SerTimeout = 1000;
		break;
	case UART_MODE_TRANSPARENT:
		SerCmdAscii_vInit(&sSerCmd, au8SerBuffTx, sizeof(au8SerBuffTx)); // 使用しないはずだが
		SerCmdAscii_vInit(&sSerCmdOut, au8SerBuffRx, SIZEOF_au8SerBuffRx); // 使用しないはずだが
		SerCmdAscii_vInit(&sSerCmdTemp, au8SerOutBuff, sizeof(au8SerOutBuff)); // 使用しないはずだが
		pf_vProcessInputByte = vProcessInputByte_Transparent;
		pf_vProcessInputByte_Slave = NULL;
		u16SerTimeout = 1000; // 使用しないはずだが
		break;
	}

	if (IS_APPCONF_OPT_UART_SLAVE_OUT()) {
		static uint8 au8cmdbuf[SERCMD_MAXPAYLOAD+32];

		switch(sAppData.u8uart_mode) {
		case UART_MODE_CHAT:
			SerCmdChat_vInit(&sSerCmd_Slave, au8cmdbuf, sizeof(au8cmdbuf));
			pf_vProcessInputByte_Slave = vProcessInputByte_Chat;
			break;
		case UART_MODE_CHAT_NO_PROMPT:
			SerCmdTimeout_vInit(&sSerCmd_Slave, au8cmdbuf, sizeof(au8cmdbuf));
			pf_vProcessInputByte_Slave = vProcessInputByte_Chat;
			break;
		case UART_MODE_BINARY:
			SerCmdBinary_vInit(&sSerCmd_Slave, au8cmdbuf, sizeof(au8cmdbuf));
			pf_vProcessInputByte_Slave = vProcessInputByte_FormatCmd;
			break;
		case UART_MODE_ASCII:
			SerCmdAscii_vInit(&sSerCmd_Slave, au8cmdbuf, sizeof(au8cmdbuf));
			pf_vProcessInputByte_Slave = vProcessInputByte_FormatCmd;
			break;
		case UART_MODE_TRANSPARENT:
			pf_vProcessInputByte_Slave = NULL;
			break;
		}

		sSerCmd_Slave.u16timeout = u16SerTimeout;
	}

	sSerCmd.u16timeout = u16SerTimeout; // デフォルトのタイムアウト
}

/**
 * シリアルポートからの１バイト引っ張るべきか、ブロックして待っておくかを判定する。
 * （送信中には次のキューの処理が出来ないため）
 *
 * @param u8dev_uart
 * @return キューから取り出した場合は文字列 取り出さない場合は(-1)
 */
static int16 i16GetSerialQueue(uint8 u8dev_uart) {
	bool_t bCond = TRUE;

	if (sAppData.u8uart_mode == UART_MODE_TRANSPARENT) {
		// 透過モード
		if (!IS_APPCONF_OPT_TX_TRIGGER_CHAR()) {
			if (sSerCmd.u16len >= SERCMD_SER_PKTLEN_MINIMUM) {
				bCond = FALSE;
			}
		} else {
			if (bPendingTxOnTransparent) {
				bCond = FALSE;
			}
		}
	} else {
		// 書式モード
		if (sSerSeqTx.bWaitComplete // 送信中はキューの取り出しをブロック
				&& !IS_APPCONF_OPT_TX_NEWER() // 最新データ優先の場合は読み込みブロックをしない
		) {
			bCond = FALSE;
		}
	}

	if (bCond) {
		return SERIAL_i16RxChar(u8dev_uart);
	} else {
		DBGOUT(5, "<B>");
		return (-1);
	}
}

/** @ingroup MASTER
 * シリアルポートからの入力を処理する。
 * @param i16CharExt アプリケーション中から、本関数を呼びたい時に入力系列をパラメータ渡しする（ボタンにUARTと共通の機能を割りつけたい場合など）
 */
void vHandleSerialInput() {
	/* RTS の処理 */
	uint16 u16RxCt = SERIAL_u16RxQueueCount(u8dev_uart_master);

#ifndef DEBUG_RTS
	static bool_t bRST = FALSE, bRST_Prev = FALSE;
	if (sAppData.bNwkUp == FALSE) {
		// ネットワーク未接続時は HI(=受信禁止) にしておく
		bRST = FALSE;
	} else
	if (sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT) {
		// CHAT_NO_PROMPT では送信中の入力禁止とする
		if (sSerSeqTx.bWaitComplete) {
			bRST = FALSE;
		} else {
			bRST = TRUE;
		}
	} else {
		if (u16RxCt> UART_BUFFER_RX_LIMIT_STOP) {
			// RX キューがリミット以上になったので RTS=HI (受信不可) を設定する
			bRST = FALSE;
		} else if (u16RxCt < UART_BUFFER_RX_LIMIT_START){
			// RX キューがリミット以下になったので RTS=LO (受信可) を設定する
			bRST = TRUE;
		}
	}

	// 念のため毎回セットする
	vPortSet_TrueAsLo(PORT_UART_RTS, bRST);
	if (bRST_Prev != bRST) {
		DBGOUT(4, "<%c>", bRST ? 'L' : 'H');
	}
	bRST_Prev = bRST;

#else
	static bool_t bStat = TRUE;

	if (u16RxCt> UART_BUFFER_RX_LIMIT_STOP) {
		// RX キューがリミット以上になったので RTS=HI (受信不可) を設定する
		vPortSetHi(PORT_UART_RTS);

		if (bStat) {
			vPortSetLo(DEBUG_RTS_LED);
			DBGOUT(0, ">");
		}
		bStat = FALSE;

	} else if (u16RxCt < UART_BUFFER_RX_LIMIT_START){
		// RX キューがリミット以下になったので RTS=LO (受信可) を設定する
		vPortSetLo(PORT_UART_RTS);

		if (!bStat) {
			vPortSetHi(DEBUG_RTS_LED);
			DBGOUT(0, "<");
		}
		bStat = TRUE;
	}
#endif

	/* 主UARTポートの処理 */
	while (!SERIAL_bRxQueueEmpty(u8dev_uart_master)) {
		int16 i16Char = i16GetSerialQueue(u8dev_uart_master);

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
				uint8 u8res;
				u8res = SerCmdPlus3_u8Parse(&sSerCmd_P3, (uint8)i16Char);

				if (u8res != E_SERCMD_PLUS3_EMPTY) {
					if (u8res == E_SERCMD_PLUS3_VERBOSE_ON) {
						// verbose モードの判定があった
						vSerUpdateScreen();
						sSerCmd.u16timeout = 0;
					}

					if (u8res == E_SERCMD_PLUS3_VERBOSE_OFF) {
						vfPrintf(&sSerStream, "!INF EXIT INTERACTIVE MODE."LB);
						sSerCmd.u16timeout = u16SerTimeout;
					}

					// still waiting for bytes.
					//continue;
				} else {
					; // コマンド解釈モードではない
				}
			}

			// Verbose モードのときは、シングルコマンドを取り扱う
			if (sSerCmd_P3.bverbose) {
				if (sAppData.u8uart_mode == UART_MODE_ASCII || sAppData.u8uart_mode == UART_MODE_BINARY) {
					// コマンドの解釈と対応の処理
					(*pf_vProcessInputByte)(&sSerCmd, i16Char);

					// エコーバック出力
					if (sSerCmd.u8state != E_SERCMD_EMPTY) {
						if (sAppData.u8uart_mode == UART_MODE_ASCII) {
							V_PUTCHAR(i16Char);
						} else if (sAppData.u8uart_mode == UART_MODE_BINARY) {
							/* 文字の16進変換 */
							uint8 c = (i16Char >> 4) & 0xF;
							V_PUTCHAR(c < 0xA ? '0' + c : 'A' + c - 10);
							c = i16Char & 0xF;
							V_PUTCHAR(c < 0xA ? '0' + c : 'A' + c - 10);
						}
					}
				} else {
					sSerCmd.u8state = E_SERCMD_EMPTY;
				}

				if (sSerCmd.u8state == E_SERCMD_EMPTY) {
					// 書式入出力でなければ、１バイトコマンド
					vProcessInputByte(i16Char);
				}
			} else {
				if (pf_vProcessInputByte) {
					pf_vProcessInputByte(&sSerCmd, i16Char);
				}
			}
		} else {
			break;
		}
	}

	/* UART の入力タイムアウトを条件として送信する場合 */
	if (sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT && sSerCmd.bComplete) {
		// CHAT_NO_PROMPT ではタイムアウトによる送信処理を行う
		if (sSerCmd.bComplete(&sSerCmd)) {
			pf_vProcessInputByte(&sSerCmd, 0xFFFF);
		}
	}

	/* 副UARTポート */
	if (IS_APPCONF_OPT_UART_SLAVE_OUT()) {
		// 副UARTの入力を処理する。インタラクティブモードだけ処理。
		while (!SERIAL_bRxQueueEmpty(u8dev_uart_salve)) {
			int16 i16Char = i16GetSerialQueue(u8dev_uart_salve);

			// 入力のチェック
			if (i16Char >= 0 && i16Char <= 0xFF) {
				// 入力コマンドの処理
				if (sSerCmd_P3.bverbose && INPSTR_bActive(&sSerInpStr)) {
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

				// コマンド書式の系列解釈、および verbose モードの判定
				{
					uint8 u8res;
					u8res = SerCmdPlus3_u8Parse(&sSerCmd_P3, (uint8)i16Char);

					if (u8res != E_SERCMD_PLUS3_EMPTY) {
						if (u8res == E_SERCMD_PLUS3_VERBOSE_ON) {
							// verbose モードの判定があった
							vSerUpdateScreen();
						}

						if (u8res == E_SERCMD_PLUS3_VERBOSE_OFF) {
							vfPrintf(&sSerStream, "!INF EXIT INTERACTIVE MODE (SUB_PORT). "LB);
						}
					} else {
						; // コマンド解釈モードではない
					}
				}

				// インタラクティブモードのシングルコマンドの処理
				if (sSerCmd_P3.bverbose) {
					vProcessInputByte(i16Char);
				} else {
					if (pf_vProcessInputByte_Slave) {
						pf_vProcessInputByte_Slave(&sSerCmd_Slave, i16Char);
					}
				}
			} else {
				break;
			}
		}

		/* UART の入力タイムアウトを条件として送信する場合 */
		if (sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT && sSerCmd_Slave.bComplete) {
			// CHAT_NO_PROMPT ではタイムアウトによる送信処理を行う
			if (sSerCmd_Slave.bComplete(&sSerCmd_Slave)) {
				pf_vProcessInputByte_Slave(&sSerCmd_Slave, 0xFFFF);
			}
		}
	}
}

/** @ingroup MASTER
 * １バイト入力コマンドの処理
 * @param u8Byte 入力バイト
 */
static void vProcessInputByte_Transparent(tsSerCmd_Context *pSerCmd, uint16 u16Byte) {
	bool_t bData = (u16Byte <= 0xFF);
	uint8 u8Byte = u16Byte & 0xFF;

	if (!bData) return;

	if (!IS_APPCONF_OPT_TX_TRIGGER_CHAR()) {
		// sSerCmd の au8data と u16len のみ利用している。
		// 送信は定期タイマーイベントで実行。
		pSerCmd->au8data[pSerCmd->u16len++] = u8Byte;
	} else {
		pSerCmd->au8data[pSerCmd->u16len++] = u8Byte;

		if (u8Byte == sAppData.sFlash.sData.u16uart_lnsep
				|| pSerCmd->u16len >= SERCMD_SER_PKTLEN_MINIMUM) {
			// 透過モードで 0x0D (CR) で送信するモード
			// （通常は TICKTIMER のイベントハンドラで実行）
			if (!sSerSeqTx.bWaitComplete) { // 送信中でない事
				i16Transmit_Transparent();
			} else {
				if (IS_APPCONF_OPT_TX_NEWER()) { // 送信中の場合
					// 系列を破棄する (送信完了後に来たより新しい系列を優先する)
					pSerCmd->u16len = 0;
				} else {
					// 送信をペンディングにする
					bPendingTxOnTransparent = TRUE;
				}
			}
		}
	}
}

/** @ingroup MASTER
 * １バイト入力コマンドの処理
 * @param u16Byte 入力バイト (0xFFFF なら入力無しのチェック)
 */
static void vProcessInputByte_FormatCmd(tsSerCmd_Context *pSerCmd, uint16 u16Byte) {
	bool_t bData = (u16Byte <= 0xFF);
	uint8 u8Byte = u16Byte & 0xFF;

	// コマンド書式の系列解釈、および verbose モードの判定
	uint8 u8res;

	if (bData) {
		u8res = pSerCmd->u8Parse(pSerCmd, (uint8)u8Byte);
	} else {
		u8res = pSerCmd->u8state;
	}

	// 完了時の処理
	if (u8res == E_SERCMD_COMPLETE || u8res == E_SERCMD_CHECKSUM_ERROR) {
		// 解釈完了

		if (u8res == E_SERCMD_CHECKSUM_ERROR) {
			// command complete, but CRC error
			V_PRINT(LB "!INF CHSUM_ERR? (might be %02X)" LB, pSerCmd->u16cksum);
		}

		if (u8res == E_SERCMD_COMPLETE) {
			// PAYLOAD が大きすぎる場合は新しくスタート
			//if (pSerCmd->u16len > SERCMD_MAXPAYLOAD) {
			//	pSerCmd->u8state = E_SERCMD_EMPTY;
			//	return;
			//}

			// process command
			vProcessSerialCmd(pSerCmd);
		}
	}
}


/** @ingroup MASTER
 * １バイト入力コマンドの処理 (UART_MODE_CHAT か UART_MODE_CHAT_NO_PROMPT前提)
 * @param u16Byte 入力バイト (0xFFFF なら入力無しのチェック)
 */
static void vProcessInputByte_Chat(tsSerCmd_Context *pSerCmd, uint16 u16Byte) {
	bool_t bData = (u16Byte <= 0xFF);
	uint8 u8Byte = u16Byte & 0xFF;

	// チャットモードの制御コード
	if (bData && sAppData.u8uart_mode != UART_MODE_CHAT_NO_PROMPT) {
		if (u8Byte == 0x0c) { // Ctrl+L
			vfPrintf(&sSerStream, "%c[2J%c[H", 27, 27); // CLEAR SCREEN
			vSerChatPrompt();
			return;
		}
	}

	// コマンド書式の系列解釈、および verbose モードの判定
	uint8 u8Prev = pSerCmd->u8state;; // cancel 表示のために直前の状態を保存しておく
	uint8 u8res;
	bool_t bPrompt = FALSE;

	if (bData) {
		u8res = pSerCmd->u8Parse(pSerCmd, (uint8)u8Byte);

		// 区切り文字の判定
		if (pSerCmd->u8state != E_SERCMD_ERROR && sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT) {
			bool_t bComp = FALSE;

			if (pSerCmd->u16len >= SERCMD_SER_PKTLEN_MINIMUM) {
				// パケットサイズ超えた時点で送信条件とする
				bComp = TRUE;
			} else
			if (IS_APPCONF_OPT_TX_TRIGGER_CHAR()) {
				// 区切り文字による送信判定
				if (u8Byte == sAppData.sFlash.sData.u16uart_lnsep) { // 区切り文字の設定かつ区切り文字
					if (sAppData.sFlash.sData.u8uart_lnsep_minpkt && (pSerCmd->u16len < sAppData.sFlash.sData.u8uart_lnsep_minpkt)) {
						bComp = FALSE; // パケット長の設定があるときにその長さに達していない
					} else {
						bComp = TRUE;
					}
				}
			} else {
				// その他、最小データによる判定
				if (sAppData.sFlash.sData.u8uart_lnsep_minpkt && pSerCmd->u16len >= sAppData.sFlash.sData.u8uart_lnsep_minpkt) {
					bComp = TRUE;
				}
			}

			// 送信判定条件を得て送信する
			if (bComp) {
				u8res = pSerCmd->u8state = E_SERCMD_COMPLETE;
			}
		}
	} else {
		u8res = pSerCmd->u8state;
	}

	// 完了時の処理
	if (u8res == E_SERCMD_COMPLETE) {
		if (sAppData.u8uart_mode == UART_MODE_CHAT) {
			// チャットモードのときは末尾にハンドル名を付記する
			// 書式：
			//   {入力テキスト} [0x0][0x0] <= ハンドル名未設定
			//   {入力テキスト} [0x0] {ハンドル名} [0x0]
			pSerCmd->au8data[pSerCmd->u16len] = 0x00; // 最初は０
			pSerCmd->u16len++;

			uint8 *p = sAppData.sFlash.sData.au8ChatHandleName;
			while(*p) {
				pSerCmd->au8data[pSerCmd->u16len] = *p;
				pSerCmd->u16len++;
				p++;
			}

			pSerCmd->au8data[pSerCmd->u16len] = 0x00; // 末尾も０
			pSerCmd->u16len++;
		}

		// PAYLOAD が大きすぎる場合は新しくスタート
		if (pSerCmd->u16len > SERCMD_MAXPAYLOAD || pSerCmd->u16len > pSerCmd->u16maxlen) {
			pSerCmd->u8state = E_SERCMD_EMPTY;

			if (sAppData.u8uart_mode != UART_MODE_CHAT_NO_PROMPT) {
				vfPrintf(&sSerStream, "(err:too long)");
			}

			return;
		}

		// process command
		// チャットモードの場合は、系列をそのまま送信する。
		if (pSerCmd->u16len) {
			i16TransmitSerMsg(
				pSerCmd->au8data, pSerCmd->u16len, NULL,
				ToCoNet_u32GetSerial(),
				sAppData.u8AppLogicalId,
				TOCONET_NWK_ADDR_NULL,
				LOGICAL_ID_BROADCAST,
				FALSE,
				sAppData.u8UartReqNum,
				sAppData.u8UartReqNum,
				0);
			sAppData.u8UartReqNum++;
			bPrompt = TRUE;
		}

		// EMPTY に戻しておく
		pSerCmd->u8state = E_SERCMD_EMPTY;
	}

	// チャットモードの後出力処理
	if (bData && sAppData.u8uart_mode != UART_MODE_CHAT_NO_PROMPT) {
		// チャットモードのエコーバック
		if (u8res != E_SERCMD_EMPTY // 何か入力中
			&& u8res < 0x80 // エラー検出でもない
		) {
			if (u8Byte == 0x08 || u8Byte == 0x7F) {
				vPutChar(&sSerStream, 0x08);
			} else {
				vPutChar(&sSerStream, u8Byte);
			}
		}

		// エラー
		if (u8res > 0x80) {
			vfPrintf(&sSerStream, "(err)");
		}

		// EMPTY 状態での改行が来たらプロンプト
		if (u8res == E_SERCMD_EMPTY && u8Byte == 0x0d) {
			bPrompt = TRUE;
		}

		// 直前が入力処理中で、EMPTY に変化した
		if (u8Prev != u8res && u8res == E_SERCMD_EMPTY && u8Prev < 0x80) {
			bPrompt = TRUE;
			vfPrintf(&sSerStream, "%c(canceled)", 0x08); // BS を追加しておく
		}

		// 状態が確定した
		if (u8res >= 0x80) {
			bPrompt = TRUE;
		}

		if (bPrompt) {
			vSerChatPrompt();
		}
	}
}

/** @ingroup MASTER
 * シリアルから入力されたコマンド形式の電文を処理する。
 * @param pSer
 */
static void vProcessSerialCmd(tsSerCmd_Context *pSer) {
	uint8 u8addr; // 送信先論理アドレス

	uint8 u8cmd;

	uint8 *p = pSer->au8data;
	uint8 *p_end;
	p_end = p + pSer->u16len; // the end points 1 byte after the data end.

	// COMMON FORMAT
	OCTET(u8addr); // [1] OCTET : Address information
	OCTET(u8cmd); // [1] OCTET : Command information

	DBGOUT(1, "* UARTCMD ln=%d cmd=%02x req=%02x %02x%02x%02x%02x..." LB,
			pSer->u16len,
			u8addr,
			u8cmd,
			*p,
			*(p+1),
			*(p+2),
			*(p+3)
			);

	if (u8addr == 0xDB) {
		// ローカルコマンド
		switch(u8cmd) {
		case SERCMD_ID_ACK:
			vSerResp_Ack(TRUE);
			break;

		case SERCMD_ID_MODULE_CONTROL:
			if (p < p_end) {
				uint8 u8ctl = G_OCTET();

				if (u8ctl == SERCMD_ID_MODULE_CONTROL_RELEASE_SILENT) {
					if (sAppData.bSilent) {
						ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
					}

					vSerResp_Silent();
				}

				if (u8ctl == SERCMD_ID_MODULE_CONTROL_INFORM_SILENT) {
					vSerResp_Silent();
				}
			}
			break;

		case SERCMD_ID_GET_MODULE_ADDRESS:
			vSerResp_GetModuleAddress();
			break;

		case SERCMD_ID_SET_MODULE_SETTING:
			if (bSerCmd_SetModuleSetting(p, p_end - p)) {
				vSerResp_GetModuleSetting(*p);
			} else {
				vSerResp_GetModuleSetting(0xFF);
			}
			break;

		case SERCMD_ID_GET_MODULE_SETTING:
			vSerResp_GetModuleSetting(*p);
			break;

		case SERCMD_ID_SAVE_AND_RESET:
			vConfig_SaveAndReset();
			break;

		case SERCMD_ID_DO_FACTORY_DEFAULT:
			bFlash_Erase(FLASH_SECTOR_NUMBER - 1); // フラッシュ領域を削除
			vWait(1000000);
			vAHI_SwReset();
			break;

		case SERCMD_ID_RESET:
			vAHI_SwReset();
			break;
		}
	} else if (u8cmd == SERCMD_ID_TRANSMIT_EX) {
		vProcessSerialCmd_TransmitEx(pSer);
	} else {
		if (pSer->u16len >= 3) {
			i16TransmitSerMsg(
				pSer->au8data + 2, pSer->u16len - 2, NULL,
				ToCoNet_u32GetSerial(),
				sAppData.u8AppLogicalId, // 送信元論理ID
				TOCONET_NWK_ADDR_NULL,
				pSer->au8data[0], // あて先論理ID
				FALSE,
				sAppData.u8UartReqNum | 0x80,
				sAppData.u8UartReqNum | 0x80,
				pSer->au8data[1] // ２バイト目コマンド
				);
			sAppData.u8UartReqNum++;
		}
	}
}

/** @ingroup MASTER
 * シリアルから入力されたコマンド形式の電文を処理する。
 * @param pSer
 */
static void vProcessSerialCmd_TransmitEx(tsSerCmd_Context *pSer) {
	uint8 *p = pSer->au8data;
	uint8 *p_end = pSer->au8data + pSer->u16len;

	tsTxDataApp sTx; // 送信オプションのテンプレート
	memset(&sTx, 0, sizeof(tsTxDataApp));

	sTx.u8Retry = 0xFF;
	sTx.u16RetryDur = 0xFFFF;
	sTx.u16DelayMin = 0;
	sTx.u16DelayMax = 0;

	uint8 u8addr = G_OCTET();
	uint8 u8cmd = G_OCTET();

	uint8 u8resp = G_OCTET();

	// 拡張アドレス
	uint32 u32AddrDst = TOCONET_NWK_ADDR_NULL;
	if (u8addr == LOGICAL_ID_EXTENDED_ADDRESS) {
		u32AddrDst = G_BE_DWORD();
	}

	// コマンドオプション
	for (;;) {
		uint8 u8Opt = G_OCTET();
		if (u8Opt == TRANSMIT_EX_OPT_TERM) break;

		switch(u8Opt) {
		case TRANSMIT_EX_OPT_SET_MAC_ACK:
			if (	u8addr != LOGICAL_ID_CHILDREN
				&&	(u32AddrDst != TOCONET_MAC_ADDR_BROADCAST && u32AddrDst != TOCONET_NWK_ADDR_BROADCAST)) {
				sTx.bAckReq = TRUE;
				if (u32AddrDst == TOCONET_NWK_ADDR_NULL) {
					// １バイトアドレス利用時で ACK 要求が有る場合は、相手先の MAC アドレスを指定する
					u32AddrDst = SERCMD_ADDR_CONV_TO_SHORT_ADDR(u8addr);
				}
			}
			break;

		case TRANSMIT_EX_OPT_SET_APP_RETRY:
			sTx.u8Retry = G_OCTET();
			break;

		case TRANSMIT_EX_OPT_SET_DELAY_MIN_ms:
			sTx.u16DelayMin = G_BE_WORD();
			break;

		case TRANSMIT_EX_OPT_SET_DELAY_MAX_ms:
			sTx.u16DelayMax = G_BE_WORD();
			break;

		case TRANSMIT_EX_OPT_SET_RETRY_DUR_ms:
			sTx.u16RetryDur = G_BE_WORD();
			break;

		case TRANSMIT_EX_OPT_SET_PARALLEL_TRANSMIT: // 併行送信
			sTx.auData[TRANSMIT_EX_OPT_SET_PARALLEL_TRANSMIT] = TRUE;
			break;

		case TRANSMIT_EX_OPT_SET_NO_RESPONSE: // 応答を返さない
			sTx.u8CbId |= CBID_MASK_SILENT;
			break;

		case TRANSMIT_EX_OPT_SET_SLEEP_AFTER_TRANSMIT: // 送信後スリープする
			sTx.auData[TRANSMIT_EX_OPT_SET_SLEEP_AFTER_TRANSMIT] = TRUE;
			break;
		}
	}


	// デバッグメッセージ
	{
		DBGOUT(1, "* TxEx: Dst:%02x,%08x Ac:%d Re:%d Di:%d Da:%d Dr:%d Ln:%d %02x%02x%02x%02x",
				u8addr,
				u32AddrDst,
				sTx.bAckReq,
				sTx.u8Retry,
				sTx.u16DelayMin,
				sTx.u16DelayMax,
				sTx.u16RetryDur,
				p_end - p,
				p[0], p[1], p[2], p[3]
		);

		int i;
		for (i = 0; i < 8 && p + i < p_end; i++) {
			DBGOUT(1, "%02x", p[i]);
		}
		if (i == 8) DBGOUT(1, "...");
	}

	if (p < p_end) {
		uint16 u16len = p_end - p; // ペイロード長

		/// 未設定オプションを書き換え
		// アプリケーション再送回数を指定
		if (sTx.u8Retry == 0xFF) {
			sTx.u8Retry = sTx.bAckReq ? 0 : DEFAULT_TX_FFFF_COUNT;
		}

		// アプリケーション再送間隔の指定
		if (sTx.u16RetryDur == 0xFFFF) {
			uint16 u16pkts = (u16len - 1) / SERCMD_SER_PKTLEN + 1; // パケット数の計算：1...80->1, 81...160->2, ...
			sTx.u16RetryDur = u16pkts * 10; // デフォルトはパケット数x10ms
		}

		// 送信実行
		if (i16TransmitSerMsg(p, u16len, &sTx,
				ToCoNet_u32GetSerial(), sAppData.u8AppLogicalId,
				u32AddrDst, u8addr,
				FALSE, sAppData.u8UartReqNum++, u8resp, u8cmd) == -1) {

			// エラー応答の出力
			if (!IS_APPCONF_OPT_NO_TX_RESULT()) {
				vSerResp_TxEx(u8resp, 0x0);
			}
		}
	}

	return;
}

/** @ingroup MASTER
 * シリアルメッセージの送信要求。
 * パケットを分割して送信する。
 *
 *  - Packet 構造
 *   - [1] OCTET    : 識別ヘッダ(APP ID より生成), 下１ビットが1の時は中継済み
 *   - [1] OCTET    : プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - [1] OCTET    : 応答ID(外部から指定される値、任意に使用出来るデータの一つ)
 *   - [1] OCTET    : 送信元、簡易アドレス
 *   - [4] BE_DWORD : 送信元、拡張アドレス
 *   - [1] OCTET    : 宛先、簡易アドレス
 *   - [4] BE_DWORD : 宛先、拡張アドレス
 *   - [2] BE_WORD  : 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - [1] OCTET    : パケット群の識別ID
 *   - [1] OCTET    : パケット番号 (0...total-1) / パケット数(total)
 *   - [1] OCTET    : 拡張情報
 *
 * @param p 送信データ（実際に送信したいデータ部で各種ヘッダは含まない）
 * @param u16len データ長
 * @param pTxTemplate 送信オプションを格納した構造体（送信用の構造体を流用）
 * @param u32AddrSrc 送信元、拡張アドレス
 * @param u8AddrSrc 送信元、簡易アドレス
 * @param u32AddrDst 宛先、拡張アドレス
 * @param u8AddrDst 宛先、簡易アドレス
 * @param bRelay 中継フラグ、TRUEなら中継ビットを立てた状態で送信する
 * @param u8Req 識別ID、パケット群を識別するための内部的に利用するID。重複の除去などに用いる。
 * @param u8RspId 応答ID、外部向けの識別ID。成功失敗などの応答などにも用いる。
 * @return
 */
static int16 i16TransmitSerMsg(uint8 *p, uint16 u16len, tsTxDataApp *pTxTemplate,
		uint32 u32AddrSrc, uint8 u8AddrSrc, uint32 u32AddrDst, uint8 u8AddrDst,
		uint8 u8Relay, uint8 u8Req, uint8 u8RspId, uint8 u8Opt) {

	if(sAppData.bSilent) return -1;

	// 処理中のチェック（処理中なら送信せず失敗）
	if (sSerSeqTx.bWaitComplete) {
		DBGOUT(4,"<S>");
		return -1;
	}

	// パケットを分割して送信する。
	tsTxDataApp sTx;
	if (pTxTemplate == NULL) {
		memset(&sTx, 0, sizeof(sTx));
	} else {
		memcpy(&sTx, pTxTemplate, sizeof(sTx));
	}
	uint8 *q; // for S_??? macros

	// sSerSeqTx は分割パケットの管理構造体
	memset(&sSerSeqTx, 0, sizeof(sSerSeqTx)); // ゼロクリアしておく
	sSerSeqTx.u8IdSender = sAppData.u8AppLogicalId;

	if( IS_APPCONF_OPT_FORMAT_TO_NOPROMPT() && (au8UartModeToTxCmdId[sAppData.u8uart_mode] == 1 || au8UartModeToTxCmdId[sAppData.u8uart_mode] == 3 )){
		if(IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId) && u8AddrDst == LOGICAL_ID_BROADCAST){
			sSerSeqTx.u8IdReceiver = 0x00;		// 自分が子機だったら親機にする
		}else{
			sSerSeqTx.u8IdReceiver = u8AddrDst;		// 自分が親機だったら指定のIDにする
		}
	}else{
		sSerSeqTx.u8IdReceiver = u8AddrDst;
	}

	sSerSeqTx.u8PktNum = (u16len - 1) / SERCMD_SER_PKTLEN + 1; // 1...80->1, 81...160->2, ...
	sSerSeqTx.u16DataLen = u16len;

	if (u16len > SERCMD_MAXPAYLOAD) {
		return -1; // ペイロードが大きすぎる
	}

	sSerSeqTx.u8RespID = u8RspId;
	sSerSeqTx.u8Seq = sAppData.u8UartSeqNext; // パケットのシーケンス番号（アプリでは使用しない）
	sAppData.u8UartSeqNext = (sSerSeqTx.u8Seq + sSerSeqTx.u8PktNum) & CBID_MASK_BASE; // 次のシーケンス番号（予め計算しておく）
	sSerSeqTx.u8ReqNum = u8Req; // パケットの要求番号（この番号で送信系列を弁別する）

	sSerSeqTx.u32Tick = u32TickCount_ms;
	if (sTx.auData[TRANSMIT_EX_OPT_SET_PARALLEL_TRANSMIT] && sSerSeqTx.u8PktNum == 1) {
		// 併行送信時は bWaitComplete の条件を立てない
		sSerSeqTx.bWaitComplete = FALSE;
	} else {
		sSerSeqTx.bWaitComplete = TRUE;
	}

	// 送信後スリープするオプション
	if (sTx.auData[TRANSMIT_EX_OPT_SET_SLEEP_AFTER_TRANSMIT]) {
		sSerSeqTx.bSleepOnFinish = TRUE;
	}

	// 送信後応答しない
	bool_t bNoResponse = FALSE;
	if (sTx.u8CbId & CBID_MASK_SILENT) {
		bNoResponse = TRUE;
	}

	memset(sSerSeqTx.bPktStatus, 0, sizeof(sSerSeqTx.bPktStatus));

	DBGOUT(3, "* >>> Transmit(req=%d,cb=0x02X) Tick=%d <<<" LB,
			sSerSeqTx.u8ReqNum, sTx.u8CbId ,u32TickCount_ms & 65535);

	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA; // data packet.

#ifdef USE_AES
	if (IS_CRYPT_MODE()) {
		sTx.bSecurePacket = TRUE;
	}
#endif

	if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
		// ネットワーク層経由の送受信

		sTx.u32SrcAddr = ToCoNet_u32GetSerial();
		sTx.u32DstAddr = TOCONET_NWK_ADDR_NULL;

		// 親機の場合
		if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
			if (u32AddrDst != TOCONET_NWK_ADDR_NULL && TOCONET_NWK_ADDR_IS_LONG(u32AddrDst)) {
				// 32bitアドレスが指定されている場合は、そのアドレス宛に送信する
				sTx.u32DstAddr = u32AddrDst;
			} else {
				// 32bitアドレスが指定されていない場合は、ブロードキャストにて送信する
				sTx.u32DstAddr = TOCONET_NWK_ADDR_BROADCAST;
			}
		}

		// 子機の場合
		if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
			// 宛先は自動的に親機(子機間はNG)
			if (IS_LOGICAL_ID_PARENT(u8AddrDst) || u32AddrDst == TOCONET_NWK_ADDR_PARENT) {
				sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
			}
		}

		// 例外時は送信しない
		if (sTx.u32DstAddr == TOCONET_NWK_ADDR_NULL) {
			return -1;
		}
	} else {
		// MAC 直接の送受信

		// 送信設定の微調整を行う
		if (u8Relay) {
			sTx.u8Retry = DEFAULT_TX_FFFF_COUNT; // ３回送信する
			sTx.u16DelayMin = DEFAULT_TX_FFFF_DELAY_ON_REPEAT_ms; // 中継時の遅延
			sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // application retry
		} else
		if (pTxTemplate == NULL) {
			// 簡易書式のデフォルト設定
			sTx.u8Retry = DEFAULT_TX_FFFF_COUNT; // ３回送信する
			sTx.u16RetryDur = sSerSeqTx.u8PktNum * 10; // アプリケーション再送間隔はパケット分割数に比例
		}

		// 宛先情報(指定された宛先に送る)
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		if (sTx.bAckReq) {
			sTx.u32DstAddr = u32AddrDst;
		} else {
			sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		}

		if (sAppData.u8uart_mode == UART_MODE_TRANSPARENT
			|| sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT
		) {
			sTx.u16DelayMin = 0; // デフォルトの遅延
			sTx.u16DelayMax = 0; // デフォルトの遅延
			sTx.u16RetryDur = 0; // application retry

			uint8 u8retry = (sAppData.sFlash.sData.u16power & 0xF0) >> 4;
			if (sAppData.u8NumChannels > 1) {
				switch (u8retry) {
				case 0x0: u8retry = 0x80; break; // 複数チャネル利用時は 0x80 を与えないと１回送信はしない
				case 0xF: u8retry = 0x80; break;
				default: u8retry |= 0x80;
				}
			} else {
				switch (u8retry) {
				case 0x0: u8retry = 0x0; break;
				case 0xF: u8retry = 0x0; break;
				default: u8retry |= 0x80;
				}
			}
			sTx.u8Retry = u8retry; // 再送回数の設定
			sTx.bAckReq = FALSE; // ACK 無し！
		}
	}

	int i;
	for (i = 0; i < sSerSeqTx.u8PktNum; i++) {
		q = sTx.auData;
		sTx.u8Seq = (sSerSeqTx.u8Seq + i) & CBID_MASK_BASE;
		sTx.u8CbId = (sSerSeqTx.u8PktNum > 1) ? (sTx.u8Seq | CBID_MASK_SPLIT_PKTS) : sTx.u8Seq; // callback will reported with this ID

		if (u8Relay || bNoResponse) { // 中継パケットおよび応答なしフラグの時は完了メッセージを返さない
			sTx.u8CbId |= CBID_MASK_SILENT;
		}

		// コールバックIDと応答IDを紐づける
		au8TxCbId_to_RespID[sTx.u8CbId] = sSerSeqTx.u8RespID;

		// UARTモード間で混在しないように sTx.u8Cmd の値を変えておく
		if( IS_APPCONF_OPT_FORMAT_TO_NOPROMPT() ){
			if( au8UartModeToTxCmdId[sAppData.u8uart_mode] == 1 ){
				sTx.u8Cmd = 3;		// 書式モードならプロンプト無しに化ける
			}else if( au8UartModeToTxCmdId[sAppData.u8uart_mode] == 3 ){
				sTx.u8Cmd = 1;		// プロンプト無しなら書式に化ける
			}else{
				sTx.u8Cmd = au8UartModeToTxCmdId[sAppData.u8uart_mode];
			}
		}else{
			sTx.u8Cmd = au8UartModeToTxCmdId[sAppData.u8uart_mode];
		}

		// ペイロードを構成
		S_OCTET(sAppData.u8AppIdentifier);
		S_OCTET(APP_PROTOCOL_VERSION + (u8Relay << 6));

		S_OCTET(sSerSeqTx.u8RespID); // 応答ID

		S_OCTET(u8AddrSrc); // 送信元アプリケーション論理アドレス
		if (!(sAppData.eNwkMode == E_NWKMODE_LAYERTREE)) { // ネットワークモードの場合は、ロングアドレスは省略
			S_BE_DWORD(u32AddrSrc);  // シリアル番号
		}

		S_OCTET(sSerSeqTx.u8IdReceiver); // 宛先

		if (!(sAppData.eNwkMode == E_NWKMODE_LAYERTREE)) { // ネットワークモードの場合は、ロングアドレスは省略
			S_BE_DWORD(u32AddrDst); //最終宛先
		}

		S_OCTET(sSerSeqTx.u8ReqNum); // request number

		uint8 u8pktinfo = (i << 4) + sSerSeqTx.u8PktNum;
		S_OCTET(u8pktinfo); //トータルパケット数とパケット番号

		if( IS_APPCONF_OPT_FORMAT_TO_NOPROMPT() && au8UartModeToTxCmdId[sAppData.u8uart_mode] == 3 ){
			S_OCTET(0xA0);
		}else{
			S_OCTET(u8Opt); // ペイロードのオプション(コマンド、など)
		}

		uint8 u8len_data = (u16len >= SERCMD_SER_PKTLEN) ? SERCMD_SER_PKTLEN : u16len;

		memcpy (q, p, u8len_data);
		q += u8len_data;

		sTx.u8Len = q - sTx.auData;

		if (sAppData.eNwkMode == E_NWKMODE_LAYERTREE) {
			ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx);
		} else {
			ToCoNet_bMacTxReq(&sTx);
		}

		p += u8len_data;
		u16len -= SERCMD_SER_PKTLEN;
	}

	return 0;
}

/**
 * 透過モード時の送信処理
 * @return
 */
static int16 i16Transmit_Transparent() {
	// 送信
	int16 i16ret
		= i16TransmitSerMsg(
			sSerCmd.au8data,
			sSerCmd.u16len,
			NULL,
			ToCoNet_u32GetSerial(),
			sAppData.u8AppLogicalId,
			sAppData.u16ShAddr_Pair,
			sAppData.u8AppLogicalId_Pair,
			FALSE,
			sAppData.u8UartReqNum,
			sAppData.u8UartReqNum,
			0);

	if (i16ret != -1) {
		// 送信出来たのでバッファ長さを巻き戻す
		sSerCmd.u16len = 0;

		// ペンディングフラグの解除
		bPendingTxOnTransparent = FALSE;
	}

	// 送信カウンタを更新
	sAppData.u8UartReqNum++;

	return i16ret;
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
	uint8 u8RepeatFlag = u8PtclVersion >> 6;
	u8PtclVersion &= 0x1F;
	if (u8PtclVersion != APP_PROTOCOL_VERSION) { return; }
	uint8 u8RespId = G_OCTET();
	uint8 u8AppLogicalId = G_OCTET(); (void)u8AppLogicalId;
	uint32 u32AddrSrc = pRx->bNwkPkt ? pRx->u32SrcAddr : G_BE_DWORD();
	uint8 u8AppLogicalId_Dest = G_OCTET();
	uint32 u32AddrDst = pRx->bNwkPkt ? pRx->u32DstAddr : G_BE_DWORD();

	/* ここから中身 */
	uint8 u8req = G_OCTET();
	uint8 u8pktsplitinfo = G_OCTET();

	uint8 u8pktnum = u8pktsplitinfo & 0xF;
	uint8 u8idx = (u8pktsplitinfo & 0xF0) >> 4;

	uint8 u8opt = G_OCTET();

	uint8 u8len = (pRx->auData + pRx->u8Len) - p;
	uint16 u16offset = u8idx * SERCMD_SER_PKTLEN;

	/* 宛先と送信元のアドレスが一致する場合は処理しない */
	if (u32AddrSrc == u32AddrDst) return;
	if (u8AppLogicalId == u8AppLogicalId_Dest && u8AppLogicalId < 0x80) return;

	/* 宛先によって処理するか決める */
	bool_t bAcceptAddress = TRUE;
	bool_t bAcceptBroadCast = FALSE;

	if (IS_LOGICAL_ID_EXTENDED(u8AppLogicalId_Dest)) {
		// 拡張アドレスの場合 (アドレスによってパケットを受理するか決定する)
		if (u32AddrDst == TOCONET_MAC_ADDR_BROADCAST || u32AddrDst == TOCONET_NWK_ADDR_BROADCAST) {
			bAcceptBroadCast = TRUE; // ブロードキャストなので受理する
		} else if (u32AddrDst < 0xFFFF) {
			// ショートアドレス形式 (アドレスの一致が原則)
			if (u32AddrDst != sToCoNet_AppContext.u16ShortAddress) {
				bAcceptAddress = FALSE;
			}
		} else if (u32AddrDst & 0x80000000) {
			// 拡張アドレス (アドレスの一致が原則)
			if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)
					&& u32AddrDst == TOCONET_NWK_ADDR_PARENT
			) {
				// 親機で拡張親機アドレス
				bAcceptAddress = TRUE;
			} else
			if (u32AddrDst != ToCoNet_u32GetSerial()) {
				bAcceptAddress = FALSE;
			}
		} else {
			bAcceptAddress = FALSE;
		}
	} else if (u8AppLogicalId_Dest == LOGICAL_ID_BROADCAST) {
		// ブロードキャストは受信する
		bAcceptBroadCast = TRUE;
		bAcceptAddress = (u32AddrSrc == ToCoNet_u32GetSerial()) ? FALSE : TRUE;
	} else if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		// 簡易アドレスが宛先で、自分が子機の場合
		if (u8AppLogicalId_Dest == sAppData.u8AppLogicalId || u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN) {
			if (u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN) {
				bAcceptBroadCast = TRUE;
			}
			if (u32AddrSrc == ToCoNet_u32GetSerial()) {
				// 自分が送ったパケットが中継機により戻ってきた場合
				bAcceptAddress = FALSE;
			}
		} else {
			bAcceptAddress = FALSE;
		}
	} else if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		//  簡易アドレスが宛先で、親機の場合
		if (u8AppLogicalId_Dest != LOGICAL_ID_PARENT) {
			// 親機同士の通信はしない
			bAcceptAddress = FALSE;
		}
	} else {
		bAcceptAddress = FALSE;
	}

	// 宛先が自分宛でない場合、非LayerNetwork のリピータなら一端受信する
	DBGOUT(5, "<B%dA%d>", bAcceptBroadCast ,bAcceptAddress);
	if (!bAcceptAddress) {
		if (!IS_REPEATER()) {
			return;
		}
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
		if (u32AddrSrc != sSerSeqRx.u32SrcAddr) {
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
#ifdef USE_TOCONET_DUPCHK
		// 重複しているかどうかのチェック

		bool_t bDup = ToCoNet_DupChk_bAdd(psDupChk, u32AddrSrc, u8req & 0x7f);
		DBGOUT(4, "<%02X,%c>", u8req, bDup ? 'D' : '-');
		if (bDup) {
			// 重複しいたら新しい人生は始めない
			bNew = FALSE;
		} else {
# ifdef FIX_DUPCHK_L106S
			static uint16 u16dupc_seqprev = 0xFFFF;
			static uint16 u16dupc_tickprev = 0xFFFF;

			if (u16dupc_seqprev == (u8req & 0x7f)
				&& ((u16dupc_tickprev & 0x8000) || ((u32TickCount_ms - u16dupc_tickprev) & 0x7FFF) < 1000)
			) {
				// ToCoNet_DupChk_bAdd() がはじけきれなかった時の保険
				bNew = FALSE;
				DBGOUT(5, "<!S>", u8req, bDup ? 'D' : '-');
			}

			u16dupc_seqprev = u8req & 0x7f;
			u16dupc_tickprev = u32TickCount_ms & 0x7FFF;
# endif
		}
#else
		uint32 u32key;
		if (DUPCHK_bFind(&sDupChk_SerMsg, u32AddrSrc, &u32key)) {
			int iPrev = u32key & 0x7F, iNow = u8req & 0x7F;// MSB がマスクされている場合もあるので修正

			if (iNow == iPrev || (uint8)(iNow - iPrev) > 0x80) {
				// 最近受信したものより新しくないリクエスト番号の場合は、処理しない
				bNew = FALSE;
			}
		}
#endif

		if (bNew) {
			sSerSeqRx.bWaitComplete = TRUE;
			sSerSeqRx.u32Tick = u32TickCount_ms;
			sSerSeqRx.u32SrcAddr = u32AddrSrc;
			sSerSeqRx.u32DstAddr = u32AddrDst;
			sSerSeqRx.u8PktNum = u8pktnum;
			sSerSeqRx.u8ReqNum = u8req;
			sSerSeqRx.u8RespID = u8RespId;
			sSerSeqRx.u8IdSender = u8AppLogicalId;
			sSerSeqRx.u8IdReceiver = u8AppLogicalId_Dest;

#ifndef USE_TOCONET_DUPCHK
			DUPCHK_vAdd(&sDupChk_SerMsg, sSerSeqRx.u32SrcAddr, u8req);
#endif
		}
	}

	if (sSerSeqRx.bWaitComplete) {
		if (u16offset + u8len <= SIZEOF_au8SerBuffRx && u8idx < sSerSeqRx.u8PktNum) {
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
			if (u8RepeatFlag > sSerSeqRx.u8RelayPacket) {
				sSerSeqRx.u8RelayPacket = u8RepeatFlag;
			}
		}

		// check completion
		int i;
		for (i = 0; i < sSerSeqRx.u8PktNum; i++) {
			if (sSerSeqRx.bPktStatus[i] == 0) break;
		}

		if (i == sSerSeqRx.u8PktNum) {
			// 分割パケットが全て届いた！

			// 中継を行うかの判定
			if (IS_REPEATER()) {
				bool_t bCondRepeat = FALSE;
				if (sAppData.u8uart_mode == UART_MODE_TRANSPARENT) {
					// 透過モード
					if ((IS_REPEATER() && !bAcceptAddress) || IS_DEDICATED_REPEATER()) {
						bCondRepeat = TRUE;
					}
				} else
				if (IS_REPEATER() && (bAcceptBroadCast || (!bAcceptAddress))) {
					bCondRepeat = TRUE;
				}

				// カウント数を超えていたらこれ以上リピートしない
				if (bCondRepeat && sSerSeqRx.u8RelayPacket >= REPEATER_MAX_COUNT()) {
					bCondRepeat = FALSE;
					DBGOUT(4, "<RPT EXPIRE(%d) FR:%02X TO:%02X #:%02X>", sSerSeqRx.u8RelayPacket, sSerSeqRx.u8IdSender, sSerSeqRx.u8IdReceiver, sSerSeqRx.u8ReqNum);
				}

				// リピータで、パケットがブロードキャスト、または、送信アドレス
				if (bCondRepeat) {
					// まだ中継されていないパケットなら、中継する
					i16TransmitSerMsg(
							au8SerBuffRx,
							sSerSeqRx.u16DataLen,
							NULL,
							sSerSeqRx.u32SrcAddr,
							sSerSeqRx.u8IdSender,
							sSerSeqRx.u32DstAddr,
							sSerSeqRx.u8IdReceiver,
							sSerSeqRx.u8RelayPacket + 1,
							sSerSeqRx.u8ReqNum,
							sSerSeqRx.u8RespID,
							u8opt
							);

					DBGOUT(4, "<RPT(%d) FR:%02X TO:%02X #:%02X>", sSerSeqRx.u8RelayPacket, sSerSeqRx.u8IdSender, sSerSeqRx.u8IdReceiver, sSerSeqRx.u8ReqNum);
				}
			}

			// 自分宛のメッセージなら、UART に出力する
			if (bAcceptAddress && !IS_DEDICATED_REPEATER()) {
				// 受信データの出力
				if (sAppData.u8uart_mode == UART_MODE_TRANSPARENT) {
					// 透過モード
					int j;
					bool_t bOk = FALSE;

					if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
						if (sAppData.u8AppLogicalId_Pair == sSerSeqRx.u8IdSender) {
							bOk = TRUE; // 送り元とIDが一致した場合
						}
					} else if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
						if (IS_LOGICAL_ID_PARENT(sSerSeqRx.u8IdSender)) {
							bOk = TRUE; // 相手が親機なら
						}
					}

					if (bOk) {
						for (j = 0; j < sSerSeqRx.u16DataLen; j++) {
							vPutChar(&sSerStream, au8SerBuffRx[j]);
						}
					}
				} else if (sAppData.u8uart_mode == UART_MODE_CHAT) {
					// Chat モードは送り元情報付きで送信する。
					int i, len = 0;

					sSerCmdOut.u16len = sSerSeqRx.u16DataLen;

					// ハンドル名の取り出し（パケット末尾のデータ）
					for (i = sSerSeqRx.u16DataLen - 2; sSerCmdOut.au8data[i] != 0; i--) {
						len++;
					}
					if (len > 31 || (len + 2 >= sSerCmdOut.u16len)) { // ハンドル名が長すぎるか、パケットより大きい
						return;
					}

					if (len == 0) {
						// ハンドル名なし
						vfPrintf(&sSerStream, LB"[%08X:%d] ", sSerSeqRx.u32SrcAddr, sSerSeqRx.u8ReqNum);
						sSerCmdOut.u16len -= 2; // 本文の長さに設定
					} else {
						// ハンドル名あり
						vfPrintf(&sSerStream, LB"[%s:%d] ", &sSerCmdOut.au8data[i+1], sSerSeqRx.u8ReqNum);
						sSerCmdOut.u16len = i; // 本文の長さに設定
					}

					// 本文の出力
					sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);

					// プロンプトの表示
					vSerChatPrompt();
				} else if (sAppData.u8uart_mode == UART_MODE_CHAT_NO_PROMPT) {
					sSerCmdOut.u16len = sSerSeqRx.u16DataLen;
					sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);
				} else {
					int8 I8HEAD = 0;

					if (u8opt == SERCMD_ID_TRANSMIT_EX) {
						// 拡張形式のパケットの出力
						I8HEAD = -14; // バイト分のヘッダを追加して出力

						uint8 *q = sSerCmdOut.au8data + I8HEAD; // このバッファは後ろ１６バイトまで有効

						// 以下ヘッダは１４バイト
						S_OCTET(sSerSeqRx.u8IdSender);
						S_OCTET(u8opt);
						S_OCTET(sSerSeqRx.u8RespID);
						S_BE_DWORD(u32AddrSrc);
						S_BE_DWORD(u32AddrDst);
						S_OCTET(pRx->u8Lqi);
						S_OCTET(sSerSeqRx.u16DataLen >> 8);
						S_OCTET(sSerSeqRx.u16DataLen & 0xFF);

					} else {
						// 標準形式のヘッダ出力
						I8HEAD = -2; // バイト分のヘッダを追加して出力

						uint8 *q = sSerCmdOut.au8data + I8HEAD; // このバッファは後ろ１６バイトまで有効

						// 以下ヘッダは２バイト
						S_OCTET(sSerSeqRx.u8IdSender);
						S_OCTET(u8opt);
					}

					// UART 出力を行う
					sSerCmdOut.u16len = sSerSeqRx.u16DataLen - I8HEAD;
					uint8 *p_bak = sSerCmdOut.au8data;
					sSerCmdOut.au8data = sSerCmdOut.au8data + I8HEAD;
					sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);
					sSerCmdOut.au8data = p_bak;
				}
			}

			memset(&sSerSeqRx, 0, sizeof(sSerSeqRx));
		}
	}
}

#ifdef USE_DIO_SLEEP
/** @ingroup MASTER
 * スリープの実行
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
static void vSleep0(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep) {
	// print message.

	// stop interrupt source, if interrupt source is still running.
	;

	// set UART Rx port as interrupt source
	vAHI_DioSetDirection(PORT_SLEEP_WAKE_MASK, 0); // set as input

	(void)u32AHI_DioInterruptStatus(); // clear interrupt register
	vAHI_DioWakeEnable(PORT_SLEEP_WAKE_MASK, 0); // also use as DIO WAKE SOURCE
	// vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
	vAHI_DioWakeEdge(PORT_SLEEP_WAKE_MASK, 0); // 割り込みエッジ（立上がりに設定）
	// vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE

	// wake up using wakeup timer as well.
	ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32SleepDur_ms, bPeriodic, bDeep); // PERIODIC RAM OFF SLEEP USING WK0
}

/** @ingroup MASTER
 * IO・ペリフェラルの前処理を行ってスリープを実行する。
 */
static void vSleep() {
	// DBGOUT(0,"!SLEEP"LB);

	// 出力完了待ち
	WAIT_UART_OUTPUT(sSerStream.u8Device);

	// ポートの設定
	vPortSetHi(PORT_UART_RTS);
	vPortSetHi(PORT_UART_TX);
	vPortSetHi(PORT_UART_TX_SUB);

	// スリープの処理
	vSleep0(0, FALSE, FALSE);
}
#endif


/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
