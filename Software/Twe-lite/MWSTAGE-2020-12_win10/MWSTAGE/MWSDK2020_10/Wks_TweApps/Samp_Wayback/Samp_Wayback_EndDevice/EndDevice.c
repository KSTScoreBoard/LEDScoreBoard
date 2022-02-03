/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/**
 * 概要
 *
 * 本パートは、IO状態を取得し、ブロードキャストにて、これをネットワーク層に
 * 送付する。
 *
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#include "EndDevice.h"

#include "utils.h"
#include "serialInputMgr.h"
#include "flash.h"

#include "common.h"

// Serial options
#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_NWK_LAYERTREE
#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#define ToCoNet_USE_MOD_NBSCAN_SLAVE
#define ToCoNet_USE_MOD_CHANNEL_MGR
#define ToCoNet_USE_MOD_NWK_MESSAGE_POOL
#define ToCoNet_USE_MOD_DUPCHK

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define V_PRINTF(...) vfPrintf(&sSerStream,__VA_ARGS__)
#define V_PUTCHAR(c) (&sSerStream)->bPutChar((s)->u8Device, c)

#define TOCONET_DEBUG_LEVEL 0 // スタックデバッグメッセージ

#define SLEEP_DUR_ms 2000
#undef NO_SLEEP

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef enum {
	E_LED_OFF,
	E_LED_WAIT,
	E_LED_ERROR,
	E_LED_RESULT
} teLedState;

typedef enum {
	E_NWK_FAIL = -1,
	E_NWK_IDLE = 0,
	E_NWK_START = 1,
	E_NWK_TX_COMP_MASK = 2,
	E_NWK_MSG_GOT_MASK = 8

} teNwStat;
typedef struct {
	// ToCoNet version
	uint32 u32ToCoNetVersion;

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk;
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig;

	// Flash Information
	tsFlash sFlash; //!< フラッシュの情報

	// wakeup status
	bool_t bWakeupByButton; //!< DIO で RAM HOLD Wakeup した場合 TRUE

	// frame count
	uint16 u16frame_count;

	// config mode
	bool_t bConfigMode; // 設定モード

	// NW STATE
	int8 i8NwState; // ネットワークの状態フラグ
	uint8 u8MsgMsk; // メッセージプールのマスク

	// 失敗カウント
	uint8 u8failct;
} tsAppData;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vProcessEvCoreConfig(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vInitHardware(int f_warm_start);

static void vSerialInit();
static void vHandleSerialInput();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
static tsAppData sAppData;

PUBLIC tsFILE sSerStream;
tsSerialPortSetup sSerPort;

/****************************************************************************/
/***        FUNCTIONS                                                     ***/
/****************************************************************************/

