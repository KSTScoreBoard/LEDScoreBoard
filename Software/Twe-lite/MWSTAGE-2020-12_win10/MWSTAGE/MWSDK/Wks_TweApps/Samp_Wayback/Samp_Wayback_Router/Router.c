/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#include "Router.h"

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
#define ToCoNet_USE_MOD_TXRXQUEUE_BIG

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define V_PRINTF(...) vfPrintf(&sSerStream,__VA_ARGS__)
#define V_PUTCHAR(c) (&sSerStream)->bPutChar((&sSerStream)->u8Device, c)

#define TOCONET_DEBUG_LEVEL 5

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// ToCoNet version
	uint32 u32ToCoNetVersion;

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk;
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig;

	// Led
	uint32 u32LedCt;

	// Flash Information
	tsFlash sFlash;
} tsAppData;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
//
PRIVATE void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
PRIVATE void vInitHardware(int f_warm_start);

PRIVATE void vSerialInit();
PRIVATE void vHandleSerialInput();


/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
PRIVATE tsAppData sAppData;

PUBLIC tsFILE sSerStream;
tsSerialPortSetup sSerPort;
tsSerCmd sSerCmd; // serial command parser

// Timer object
tsTimerContext sTimerApp;

/****************************************************************************/
/***        FUNCTIONS                                                     ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: vProcessEvent
 *
 * DESCRIPTION:
 *   The Application Main State Machine.
 *
 * RETURNS:
 *
 ****************************************************************************/
//
PRIVATE void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch (pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			// フラッシュからの読み込みが成功すれば、その Layer を採用する
			sAppData.sNwkLayerTreeConfig.u8Layer = 0;
			if (bFlash_Read(&sAppData.sFlash, FLASH_SECTOR_NUMBER-1, 0)) {
				sAppData.sNwkLayerTreeConfig.u8Layer = sAppData.sFlash.sData.u8Layer;
			}
			if (sAppData.sNwkLayerTreeConfig.u8Layer == 0) {
				sAppData.sNwkLayerTreeConfig.u8Layer = 1;
			}

			sAppData.sNwkLayerTreeConfig.u16TxMinDelayUp_ms = 100;
			sAppData.sNwkLayerTreeConfig.u16TxMaxDelayUp_ms = 200;

#ifdef USE_AES
			// 暗号化鍵の登録
			ToCoNet_bRegisterAesKey((void*)au8EncKey, NULL);
#endif
			// Router として始動
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ROUTER;
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				vfPrintf(&sSerStream, LB "* start router (layer %d)", sAppData.sFlash.sData.u8Layer);
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			} else {
				// fatal error
			}

			// start with verbose mode
			bSerCmd_VerboseMode = TRUE;
		}
		break;

	case E_STATE_RUNNING:
		break;
	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: AppColdStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
PUBLIC void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI initialization (very first of code)

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// clear application context
		memset(&sAppData, 0x00, sizeof(sAppData));
		memset(&sSerCmd, 0x00, sizeof(sSerCmd));

		// SPRINTF
		SPRINTF_vInit128();

		// configure network]
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;
		sToCoNet_AppContext.u32ChMask = CHMASK;

		sToCoNet_AppContext.u8TxMacRetry = 1;
		sToCoNet_AppContext.bRxOnIdle = TRUE;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// event machine
		ToCoNet_Event_Register_State_Machine(vProcessEvCore); // main state machine

		// Other Hardware
		vSerialInit();
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		vInitHardware(FALSE);

		// START UP MESSAGE
		vfPrintf(&sSerStream, "\r\n\r\n*** " APP_NAME " %d.%02d-%d ***",
				VERSION_MAIN, VERSION_SUB, VERSION_VAR);
		vfPrintf(&sSerStream, LB "* App ID:%08x Long Addr:%08x Short Addr %04x",
				sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(),
				sToCoNet_AppContext.u16ShortAddress);
	}
}

/****************************************************************************
 *
 * NAME: AppWarmStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
PUBLIC void cbAppWarmStart(bool_t bAfterAhiInit) {
	cbAppColdStart(bAfterAhiInit);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vMain
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
//
void cbToCoNet_vMain(void) {
	/* handle serial input */
	vHandleSerialInput();
}