/**
 * アプリケーション制御関数
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static uint32 u32InitCt;
	static bool_t bNoSleepReinit;

	switch (pEv->eState) {
	case E_STATE_IDLE:
		_C {
			bool_t bAesRes = FALSE;

			if (eEvent == E_EVENT_START_UP || bNoSleepReinit) {
				u32InitCt = u32TickCount_ms;
#ifdef USE_AES
				// 暗号化鍵の登録
				bAesRes = ToCoNet_bRegisterAesKey((void*)au8EncKey, NULL);
#endif

				if (bNoSleepReinit || (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
					// 起床メッセージ
					vfPrintf(&sSerStream, LB LB "*** Warm starting Addr:%08x woke by %s. ***",
							ToCoNet_u32GetSerial(),
							sAppData.bWakeupByButton ? "DIO" : "WakeTimer");

					// レジュームする
					if (sAppData.u8failct > 5) {
						// 5連続でシーケンス失敗したら、接続先を再探索
						sAppData.u8failct = 0;

						vfPrintf(&sSerStream, LB "* LOST PARENT/ROUTER? RESCAN");
						ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_NW_START_BOOT);
					} else {
						// レジューム（即送信可能）する
						sAppData.i8NwState = E_NWK_IDLE;
						if (ToCoNet_Nwk_bResume(sAppData.pContextNwk)) {
							// ToCoNet_Nwk_bResume() 関数呼び出し中に E_EVENT_TOCONET_NWK_START
							// イベントが発生するので注意！
							ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_NW_START);
						} else {
							// 失敗したらスキャンからやり直し
							ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_NW_START_BOOT);
						}
					}
				} else {
					// 始動メッセージ
					vfPrintf(&sSerStream, LB LB "*** " APP_NAME " %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
					vfPrintf(&sSerStream, LB "* App ID:%08x Long Addr:%08x Short Addr %04x",
							sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress);

					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_NW_START_BOOT);
				}
			}
#ifdef USE_AES
			vfPrintf(&sSerStream, LB "*** Register AES key (%d) ***", bAesRes);
#endif
		}

		bNoSleepReinit = FALSE;
		break;

	case E_STATE_APP_WAIT_NW_START_BOOT:
		/*
		 * ネットワークを開始する(接続先スキャンから開始)
		 */
		_C {
			static uint32 u32startCt;

			if (eEvent == E_EVENT_NEW_STATE) {
				V_PRINTF(LB"[E_STATE_APP_WAIT_NW_START_BOOT]");
			}

			if (eEvent == E_EVENT_NEW_STATE) {
				// SCANのネットワーク負荷が大きいので集中しないようにランダム待ちを入れる
				u32startCt = u32TickCount_ms + (ToCoNet_u16GetRand() & 0x7FF); // 最大２秒まつ
			}

			if (u32TickCount_ms - u32startCt > 0x80000000) {
				// 引き算して0x80000000 以上(＝負の値に相当)は待ち中
				break;
			} else {
				// SCAN開始
				vfPrintf(&sSerStream, LB LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);
				sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
				sAppData.sNwkLayerTreeConfig.u8ResumeOpt = 0x01; // レジューム時にLOCATE&SCANしない
				sAppData.sNwkLayerTreeConfig.u8Second_To_Relocate = 0xFF; // LOCATE しない

				// ネットワークの初期化
				sAppData.i8NwState = E_NWK_IDLE;
				sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);

				if (sAppData.pContextNwk) {
					ToCoNet_Nwk_bInit(sAppData.pContextNwk);
					ToCoNet_Nwk_bStart(sAppData.pContextNwk);
				}

				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_NW_START);
			}
		}
		break;


	case E_STATE_APP_WAIT_NW_START:
		/*
		 * ネットワークの開始を待つ
		 */
		if (eEvent == E_EVENT_NEW_STATE) {
			V_PRINTF(LB"[E_STATE_APP_WAIT_NW_START]");
		}

		if (sAppData.i8NwState == E_NWK_START) {
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		} else if (sAppData.i8NwState == E_NWK_FAIL) {
			V_PRINTF(LB"! CONNECTION FAILS");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		} else if (ToCoNet_Event_u32TickFrNewState(pEv) > 1000) {
			V_PRINTF(LB"! CONNECTION TIMEOUT");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}
		break;

	case E_STATE_RUNNING:
		/*
		 * 実行状態
		 */
		if (eEvent == E_EVENT_NEW_STATE) {
			V_PRINTF(LB"[E_STATE_RUNNING]");

			sAppData.u16frame_count++; // 続き番号を更新する

			tsTxDataApp sTx;
			memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！

			sTx.u32SrcAddr = ToCoNet_u32GetSerial();
			sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;

			// ペイロードの準備
			SPRINTF_vRewind();
			vfPrintf(SPRINTF_Stream, "T:%04X:%08X:%02X:%s",
					sAppData.u16frame_count,
					ToCoNet_u32GetSerial(),
					sAppData.sFlash.sData.u16Id,
					"012345678901234567890123456789012345678901234567890123456789");
			memcpy (sTx.auData, SPRINTF_pu8GetBuff(), SPRINTF_u16Length());

			sTx.u8Len = SPRINTF_u16Length(); // パケットのサイズ
			sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
			sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)
			sTx.u8Cmd = 0; // 0..7 の値を取る。パケットの種別を分けたい時に使用する

#ifdef USE_AES
			sTx.bSecurePacket = TRUE;
#endif
			sAppData.u8MsgMsk = (1 << 0) | (1 << 1) | (1 << 2); // pool #0,1,2
			if (ToCoNet_MsgPl_bRequest_w_Payload_to_Parenet(0x80 | sAppData.u8MsgMsk, &sTx)) {
				V_PRINTF(LB"! TxOk");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
			} else {
				V_PRINTF(LB"! TxFl");
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
			}
		}
		break;

	case E_STATE_APP_WAIT_TX:
		/*
		 * 送信,受信完了待ち
		 */
		if ((sAppData.i8NwState & E_NWK_TX_COMP_MASK) && (sAppData.i8NwState & E_NWK_MSG_GOT_MASK)) {
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		} else
		if (ToCoNet_Event_u32TickFrNewState(pEv) > 1000) {
			V_PRINTF(LB"! TIMEOUT");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}
		break;

	case E_STATE_APP_SLEEP:
		/*
		 * スリープ処理
		 */
		if (eEvent == E_EVENT_NEW_STATE) {
			// ネットワークをポーズしておく
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);

#ifdef NO_SLEEP
			// 仮想スリープする
			ToCoNet_Event_SetState(pEv, E_STATE_APP_PREUDO_SLEEP);
#else
			// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
			V_PRINTF(LB"! Sleeping... %d", u32TickCount_ms - u32InitCt);
			SERIAL_vFlush(UART_PORT);

			// 2秒間のスリープに入る
			vSleep(SLEEP_DUR_ms, TRUE, FALSE);
#endif
		}
		break;

	case E_STATE_APP_PREUDO_SLEEP:
		/*
		 * スリープせずに稼働する
		 */
		if (eEvent == E_EVENT_NEW_STATE) {
			V_PRINTF(LB"! Pseudo Sleep %d", SLEEP_DUR_ms);
			sAppData.i8NwState = E_NWK_IDLE;
		}

		if (ToCoNet_Event_u32TickFrNewState(pEv) > SLEEP_DUR_ms) {
			bNoSleepReinit = TRUE;
			ToCoNet_Event_SetState(pEv, E_STATE_IDLE);
		}
		break;

	default:
		break;
	}
}

/**
 * 設定モード(UART の処理を行うため常時稼働)
 */
static void vProcessEvCoreConfig(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch (pEv->eState) {
		case E_STATE_IDLE:
			if (eEvent == E_EVENT_START_UP) {
				vfPrintf(&sSerStream, LB "*** ToCoSamp IO Monitor %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
				vfPrintf(&sSerStream, LB "* App ID:%08x Long Addr:%08x Short Addr %04x",
						sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress);
				vfPrintf(&sSerStream, LB "* start end device config mode...", u32TickCount_ms & 0xFFFF);
				vfPrintf(&sSerStream, LB "* - press 0-9 to set local ID");

				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}
			break;

		case E_STATE_RUNNING:
			break;

		default:
			break;
	}
}

/**
 * 始動処理
 * @param bAfterAhiInit
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI initialization (very first of code)

		// check DIO source
		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
    	if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
    		sAppData.bWakeupByButton = TRUE;
		}

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// アプリケーション保持構造体の初期化
		memset(&sAppData, 0x00, sizeof(sAppData));

		// SPRINTFの初期化(128バイトのバッファを確保する)
		SPRINTF_vInit128();

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		if (!bFlash_Read(&sAppData.sFlash, FLASH_SECTOR_NUMBER-1, 0)) {
			sAppData.sFlash.sData.u16Id = 15;
		}

		// configure network
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;
		sToCoNet_AppContext.u32ChMask = CHMASK;

		sToCoNet_AppContext.u8TxMacRetry = 1;
		sToCoNet_AppContext.bRxOnIdle = FALSE;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// Other Hardware
		vInitHardware(FALSE);

		// ToCoNet DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		// event machine
		if (sAppData.bConfigMode) {
			ToCoNet_Event_Register_State_Machine(vProcessEvCoreConfig); // main state machine
		} else {
			ToCoNet_Event_Register_State_Machine(vProcessEvCore/*_normal*/); // main state machine
		}
	}
}