/****************************************************************************
 *
 * NAME: cbvMcRxHandler
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
PUBLIC void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	int i;

	// print coming payload
	V_PRINTF(
			"\n\r[PKT Ad:%04x,Ln:%03d,Seq:%03d,Lq:%03d,Tms:%05d \"",
			pRx->u32SrcAddr, pRx->u8Len, // Actual payload byte: the network layer uses additional 4 bytes.
			pRx->u8Seq, pRx->u8Lqi, pRx->u32Tick & 0xFFFF);

	for (i = 0; i < pRx->u8Len; i++) {
		if (i < 32) {
			V_PUTCHAR((pRx->auData[i] >= 0x20 && pRx->auData[i] <= 0x7f) ?
							pRx->auData[i] : '.');
		} else {
			V_PRINTF( "..");
			break;
		}
	}
	V_PRINTF( "\"]");

	// 直接受信したパケットを上位へ転送する
	if (pRx->auData[0] == 'T') {
		tsTxDataApp sTx;
		memset(&sTx, 0, sizeof(sTx));

		sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
		sTx.u32SrcAddr = ToCoNet_u32GetSerial(); // Transmit using Long address
		sTx.u8Cmd = 0; // data packet.

		sTx.u8Seq = pRx->u8Seq;
		sTx.u8CbId = pRx->u8Seq;

		sTx.u16DelayMax = 300; // 送信開始の遅延を大きめに設定する

		memcpy(sTx.auData, pRx->auData, pRx->u8Len);
		sTx.auData[0] = 'R';
		sTx.u8Len = pRx->u8Len;

#ifdef USE_AES
		sTx.bSecurePacket = TRUE;
#endif

		SPRINTF_vRewind();
		vfPrintf(SPRINTF_Stream, ":%03d", pRx->u8Lqi);

		memcpy(sTx.auData + sTx.u8Len, SPRINTF_pu8GetBuff(), SPRINTF_u16Length());
		sTx.u8Len += SPRINTF_u16Length();

		ToCoNet_Nwk_bTx(sAppData.pContextNwk, &sTx);
	}
}

/****************************************************************************
 *
 * NAME: cbvMcEvTxHandler
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
PUBLIC void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	V_PRINTF( LB ">>> MacAck%s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);

	return;
}

/****************************************************************************
 *
 * NAME: cbToCoNet_vNwkEvent
 *
 * DESCRIPTION:
 *
 * PARAMETERS:      Name            RW  Usage
 *
 * RETURNS:
 *
 * NOTES:
 ****************************************************************************/
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		V_PRINTF( LB"[E_EVENT_TOCONET_NWK_START]");
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
		V_PRINTF( LB"[E_EVENT_TOCONET_NWK_DISCONNECT]");
		break;

	case E_EVENT_TOCONET_NWK_ROUTE_PKT:
		if (u32arg) {
			tsRoutePktInfo *pInfo = (void*)u32arg;

			if (pInfo->bUpstream) {
				sAppData.u32LedCt = u32TickCount_ms;
			}
		}
		break;

	case E_EVENT_TOCONET_NWK_MESSAGE_POOL_REQUEST:
		sAppData.u32LedCt = u32TickCount_ms;
		break;

	case E_EVENT_TOCONET_NWK_MESSAGE_POOL:
		if (u32arg) {
			tsToCoNet_MsgPl_Entity *pInfo = (void*)u32arg;
			int i;

			uint8 u8buff[TOCONET_MOD_MESSAGE_POOL_MAX_MESSAGE+1];
			memcpy(u8buff, pInfo->au8Message, pInfo->u8MessageLen);
			u8buff[pInfo->u8MessageLen] = 0;

			V_PRINTF( "[MSGPOOL sl=%d ln=%d msg=",
					pInfo->u8Slot,
					pInfo->u8MessageLen
					);

			for (i = 0; i < pInfo->u8MessageLen; i++) {
				V_PRINTF("%02X", u8buff[i]);
			}

			V_PRINTF("]"LB);
		}
		break;

	case E_EVENT_TOCONET_PANIC:
		if (u32arg) {
			tsPanicEventInfo *pInfo = (void*)u32arg;
			V_PRINTF( "PANIC DETECTED!");
			pInfo->bCancelReset = FALSE;
		}
		break;
	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: cbToCoNet_vHwEvent
 *
 * DESCRIPTION:
 * Process any hardware events.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
//
PUBLIC void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		// LED BLINK
		vPortSet_TrueAsLo(PORT_KIT_LED2, u32TickCount_ms & 0x400);

		// LED ON when receive
		if (u32TickCount_ms - sAppData.u32LedCt < 300) {
			vPortSetLo(PORT_KIT_LED1);
		} else {
			vPortSetHi(PORT_KIT_LED1);
		}
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	default:
		break;
	}
}