/**
 * スリープ起床
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
		if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}
	} else {
		// Other Hardware
		vSerialInit();
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		vInitHardware(FALSE);

		if (!sAppData.bWakeupByButton) {
			// タイマーで起きた
		} else {
			// ボタンで起床した
		}
	}
}


/**
 * メイン処理
 */
void cbToCoNet_vMain(void) {
	/* handle serial input */
	vHandleSerialInput();
}

/**
 * 受信は無い
 */
PUBLIC void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	V_PRINTF(LB ">>> Rx <<<");
	V_PRINTF(LB "> Addr=%08X", pRx->u32SrcAddr);
	V_PRINTF(LB "> Lqi=%02d", pRx->u8Lqi);
	V_PRINTF(LB "> %s", pRx->auData);

	sAppData.i8NwState |= 4;
}

/**
 * 送信完了時のイベント
 * @param u8CbId
 * @param bStatus
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	V_PRINTF(LB ">>> TxCmp %s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);

	if (bStatus) {
		sAppData.i8NwState |= E_NWK_TX_COMP_MASK;
		sAppData.u8failct = 0;
	} else {
		sAppData.i8NwState = E_NWK_FAIL;
		sAppData.u8failct++;
	}

	return;
}

/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_DISCONNECT:
		sAppData.i8NwState = E_NWK_FAIL;
		break;

	case E_EVENT_TOCONET_NWK_START:
		sAppData.i8NwState = E_NWK_START;

		_C {
			tsToCoNet_NwkLyTr_Context *pc = (tsToCoNet_NwkLyTr_Context *)(sAppData.pContextNwk);
			V_PRINTF( LB "Parent: %08x", pc->u32AddrHigherLayer);
		}
		break;

#ifdef ToCoNet_USE_MOD_NWK_MESSAGE_POOL
	/*
	 * メッセージプールを受信
	 */
	case E_EVENT_TOCONET_NWK_MESSAGE_POOL:
		if (u32arg) {
			tsToCoNet_MsgPl_Entity *pInfo = (void*)u32arg;
			int i;

			uint8 u8buff[TOCONET_MOD_MESSAGE_POOL_MAX_MESSAGE+1];
			memcpy(u8buff, pInfo->au8Message, pInfo->u8MessageLen); // u8Message にデータ u8MessageLen にデータ長
			u8buff[pInfo->u8MessageLen] = 0;

			// UART にメッセージ出力
			if (pInfo->bGotData) { // empty なら FALSE になる
				V_PRINTF(LB"---MSGPOOL sl=%d ln=%d msg=",
						pInfo->u8Slot,
						pInfo->u8MessageLen
						);

				SPRINTF_vRewind();
				for (i = 0; i < pInfo->u8MessageLen; i++) {
					vfPrintf(SPRINTF_Stream, "%02X", u8buff[i]);
				}
				V_PRINTF("%s", SPRINTF_pu8GetBuff());

#ifdef USE_LCD
				V_PRINTF_LCD("%02x:%s\r\n", u8seq++, SPRINTF_pu8GetBuff());
				vLcdRefresh();
#endif

				V_PRINTF("---");
			} else {
				V_PRINTF(LB"---MSGPOOL sl=%d EMPTY ---",
						pInfo->u8Slot
						);
#ifdef USE_LCD
				V_PRINTF_LCD("%02x: EMPTY\r\n", u8seq++);
				vLcdRefresh();
#endif
			}

			// 受信スロットのマスクを外す
			sAppData.u8MsgMsk &= ~(1 << pInfo->u8Slot);

			// 要求スロットが全部そろったら、終了状態へ遷移
			if (sAppData.u8MsgMsk == 0) {
				sAppData.i8NwState |= E_NWK_MSG_GOT_MASK;
				ToCoNet_Event_Process(E_EVENT_APP_GET_IC_INFO, pInfo->bGotData, vProcessEvCore); // vProcessEvCore にイベント送信
			}
		}
		break;
#endif

	default:
		break;
	}
}

/**
 * ハードウェアイベント（割り込み遅延実行）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	default:
		break;
	}
}

/**
 * ハードウェア割り込み
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	default:
		break;
	}

	return u8handled;
}

/****************************************************************************
 *
 * NAME: vInitHardware
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
static void vInitHardware(int f_warm_start) {
	// LED's
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);

	vPortSetHi(PORT_KIT_LED1);
	vPortSetHi(PORT_KIT_LED2);

	vPortAsInput(PORT_KIT_SW1);
	vPortAsInput(PORT_KIT_SW2);
	vPortAsInput(DIO_BUTTON);

	// いずれかのボタンが押されていた場合、設定モードとして動作する
	if (!f_warm_start && (~u32AHI_DioReadInput() & PORT_INPUT_MASK)) {
		sAppData.bConfigMode = TRUE;
	}

	// Serial Port
	vSerialInit();
}

/****************************************************************************
 *
 * NAME: vSerialInit
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vSerialInit(void) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[1024];
	static uint8 au8SerialRxBuffer[512];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = UART_BAUD;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInit(&sSerPort);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}

/****************************************************************************
 *
 * NAME: vHandleSerialInput
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
static void vHandleSerialInput() {
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		// process
		if (i16Char >=0 && i16Char <= 0xFF) {
			switch (i16Char) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				memset(&sAppData.sFlash.sData, 0, sizeof(tsFlashApp));
				sAppData.sFlash.sData.u16Id = i16Char - '0';
				if (bFlash_Write(&sAppData.sFlash, FLASH_SECTOR_NUMBER - 1, 0)) {
					V_PRINTF( LB "Flash Saved (ID=%d). .. RESETTING", sAppData.sFlash.sData.u16Id);
					vWait(100000);
					vAHI_SwReset();
				} else {
					V_PRINTF( LB "Failed to save flash...");
				}
				break;

			case 'i':
				vDispInfo(&sSerStream, (tsToCoNet_NwkLyTr_Context *)(sAppData.pContextNwk));
				break;

			case '>':
				sToCoNet_AppContext.u8Channel++;
				if (sToCoNet_AppContext.u8Channel > 25)
					sToCoNet_AppContext.u8Channel = 0;
				ToCoNet_vRfConfig();
				V_PRINTF(LB"channel set to %d.", sToCoNet_AppContext.u8Channel);
				break;

			case '<':
				sToCoNet_AppContext.u8Channel--;
				if (sToCoNet_AppContext.u8Channel < 11)
					sToCoNet_AppContext.u8Channel = 25;
				ToCoNet_vRfConfig();
				V_PRINTF(LB"channel set to %d.", sToCoNet_AppContext.u8Channel);
				break;

			case 'd': case 'D':
				_C {
					static uint8 u8DgbLvl;

					u8DgbLvl++;
					if(u8DgbLvl > 5) u8DgbLvl = 0;
					ToCoNet_vDebugLevel(u8DgbLvl);

					V_PRINTF(LB"set NwkCode debug level to %d.", u8DgbLvl);
				}
				break;

			default:
				break;
			}
		}
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