/****************************************************************************
 *
 * NAME: cbToCoNet_u8HwInt
 *
 * DESCRIPTION:
 *   called during an interrupt
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u32DeviceId
 *                  u32ItemBitmap
 *
 * RETURNS:
 *                  FALSE -  interrupt is not handled, escalated to further
 *                           event call (cbToCoNet_vHwEvent).
 *                  TRUE  -  interrupt is handled, no further call.
 *
 * NOTES:
 *   Do not put a big job here.
 ****************************************************************************/
//
PUBLIC uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
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
//
PRIVATE void vInitHardware(int f_warm_start) {
	// LED's
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);
	vPortAsOutput(PORT_KIT_LED3);
	vPortAsOutput(PORT_KIT_LED4);
	vPortSetHi(PORT_KIT_LED1);
	vPortSetHi(PORT_KIT_LED2);
	vPortSetHi(PORT_KIT_LED3);
	vPortSetHi(PORT_KIT_LED4);

	// activate tick timers
	memset(&sTimerApp, 0, sizeof(sTimerApp));
	sTimerApp.u8Device = E_AHI_DEVICE_TIMER0;
	sTimerApp.u16Hz = 1;
	sTimerApp.u8PreScale = 10;
	vTimerConfig(&sTimerApp);
	vTimerStart(&sTimerApp);
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
PRIVATE void vHandleSerialInput() {
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		// process
		if (i16Char >=0 && i16Char <= 0xFF) {
			uint8 u8res = u8ParseSerCmd(&sSerCmd, (uint8)i16Char);
			if (u8res == E_SERCMD_VERBOSE) {
				vfPrintf(&sSerStream, "\n\rVERBOSE MODE = %s", bSerCmd_VerboseMode ? "ON" : "OFF");
				continue;
			}
			if (!bSerCmd_VerboseMode) continue;

			switch (i16Char) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				sAppData.sFlash.sData.u8Layer = i16Char - '0';
				if (bFlash_Write(&sAppData.sFlash, FLASH_SECTOR_NUMBER - 1, 0)) {
					V_PRINTF( LB "Flash Saved (Router Layer #%d). .. RESETTING", sAppData.sFlash.sData.u8Layer);
					vWait(100000);
					vAHI_SwReset();
				} else {
					V_PRINTF( LB "Failed to save flash...");
				}
				break;

			case 'i': // info
				_C {
					tsToCoNet_NwkLyTr_Context *pc = (tsToCoNet_NwkLyTr_Context *)(sAppData.pContextNwk);

					V_PRINTF( LB "Info: la=%d ty=%d ro=%02x st=%02x",
							pc->sInfo.u8Layer, pc->sInfo.u8NwkTypeId, pc->sInfo.u8Role, pc->sInfo.u8State);
					V_PRINTF( LB "Parent: %08x", pc->u32AddrHigherLayer);
					V_PRINTF( LB "LostParent: %d", pc->u8Ct_LostParent);
					V_PRINTF( LB "SecRescan: %d, SecRelocate: %d", pc->u8Ct_Second_To_Rescan, pc->u8Ct_Second_To_Relocate);
				}
				break;

			case '>':
				sToCoNet_AppContext.u8Channel++;
				if (sToCoNet_AppContext.u8Channel > 25)
					sToCoNet_AppContext.u8Channel = 0;
				ToCoNet_vRfConfig();
				V_PRINTF( LB"channel set to %d.", sToCoNet_AppContext.u8Channel);
				break;

			case '<':
				sToCoNet_AppContext.u8Channel--;
				if (sToCoNet_AppContext.u8Channel < 11)
					sToCoNet_AppContext.u8Channel = 25;
				ToCoNet_vRfConfig();
				V_PRINTF( LB"channel set to %d.", sToCoNet_AppContext.u8Channel);
				break;

			case 't':
				SPRINTF_vRewind();
				vfPrintf(SPRINTF_Stream, "TEST FROM ROUTER(#%08X)", ToCoNet_u32GetSerial());
				bTransmitToParent(sAppData.pContextNwk, SPRINTF_pu8GetBuff(), SPRINTF_u16Length());
				break;

			case 'd': case 'D':
				_C {
					static uint8 u8DgbLvl;

					u8DgbLvl++;
					if(u8DgbLvl > 10) u8DgbLvl = 0;
					ToCoNet_vDebugLevel(u8DgbLvl);

					V_PRINTF( LB"set NwkCode debug level to %d.", u8DgbLvl);
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
