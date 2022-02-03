/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>
#include <mac_sap.h>
#include <mac_pib.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#include "Master.h"

#include "utils.h"

#include "LCD_Literals.txt"

// Serial options
#define USE_SERIAL
#define SERIAL_FROM_QUEUE (-1)
#ifdef USE_SERIAL
#define USE_SERIAL1
# include <serial.h>
# include <fprintf.h>
#endif

// LCD options
#ifdef USE_LCD
#include "LcdDriver.h"
#include "LcdDraw.h"
#include "LcdPrint.h"
#include "btnMgr.h"

enum {
	E_LCD_PAGE_OPT0 = 0,
	E_LCD_PAGE_OPT1,
	E_LCD_PAGE_OPT2,
	E_LCD_PAGE_LAST,
	E_LCD_PAGE_SCAN_INIT = 10,
	E_LCD_PAGE_SCAN_SUCCESS,
	E_LCD_PAGE_SCAN_FAIL,
	E_LCD_PAGE_PER = 20,
	E_LCD_PAGE_PER_RESULT
};
#endif

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
#define ToCoNet_USE_MOD_ENERGYSCAN
#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#undef ToCoNet_USE_MOD_NBSCAN_SLAVE

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

typedef struct {
	// application state
	teState eState;

	// Random count generation
	uint32 u32randcount; // not used in this application

	// OSC calibration
	uint32 u32RcOscError;

	// frame sequence number
	uint32 u32FrameCount;

	// CCA fail count
	uint16 u16CCAfail;

	// PER
	uint16 u16PerCount;
	uint16 u16PerCountMax;
	uint16 u16PerSuccess;
	uint16 u16PerSuccessAppAck;
	uint8 bPerFinish;
	uint8 bPerAppAckMode;
	uint8 u8PerLqiLast;
	uint32 u32PerLqiSum;

	// PER MAC SETTING
	uint8 u8retry;
	uint8 u8channel;
	uint8 u8Power;

	uint8 u8payload;

	// TICK COUNT
	uint8 u8tick_ms;

	// Tx Finish Flag
	uint8 bTxBusy;

	// 送信中のシーケンス番号
	uint8 u8TxSeq;
	bool_t bRcvAppAck;

	// Child nodes
	uint8 u8ChildFound;
	uint8 u8ChildSelect;
	tsToCoNet_NbScan_Entitiy asChilds[4];
	uint32 u32ChildAddr;
	uint8 u8ChildConfCh;

	// Energy Scan
	uint8 u8ChEnergy;
	uint8 u8ChEnergyMax;
	bool_t bEnergyScanEnabled;

	// Disp Lang
	uint8 u8Lang;
} tsAppData;

#ifdef USE_LCD
static uint8 u8LcdPage = 0;
static uint32 u32LastTickPressed;
#endif

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
//
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vInitHardware(int f_warm_start);

#ifdef USE_SERIAL
static void vSerialInit();
static void vHandleSerialInput(int16 i16CharExt);
#endif

#ifdef USE_LCD
static void vLcdInit();
static void vHandleButtonInput();
void vUpdateLcdPerMes();
void vUpdateLcdOpt();
#endif
/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// PER dummy payload
const uint8 au8payload[] =
		"0123456789a123456789b123456789c123456789d123456789e123456789f123456789g123456789h123456789i123456789j123456789k123456789";
// Local data used by the tag during operation
static tsAppData sAppData;

#ifdef USE_SERIAL
PUBLIC tsFILE sSerStream;
tsSerialPortSetup sSerPort;
#endif
#ifdef USE_LCD
static tsFILE sLcdStream, sLcdStreamBtm;
#endif

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
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	bool_t bFlag;

	static uint32 u32TickStart;

	switch (pEv->eState) {
	case E_STATE_IDLE:
		// just after cold booting
		if (eEvent == E_EVENT_START_UP || eEvent == E_PER_RESCAN_SLAVE) {
			// do nothing
#ifdef USE_LCD
			u8LcdPage = E_LCD_PAGE_SCAN_INIT;
#endif
			ToCoNet_Event_SetState(pEv, E_STATE_SCANNING_INIT);
			break;
		}

		// if button was pressed, do its behavior
		if (eEvent == E_PER_START && sAppData.u8ChildSelect != 0xFF) {
			// KICK PER
			ToCoNet_Event_SetState(pEv, E_STATE_PER_RESCAN_PRE_WAIT);
		}
		break;

	case E_STATE_SCANNING_INIT:
		if (eEvent == E_EVENT_NEW_STATE) {
#ifdef USE_SERIAL
			vfPrintf(&sSerStream, "\n\rChild Scanning...");
#endif
#ifdef USE_LCD
			vUpdateLcdOpt();
#endif
		}

		// wait a small tick
		if (ToCoNet_Event_u32TickFrNewState(pEv) > 200) { // wait to finish Energy Scan (will take around 64ms)
			ToCoNet_vRfConfig();
			ToCoNet_NbScan_bStart(0, 128);
			ToCoNet_Event_SetState(pEv, E_STATE_SCANNING);
		}
		break;

	case E_STATE_SCANNING:
		if (eEvent == E_EVENT_SCAN_FINISH) {
			int i;
			if (sAppData.u8ChildFound == 0) {
#ifdef USE_SERIAL
				vfPrintf(&sSerStream, "\n\rslave device not found (press 'n' to rescan)");
#endif
#ifdef USE_LCD
				u8LcdPage = E_LCD_PAGE_SCAN_FAIL;
				vUpdateLcdOpt();
#endif
			} else {
#ifdef USE_SERIAL
				vfPrintf(&sSerStream, "\n\rfound slave device(s) (press 1 to 4)");
				for (i = 0; i < sAppData.u8ChildFound; i++) {
					tsToCoNet_NbScan_Entitiy *pEnt = &(sAppData.asChilds[i]);
					vfPrintf(&sSerStream, "\n\r%d: %07x,%04x Ch:%02d Lq:%03d",
							i + 1,
							pEnt->u32addr,
							pEnt->u16addr,
							pEnt->u8ch,
							pEnt->u8lqi
					);
				}
#ifdef USE_LCD
				u8LcdPage = E_LCD_PAGE_SCAN_SUCCESS;
				vUpdateLcdOpt();
#endif

				ToCoNet_Event_SetState(pEv, E_STATE_IDLE);
			}
#endif
		}

		if (ToCoNet_Event_u32TickFrNewState(pEv) > 5000) {
			pEv->u32tick_new_state = u32TickCount_ms;
			// time out
#ifdef USE_LCD
			u8LcdPage = E_LCD_PAGE_SCAN_FAIL;
			vUpdateLcdOpt();
#endif

			ToCoNet_Event_SetState(pEv, E_STATE_IDLE);
		}
		break;

	case E_STATE_PER_RESCAN_PRE_WAIT:
		if (eEvent == E_EVENT_NEW_STATE) {
			if (sAppData.u8ChildConfCh != sAppData.u8channel) {
#ifdef USE_SERIAL
			vfPrintf(&sSerStream, "\n\rChild Scanning...");
#endif
#ifdef USE_LCD
				u8LcdPage = E_LCD_PAGE_SCAN_INIT;
				vUpdateLcdOpt();
#endif
			}
			// perform mac reset

		}

		if (ToCoNet_Event_u32TickFrNewState(pEv) >= 200) { // wait to finish energy scan
			if (sAppData.u8ChildConfCh != sAppData.u8channel) {
				ToCoNet_Event_SetState(pEv, E_STATE_PER_RESCAN);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_PER_INIT);
			}
		}
		break;

	case E_STATE_PER_RESCAN:
		_C {
			static bool_t bFailure;

			if (eEvent == E_EVENT_NEW_STATE) {
				bFailure = FALSE;

				ToCoNet_vRfConfig();
				// ToCoNet_bNeighbourScanStart(0, 128);
				ToCoNet_NbScan_bStartToFindAddr(0, sAppData.u32ChildAddr); // try to find the child with 64bit addr.
			}

			if(eEvent == E_EVENT_SCAN_FINISH) {
				if (sAppData.u8ChildSelect == 0xFF) {
					bFailure = TRUE;
#ifdef USE_SERIAL
					vfPrintf(&sSerStream, "\n\rChild not found...");
#endif
#ifdef USE_LCD
					u8LcdPage = E_LCD_PAGE_SCAN_FAIL;
					vUpdateLcdOpt();
#endif
				} else {
					ToCoNet_Event_SetState(pEv, E_STATE_PER_INIT);
				}
			}


			if (ToCoNet_Event_u32TickFrNewState(pEv) > 5000) {
				bFailure = TRUE;

				pEv->u32tick_new_state = u32TickCount_ms;
				// time out
#ifdef USE_LCD
				u8LcdPage = E_LCD_PAGE_SCAN_FAIL;
				vUpdateLcdOpt();
#endif
			}

			if (bFailure && eEvent == E_PER_RESCAN_SLAVE) {
				sAppData.u8ChildConfCh = 0;
				ToCoNet_Event_SetState(pEv, E_STATE_PER_RESCAN_PRE_WAIT);
			}
		}
		break;

	case E_STATE_PER_INIT:
		bFlag = FALSE;
		if (eEvent == E_EVENT_NEW_STATE) {
			sAppData.u16PerCount = 0;
			sAppData.u16PerSuccess = 0;
			sAppData.u16PerSuccessAppAck = 0;
			sAppData.u8PerLqiLast = 0;
			sAppData.u32PerLqiSum = 0;
			sAppData.bTxBusy = FALSE;
			sAppData.bPerFinish = FALSE;

#ifdef USE_SERIAL
			vfPrintf(&sSerStream, "\r\n*** CONFIG DESTINATION ***");
#endif
#ifdef USE_LCD
			u8LcdPage = E_LCD_PAGE_SCAN_INIT;
			vUpdateLcdOpt();
#endif

			if (sAppData.u8ChildConfCh != sAppData.u8channel) {
#ifdef USE_SERIAL
			vfPrintf(&sSerStream, "\r\n** Seek child %04x/ch%02d",
					sAppData.asChilds[sAppData.u8ChildSelect].u8ch,
					sAppData.asChilds[sAppData.u8ChildSelect].u16addr);
#endif
				sToCoNet_AppContext.u8Channel = sAppData.asChilds[sAppData.u8ChildSelect].u8ch;
				sToCoNet_AppContext.u8TxMacRetry = 3;
				sToCoNet_AppContext.u8TxPower = 3;
				ToCoNet_vRfConfig();

				// transmit!
				tsTxDataApp tsTx;
				memset(&tsTx, 0, sizeof(tsTxDataApp));
				uint8 *q = tsTx.auData;

				tsTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress; // can be extended???
				tsTx.u32DstAddr = sAppData.asChilds[sAppData.u8ChildSelect].u16addr;

				tsTx.bAckReq = TRUE;
				tsTx.u8Retry = 10;
				tsTx.u8CbId = 0x80;
				tsTx.u8Seq = 0x80;
				tsTx.u8Cmd = PKT_CMD_CONTRL;

				S_OCTET(0); // INFORMATION
				S_OCTET(sAppData.u8channel); // CHANNEL
				S_OCTET(sAppData.bPerAppAckMode); // APP ACK REQUEST
				S_OCTET(sAppData.u8retry); // MAC ACK COUNT
				S_OCTET(sAppData.u8Power); // RF POWER CONFIG
				tsTx.u8Len = q - tsTx.auData;

				ToCoNet_bMacTxReq(&tsTx);
			} else {
				bFlag = TRUE;
			}
		}

		if (eEvent == E_PER_START) {
			bFlag = TRUE;
		}

		if (eEvent == E_EVENT_SLAVE_CONF_FAIL) {
#ifdef USE_SERIAL
			vfPrintf(&sSerStream, "\r\n*** FAILED TO CONFIGURE DESTINATION (PLEASE TRY AGAIN!) ***");
#endif
#ifdef USE_LCD
			u8LcdPage = E_LCD_PAGE_SCAN_FAIL;
			vUpdateLcdOpt();
#endif
			// ERROR
			sAppData.u8ChildConfCh = 0;
			ToCoNet_Event_SetState(pEv, E_STATE_PER_RESCAN_PRE_WAIT);
		}

		if (bFlag) {
			// reconfig RF setting
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			sToCoNet_AppContext.u8TxMacRetry = sAppData.u8retry;
			sToCoNet_AppContext.u8TxPower = sAppData.u8Power; // TODO sAppData.u8Power;

			ToCoNet_vRfConfig();

			ToCoNet_Event_SetState(pEv, E_STATE_PER);

#ifdef USE_SERIAL1
			vfPrintf(&sSerStream, "\r\n*** PER TEST START ***\r\n");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\nCH=%d, RETRY=%d, PAYLOAD=%d, TIMER=%d%s",
					sAppData.u8channel, sAppData.u8retry, sAppData.u8payload,
					sAppData.u8tick_ms, sAppData.bPerAppAckMode ? ", AppAck" : "");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\nADDR=%07x,%04d",
					sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u32addr : 0xFFFFFFF,
					sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u16addr : 0xFFFF);
			vfPrintf(&sSerStream, "\r\n --- DON'T CHANGE SETTING DURING TEST.\r\n");
			SERIAL_vFlush(UART_PORT_MASTER);
#endif
#ifdef USE_LCD
			u8LcdPage = E_LCD_PAGE_PER;
			vUpdateLcdPerMes();
#endif
		}
		break;

	case E_STATE_PER:
		if (eEvent == E_PER_STOP) {
			// force stop measuring
			ToCoNet_Event_SetState(pEv, E_STATE_PER_FINISH);
			break;
		}

		// TRANSMIT NEXT PACKET
		if (eEvent == E_EVENT_TICK_A && !sAppData.bTxBusy) {
			if (sAppData.u16PerCount == 0) {
				u32TickStart = u32TickCount_ms;
			}

			sAppData.u16PerCount++;

			if (sAppData.u16PerCount > sAppData.u16PerCountMax) {
				sAppData.u16PerCount = sAppData.u16PerCountMax;
				ToCoNet_Event_SetState(pEv, E_STATE_PER_FINISH);
				break;
			}

			// transmit!
			tsTxDataApp tsTx;
			memset(&tsTx, 0, sizeof(tsTxDataApp));

			tsTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
			tsTx.u32DstAddr = sAppData.asChilds[sAppData.u8ChildSelect].u16addr;

			tsTx.bAckReq = TRUE;
			tsTx.u8Retry = 0;
			tsTx.u8CbId = (sAppData.u16PerCount) & 0xFF;
			tsTx.u8Seq = (sAppData.u16PerCount) & 0xFF;
			sAppData.u8TxSeq = tsTx.u8Seq;
			sAppData.bRcvAppAck = FALSE;
			tsTx.u8Cmd = sAppData.bPerAppAckMode ? PKT_CMD_APPACK : PKT_CMD_NORMAL;
			tsTx.u8Len = sAppData.u8payload - 4;

			tsTx.u16DelayMin = 0;
			tsTx.u16DelayMax = 0;

			// set payload
			if (tsTx.u8Len > 0) {
				memcpy(tsTx.auData, au8payload, tsTx.u8Len);
			}

			// transmit
			if (ToCoNet_bMacTxReq(&tsTx)) {
				ToCoNet_Tx_vProcessQueue(); // 送信をすみやかに実施する

				sAppData.bTxBusy = TRUE;
			}

			pEv->u32tick_new_state = u32TickCount_ms; // renew transit time
		}

		// TIMEOUT
		if (ToCoNet_Event_u32TickFrNewState(pEv) > 1000) {
			ToCoNet_Event_SetState(pEv, E_STATE_PER_FINISH);
			sAppData.bTxBusy = FALSE;
			break;
		}
		break;

	case E_STATE_PER_FINISH:
		if (ToCoNet_Event_u32TickFrNewState(pEv) > 250) {
			uint32 u32permil, u32permilAppAck, u32dur_ms;

			sAppData.bPerFinish = TRUE;

			u32dur_ms = u32TickCount_ms - u32TickStart - 250; // 経過時間

			u32permil = ((sAppData.u16PerCount - sAppData.u16PerSuccess) * 1000 + 5)
							/ (sAppData.u16PerCount);
			u32permilAppAck = ((sAppData.u16PerCount - sAppData.u16PerSuccessAppAck) * 1000 + 5)
							/ (sAppData.u16PerCount);
#ifdef USE_SERIAL1
			vfPrintf(&sSerStream, "\r\n======== PER FINISH ========");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n%03d/%03d PER=%03d.%1d%%",
					sAppData.u16PerSuccess, sAppData.u16PerCount,
					u32permil / 10, u32permil % 10);
			SERIAL_vFlush(UART_PORT_MASTER);

			if (sAppData.bPerAppAckMode) {
				int iLgi = sAppData.u32PerLqiSum / sAppData.u16PerSuccessAppAck;
				int iLgiDb = -((7*iLgi-1970)/20);

				vfPrintf(&sSerStream, "\r\n%03d/%03d PER(AppAck)=%03d.%1d%%",
						sAppData.u16PerSuccessAppAck, sAppData.u16PerCount,
						u32permilAppAck / 10, u32permilAppAck % 10);
				vfPrintf(&sSerStream, "\r\nLQI %3d/-%2ddBm(est)", iLgi, iLgiDb);
			}

			uint32 u32us = u32dur_ms * 1000 / sAppData.u16PerCount;
			vfPrintf(&sSerStream, "\r\nDur=%dms, %d.%03dms/pkt)",
					u32dur_ms, u32us / 1000, u32us % 1000);

			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n============================");
			SERIAL_vFlush(UART_PORT_MASTER);
#endif
#ifdef USE_LCD
			vUpdateLcdPerMes();
#endif

			ToCoNet_Event_SetState(pEv, E_STATE_IDLE);
		}
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
		sAppData.u32RcOscError = 10000;
		sAppData.u32randcount = 0xffffffffUL;
		sAppData.u32ChildAddr = 0xffffffffUL;
		sAppData.u8channel = CHANNEL;
		sAppData.u8retry = PER_DEF_RETRY;
		sAppData.u8payload = PER_DEF_PAYLOAD;
		sAppData.u16PerCountMax = PER_DEF_COUNT;
		sAppData.u8tick_ms = TIMER_TICK_MS;
		sAppData.u8Power = 3;

	    // release all TIMER IO pins
		vAHI_TimerFineGrainDIOControl(0x7F);

		// 表示言語の選択
		vPortAsInput(PORT_KIT_SW4);
		if (bPortRead(PORT_KIT_SW4)) {
			sAppData.u8Lang = MSG_LANG_JAPANESE;
		} else {
			sAppData.u8Lang = MSG_LANG_ENGLISH;
		}

#ifdef USE_SERIAL
		vSerialInit();
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);
#endif

#ifdef USE_LCD
		vLcdInit(); // register sLcdStream

		vBTM_Init(PORT_KIT_SW1_MASK | PORT_KIT_SW2_MASK | PORT_KIT_SW3_MASK | PORT_KIT_SW4_MASK, E_AHI_DEVICE_TIMER2);
		vBTM_Enable();
#endif

		// configure network
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;
		sToCoNet_AppContext.u16ShortAddress = MASTER_ADDR;

		sToCoNet_AppContext.u8TxMacRetry = sAppData.u8retry;
		sToCoNet_AppContext.bRxOnIdle = TRUE;

#ifdef NO_CCA
		//sToCoNet_AppContext.u8TxPower = 0; // TODO
		sToCoNet_AppContext.u8CCA_Level = 0;
		sToCoNet_AppContext.u8CCA_Retry = 0;
#endif

		// Other Hardware
		vInitHardware(FALSE);

		// MAC start
		ToCoNet_vMacStart();

		// event machine
		ToCoNet_Event_Register_State_Machine(vProcessEvCore); // main state machine
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
	vHandleSerialInput(SERIAL_FROM_QUEUE);

	/* handle LCD operation */
#ifdef USE_LCD
	vHandleButtonInput();
#endif
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
	switch(ToCoNet_Event_eGetState(vProcessEvCore)) {
	case E_STATE_PER:
	case E_STATE_PER_FINISH:
		if (pRx->u32SrcAddr == sAppData.asChilds[sAppData.u8ChildSelect].u16addr) {
			if (!sAppData.bRcvAppAck && sAppData.u8TxSeq == pRx->u8Seq) {
				sAppData.u16PerSuccessAppAck++;
				sAppData.u8PerLqiLast = pRx->u8Lqi;
				sAppData.u32PerLqiSum += pRx->u8Lqi;
				sAppData.bRcvAppAck = TRUE;

				vfPrintf(&sSerStream, "!");
			} else {
				vfPrintf(&sSerStream, "<%d,%d>", sAppData.u8TxSeq, pRx->u8Seq);
			}
		}
		break;

	default:
		break;
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
	switch(ToCoNet_Event_eGetState(vProcessEvCore)) {
	case E_STATE_PER:
	case E_STATE_PER_FINISH:
		if (bStatus) {
			// success transmission
			sAppData.u16PerSuccess++;
		#ifdef USE_SERIAL
				vfPrintf(&sSerStream, ".");
		#endif
		} else {
		#ifdef USE_SERIAL
				vfPrintf(&sSerStream, "x");
		#endif
		}

		sAppData.bTxBusy = FALSE;

		#ifdef USE_LCD
			vUpdateLcdPerMes();
		#endif
		break;

	case E_STATE_PER_INIT:
#ifdef USE_SERIAL
		vfPrintf(&sSerStream, "\n\r** Child respond %d.", bStatus);
#endif
		if(bStatus) {
			sAppData.u8ChildConfCh = sAppData.u8channel;
			ToCoNet_Event_Process(E_PER_START, 0, vProcessEvCore);
		} else {
			sAppData.u8ChildConfCh = 0;
			ToCoNet_Event_Process(E_EVENT_SLAVE_CONF_FAIL, 0, vProcessEvCore);
		}
		break;
	default:
		break;
	}

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
	int i;
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	case E_EVENT_TOCONET_NWK_SCAN_COMPLETE:
		_C {
			tsToCoNet_NbScan_Result *pNbsc = (tsToCoNet_NbScan_Result *)u32arg;

			if (pNbsc->u8scanMode & TOCONET_NBSCAN_NORMAL_MASK) {
				memset(sAppData.asChilds, 0, sizeof(sAppData.asChilds));
				sAppData.u8ChildFound = 0;
				sAppData.u8ChildSelect = 0xff;

				// 全チャネルスキャン結果
				for(i = 0; i < pNbsc->u8found && sAppData.u8ChildFound <= 4; i++) {
					tsToCoNet_NbScan_Entitiy *pEnt = &pNbsc->sScanResult[pNbsc->u8IdxLqiSort[i]];
					if (pEnt->bFound) {
						sAppData.u8ChildFound++;
						sAppData.asChilds[i] = *pEnt;
						if (pEnt->u32addr == sAppData.u32ChildAddr) {
							sAppData.u8ChildSelect = i;
						}
					}
				}

				if (sAppData.u8ChildFound > 0 && sAppData.u8ChildSelect == 0xff) {
					sAppData.u8ChildSelect = 0; // default selection
					sAppData.u32ChildAddr = sAppData.asChilds[0].u32addr;
				}
			} else if (pNbsc->u8scanMode & TOCONET_NBSCAN_QUICK_EXTADDR_MASK) {
				// Ext Addr による検索で、チャネルが特定できた。
				tsToCoNet_NbScan_Entitiy *pEnt = &pNbsc->sScanResult[0];

				sAppData.u8ChildSelect = 0xFF;
				for(i = 0; i < sAppData.u8ChildFound; i++) {
					if (sAppData.asChilds[i].u32addr == pEnt->u32addr) {
						sAppData.u8ChildSelect = i;
						sAppData.asChilds[i].u8ch = pEnt->u8ch;
						break;
					}
				}
			}

			ToCoNet_Event_Process(E_EVENT_SCAN_FINISH, 0, vProcessEvCore);
		}
		break;

	case E_EVENT_TOCONET_ENERGY_SCAN_COMPLETE:
		_C {
			uint8 *pu8Result = (uint8*)u32arg;

			sAppData.u8ChEnergy = pu8Result[1];

			int i, max = 0;
			static uint8 u8Hist[8] = { 0 };
			static uint8 u8Idx = 0;
			u8Hist[(u8Idx++) & 0x7] = pu8Result[1];
			for (i = 0; i < 8; i++) {
				if (u8Hist[i] > max) max = u8Hist[i];
			}
			sAppData.u8ChEnergyMax = max;

			sAppData.bTxBusy = FALSE;

#ifdef USE_LCD
			if (u8LcdPage <= E_LCD_PAGE_LAST) {
				vUpdateLcdOpt();
			}
#endif
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
		_C {
#ifdef USE_SERIAL2
			vfPrintf(&sSerStream, "\n\rEV: ADC_C");
#endif
		}
		break;

	case E_AHI_DEVICE_SYSCTRL:
		if (u32ItemBitmap & E_AHI_SYSCTRL_RNDEM_MASK) {
#ifdef USE_SERIAL2
			vfPrintf(&sSerStream, "\n\rEV:RAND(%d)", sAppData.u32randcount);
#endif
			// read random number
			// vAHI_StopRandomNumberGenerator();
			sAppData.u32randcount = (uint32) u16AHI_ReadRandomNumber();
		}
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		_C {
			static uint32 u32TickLast;

			// LCD updator
			if (u32TickCount_ms - u32TickLast > 100) {
				u32TickLast = u32TickCount_ms;
#ifdef USE_LCD
				vLcdClear();
				vDrawLcdDisplay(0, TRUE); /* write to lcd module */
				vLcdRefreshAll(); /* display new data */
#endif

				if (ToCoNet_Event_eGetState(vProcessEvCore) == E_STATE_IDLE) {
					// vfPrintf(&sSerStream, LB "EnergyScan: %d t=%d", sAppData.u8channel, u32TickCount_ms & 0x3fff);

#ifdef USE_LCD		// 不要なタイミングでは EnergyScan しない
					if (sAppData.bEnergyScanEnabled) {
						ToCoNet_EnergyScan_bStart(1 << sAppData.u8channel, 2); // 2 will take around 64ms, approx.
					}
#endif
					sAppData.bTxBusy = TRUE;
				}
			}
		}
#ifdef USE_SERIAL2
		vfPrintf(&sSerStream, "$");
#endif
		break;

	case E_AHI_DEVICE_TIMER0:
#ifdef USE_SERIAL2
		vfPrintf(&sSerStream, "$");
#endif
		ToCoNet_Event_Process(E_EVENT_TICK_A, 0, vProcessEvCore);
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
static void vInitHardware(int f_warm_start) {
	// LED's
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);
	vPortAsOutput(PORT_KIT_LED3);
	vPortAsOutput(PORT_KIT_LED4);
	vPortSetHi(PORT_KIT_LED1);
	vPortSetHi(PORT_KIT_LED2);
	vPortSetHi(PORT_KIT_LED3);
	vPortSetHi(PORT_KIT_LED4);

	//

	// activate random generator
	vAHI_StartRandomNumberGenerator(E_AHI_RND_SINGLE_SHOT, E_AHI_INTS_ENABLED);

	// activate tick timers
	memset(&sTimerApp, 0, sizeof(sTimerApp));
	sTimerApp.u8Device = E_AHI_DEVICE_TIMER0;
	sTimerApp.u16Hz = 1000 / sAppData.u8tick_ms;
	sTimerApp.u8PreScale = 7;
	vTimerConfig(&sTimerApp);
	vTimerStart(&sTimerApp);

#ifdef USE_SERIAL1
	vfPrintf(&sSerStream, "\r\n*** TWELITE NET PER MASTER %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	vfPrintf(&sSerStream, "\r\n  press 'h' to show usage.\r\n");
	SERIAL_vFlush(UART_PORT_MASTER);
#endif
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
#ifdef USE_SERIAL
void vSerialInit(void) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[96];
	static uint8 au8SerialRxBuffer[32];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = UART_BAUD;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT_MASTER;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInit(&sSerPort);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_MASTER;
}
#endif

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
#ifdef USE_SERIAL
static void vHandleSerialInput(int16 i16CharExt) {
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)
			|| i16CharExt != SERIAL_FROM_QUEUE) {
		int16 i16Char;

		if (i16CharExt != SERIAL_FROM_QUEUE) {
			i16Char = i16CharExt;
			i16CharExt = SERIAL_FROM_QUEUE;
		} else {
			i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);
		}

		vfPrintf(&sSerStream, "\n\r# [%c] --> ", i16Char);
		switch (i16Char) {

		case '\r':
		case 'h':
		case 'H':
			vfPrintf(&sSerStream, "\f\r\nUSAGE: ");
			vfPrintf(&sSerStream, "\r\n 'n' or 'N' --> RESCAN NODES");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n '0', '1-4' --> SHOW, CHOOSE NODES");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'a' or 'A' --> SET APPACK");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'c' or 'C' --> SET TX COUNT");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'p' or 'P' --> SET PAYLOAD SIZE");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n '>', '<'   --> CH UP, DOWN");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'r' or 'R' --> SET RETRY COUNT");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 't' or 'T' --> SET TIMER ms");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'o' or 'O' --> SET Power");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 's' or SPACE --> START PER TEST");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\n 'b' or 'B'   --> STOP PER TEST");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, "\r\nCONFIG:\r\nCh=%d,Rt=%d,Py=%d,Tt=%d%s",
					sAppData.u8channel, sAppData.u8retry, sAppData.u8payload,
					sAppData.u8tick_ms, sAppData.bPerAppAckMode ? ", AppAck" : "");
			SERIAL_vFlush(UART_PORT_MASTER);
			vfPrintf(&sSerStream, " ,ADDR=%07x/%04x",
					sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u32addr : 0xFFFFFFF,
					sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u16addr : 0xFFFF);
			SERIAL_vFlush(UART_PORT_MASTER);

			break;

		case 's':
		case 'S':
		case ' ':
			_C {
				ToCoNet_Event_Process(E_PER_START, 0, vProcessEvCore);
			}
			break;


		case 'b':
		case 'B':
			_C {
				ToCoNet_Event_Process(E_PER_STOP, 0, vProcessEvCore);
			}
			break;

		case 'n': case 'N':
			_C {
				ToCoNet_Event_Process(E_PER_RESCAN_SLAVE, 0, vProcessEvCore);
				//ToCoNet_bNeighbourScanStart(0, 128);
			}
			break;

		case '>':
		case '.':
			/* channel up */
			sAppData.u8channel++;
			if (sAppData.u8channel > MAX_CHANNEL)
				sAppData.u8channel = 11;
			sAppData.u8ChildConfCh = 0;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case '<':
		case ',':
			/* channel down */
			sAppData.u8channel--;
			if (sAppData.u8channel < 11)
				sAppData.u8channel = MAX_CHANNEL;
			sAppData.u8ChildConfCh = 0;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case 'a':
		case 'A':
			sAppData.bPerAppAckMode = sAppData.bPerAppAckMode ? FALSE : TRUE;
			sAppData.u8ChildConfCh = 0;
			vfPrintf(&sSerStream, "%sset AppAck mode.", sAppData.bPerAppAckMode ? "" : "un");
			break;

		case 'r':
		case 'R':
			sAppData.u8retry++;
			if (sAppData.u8retry > 7)
				sAppData.u8retry = 0;
			sAppData.u8ChildConfCh = 0;
			vfPrintf(&sSerStream, "set retry to %d.", sAppData.u8retry);
			break;

		case 'c':
		case 'C':
			_C {
				const uint16 au16count_table[] = { PER_DEF_COUNT, 100, 200, 500,
						1000, 2000, 5000, 10000, 20000, 50000, 0 };
				static uint8 u8idx = 0;

				u8idx++;
				if (au16count_table[u8idx] == 0)
					u8idx = 1;

				sAppData.u16PerCountMax = au16count_table[u8idx];
				vfPrintf(&sSerStream, "set PER count to %d.",
						sAppData.u16PerCountMax);
			}
			break;

		case 'p':
		case 'P':
			_C {
				const uint16 au16count_table[] = {
						PER_DEF_PAYLOAD, 5, 6, 7, 8, 10,
						12, 16, 20, 25, 32, 50, 64, 80, 96, 108, 0 };
				static uint8 u8idx = 0;

				u8idx++;
				if (au16count_table[u8idx] == 0)
					u8idx = 1;

				sAppData.u8payload = au16count_table[u8idx];
				vfPrintf(&sSerStream, "set payload to %d bytes.",
						sAppData.u8payload);
			}
			break;

		case 'o':
		case 'O':
			_C {
				const uint16 au16pow_table[] = { 3, 0, 1, 2, 3, 255 };
				static uint8 u8idx = 0;

				u8idx++;
				if (au16pow_table[u8idx] == 255) {
					u8idx = 1;
				}

				sAppData.u8ChildConfCh = 0;
				sAppData.u8Power = au16pow_table[u8idx];
				vfPrintf(&sSerStream, "set power to %d [3:max, 0:min].",
						sAppData.u8Power);
			}
			break;

		case 't':
		case 'T':
			_C {
				const uint16 au16count_table[] = {
						TIMER_TICK_MS,
						0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 33, 50, 67,
						100, 250, 65535 };
				static uint8 u8idx = 0;

				u8idx++;
				if (au16count_table[u8idx] == 65535)
					u8idx = 1;

				sAppData.u8tick_ms = au16count_table[u8idx];

				// タイマーを定義
				if (sAppData.u8tick_ms == 0) {
					// 厳密には 0 ではないが 10kHz でも十分パケット間隔が詰まるはず
					sTimerApp.u16Hz = 10000;
				} else {
					// 指定された ms で動作
					sTimerApp.u16Hz = 1000 / sAppData.u8tick_ms;
				}

				vTimerStop(&sTimerApp);
				vTimerStart(&sTimerApp);

				vfPrintf(&sSerStream, "set time tick to %d ms.",
						sAppData.u8tick_ms);
			}
			break;

		case 'd': case 'D':
			_C {
				static uint8 u8DgbLvl;

				u8DgbLvl++;
				if(u8DgbLvl > 5) u8DgbLvl = 0;
				ToCoNet_vDebugLevel(u8DgbLvl);

				vfPrintf(&sSerStream, "set NwkCode debug level to %d.", u8DgbLvl);
			}
			break;

		case '1':
		case '2':
		case '3':
		case '4':
			sAppData.u8ChildSelect = i16Char - '1';
			if (sAppData.asChilds[sAppData.u8ChildSelect].bFound) {
				vfPrintf(&sSerStream, "set child as %07x,%04x.",
						sAppData.asChilds[sAppData.u8ChildSelect].u32addr,
						sAppData.asChilds[sAppData.u8ChildSelect].u16addr);
				sAppData.u32ChildAddr = sAppData.asChilds[sAppData.u8ChildSelect].u32addr;
			} else {
				vfPrintf(&sSerStream, "invalid child number (select #1)");
			}
			break;

		case '0':
			_C {
				int i;
				vfPrintf(&sSerStream, "slave device(s) (press 1 to 4)");
				for (i = 0; i < sAppData.u8ChildFound; i++) {
					tsToCoNet_NbScan_Entitiy *pEnt = &(sAppData.asChilds[i]);
					vfPrintf(&sSerStream, "\r\n%d%c %07x,%04x Ch:%02d Lq:%03d",
							i + 1,
							i == sAppData.u8ChildSelect ? '*' : ':',
							pEnt->u32addr,
							pEnt->u16addr,
							pEnt->u8ch,
							pEnt->u8lqi
					);
				}
			}
			break;

		case 'e':
		case 'E':
			if (ToCoNet_Event_eGetState(vProcessEvCore) == E_STATE_IDLE) {
				vfPrintf(&sSerStream, LB "EnergyScan: %d max(%d)", sAppData.u8ChEnergy, sAppData.u8ChEnergyMax);
			}
			break;

		default:
			break;
		}

		//vfPrintf(&sSerStream, "\n\r");
	}
}
#endif

#ifdef USE_LCD
/****************************************************************************
 *
 * NAME: vLcdInit
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vLcdInit(void) {
	/* Initisalise the LCD */
	vLcdReset(3, 0);

	/* register for vfPrintf() */
	sLcdStream.bPutChar = LCD_bTxChar;
	sLcdStream.u8Device = 0xFF;
	sLcdStreamBtm.bPutChar = LCD_bTxBottom;
	sLcdStreamBtm.u8Device = 0xFF;
}

/****************************************************************************
 *
 * NAME: vHandleButtonInput
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vHandleButtonInput(void) {
	uint32 bmPorts, bmChanged;

	if (bBTM_GetState(&bmPorts, &bmChanged)) {
		if (u32TickCount_ms - u32LastTickPressed > 250 && bmPorts) {
			u32LastTickPressed = u32TickCount_ms;

			switch (u8LcdPage) {
			case E_LCD_PAGE_OPT0:
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					u8LcdPage++;
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
					vHandleSerialInput('<');
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
					vHandleSerialInput('>');
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					u8LcdPage = E_LCD_PAGE_PER;
					vHandleSerialInput('s');
				}
				break;

			case E_LCD_PAGE_OPT1:
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					u8LcdPage++;
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
					vHandleSerialInput('p');
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
					vHandleSerialInput('t');
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					vHandleSerialInput('c');
				}
				break;

			case E_LCD_PAGE_OPT2:
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					u8LcdPage++; // back to the first page
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
					vHandleSerialInput('r');
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
					vHandleSerialInput('a');
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					vHandleSerialInput('o');
				}
				break;

			case E_LCD_PAGE_LAST:
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					u8LcdPage = E_LCD_PAGE_OPT0; // back to the first page
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					u8LcdPage = E_LCD_PAGE_PER;
					vHandleSerialInput('s');
				}
				break;

			case E_LCD_PAGE_SCAN_INIT: // opening
				break;

			case E_LCD_PAGE_SCAN_SUCCESS: // select neighbour
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					if (sAppData.asChilds[0].bFound) {
						vHandleSerialInput('1');
						u8LcdPage = E_LCD_PAGE_OPT0;
					}
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
					if (sAppData.asChilds[1].bFound) {
						vHandleSerialInput('2');
						u8LcdPage = E_LCD_PAGE_OPT0;
					}
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
					if (sAppData.asChilds[2].bFound) {
						vHandleSerialInput('3');
						u8LcdPage = E_LCD_PAGE_OPT0;
					}
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					if (sAppData.asChilds[3].bFound) {
						vHandleSerialInput('4');
						u8LcdPage = E_LCD_PAGE_OPT0;
					}
				}

				break;

			case E_LCD_PAGE_SCAN_FAIL:
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					vHandleSerialInput('n');
					u8LcdPage = E_LCD_PAGE_SCAN_INIT;
				}
				break;

			case E_LCD_PAGE_PER: // measuring
				// BUTTON PRESSED
				if (bmPorts & PORT_KIT_SW1_MASK) { // SW1
					if (sAppData.bPerFinish) {
						u8LcdPage = E_LCD_PAGE_OPT0; // set default
					} else {
						return; // no update screen.
					}
				}
				if (bmPorts & PORT_KIT_SW2_MASK) { // SW2
					return; // no update screen.
				}
				if (bmPorts & PORT_KIT_SW3_MASK) { // SW3
					return; // no update screen.
				}
				if (bmPorts & PORT_KIT_SW4_MASK) { // SW4
					if (sAppData.bPerFinish) {
						vHandleSerialInput('s');
					} else {
						vHandleSerialInput('b');
					}
				}
				break;

			default:
				u8LcdPage = 0;
				break;
			}

			vUpdateLcdOpt(u8LcdPage);
		}
	}
}

/****************************************************************************
 *
 * NAME: vUpdateLcdOpt
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vUpdateLcdOpt() {
	vfPrintf(&sLcdStream, "\f< %s PER %d.%02d-%d >\n\r",
			              // 012567b123 45   67 c1234567
			sAppData.u8Lang == MSG_LANG_JAPANESE ? (char*)au8Tocos : "MONO WIRELESS",
			VERSION_MAIN, VERSION_SUB, VERSION_VAR);

	// update bars with max hold
	uint8 au8EnergyBar[11];
	{
		int i, j;

		for (i = 0; i < 10; i++) {
			au8EnergyBar[i] = CHR_BLK_50;
		}
		au8EnergyBar[10] = '\0';
		for (i = 0, j = (256/10)/2; i < 10; i++, j += (256/10)) {
			au8EnergyBar[i] = sAppData.u8ChEnergy >= j ?  CHR_BLK : CHR_BLK_50;
			if (i > 0 && sAppData.u8ChEnergyMax < j) {
				au8EnergyBar[i - 1] = CHR_BLK;
				break;
			}
		}
	}

	sAppData.bEnergyScanEnabled = FALSE;
	if (u8LcdPage <= E_LCD_PAGE_LAST) {
		sAppData.bEnergyScanEnabled = TRUE;

		vfPrintf(&sLcdStream,
				"%s%c %2d %s%c |%07x\n\r"
				//1234567 012 34  6701..E
				"%s%c %d byte%s       |sh=%04x|\n\r"
				//1234567 01 234567012345670123456E
				"%s%c %d ms\n\r"
				"%s%c %d\n\r"
				"%s%c %d\n\r"
				"%s%c %s | Pwr%c %d\n\r"
				"",
				MSG_TBL_SETTINGS[0][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT0 ? '*' : ':',
						sAppData.u8channel,
						au8EnergyBar,
						CHR_BLK,
						sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u32addr : 0xFFFFFFF,

				MSG_TBL_SETTINGS[1][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT1 ? '*' : ':',
						sAppData.u8payload,
						sAppData.u8payload < 10 ? "  " : (sAppData.u8payload < 100 ? " " : ""),
						sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u16addr : 0xFFFF,

				MSG_TBL_SETTINGS[2][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT1 ? '*' : ':', sAppData.u8tick_ms,

				MSG_TBL_SETTINGS[3][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT1 ? '*' : ':', sAppData.u16PerCountMax,

				MSG_TBL_SETTINGS[4][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT2 ? '*' : ':', sAppData.u8retry,

				MSG_TBL_SETTINGS[5][sAppData.u8Lang],
				u8LcdPage == E_LCD_PAGE_OPT2 ? '*' : ':', sAppData.bPerAppAckMode ? "Yes" : "No",

				u8LcdPage == E_LCD_PAGE_OPT2 ? '*' : ':', sAppData.u8Power // TODO: MSG_TBL_SETTINGS[] に対応
			);

		vfPrintf(&sLcdStreamBtm, "%s\n", MSG_TBL_OPTS[u8LcdPage][sAppData.u8Lang]);
	} else
	if (u8LcdPage == E_LCD_PAGE_SCAN_INIT) {
		vfPrintf(&sLcdStream,
				MSG_TBL_WAIT_A_MINUTE[sAppData.u8Lang]);

		vfPrintf(&sLcdStreamBtm, MSG_TBL_CHILD_SCANNING[sAppData.u8Lang]);
	} else
	if (u8LcdPage == E_LCD_PAGE_SCAN_SUCCESS) {
		// child found
		vfPrintf(&sLcdStream,
				MSG_TBL_CHILD_SELECT[sAppData.u8Lang]);

		int i;
		for (i = 0; i < 4; i++) {
			tsToCoNet_NbScan_Entitiy *pEnt = &sAppData.asChilds[i];

			if (pEnt->bFound) {
				vfPrintf(&sLcdStream, "%d: %07x,%04x Ch:%02d Lq:%03d\n\r",
						i + 1,
						pEnt->u32addr,
						pEnt->u16addr,
						pEnt->u8ch,
						pEnt->u8lqi
				);
			} else {
				vfPrintf(&sLcdStream, MSG_TBL_CHILD_NOT_AVAILABLE[sAppData.u8Lang],
						i + 1
				);
			}

			vfPrintf(&sLcdStreamBtm, "%s%c",
					i == 0 ? "   " : "       ",
					pEnt->bFound ? '1' + i : '-');
		}

		vfPrintf(&sLcdStreamBtm, "\r\n");
	} else
	if (u8LcdPage == E_LCD_PAGE_SCAN_FAIL) {
		// child not found
		vfPrintf(&sLcdStream,
				MSG_TBL_CHILD_NOT_FOUND[sAppData.u8Lang]);

		vfPrintf(&sLcdStreamBtm,
				"                         ReScan \n");
	}
}

/****************************************************************************
 *
 * NAME: vUpdateLcdPerMes
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vUpdateLcdPerMes() {
	sAppData.bEnergyScanEnabled = FALSE;

	//vLcdClear();
	uint32 u32permil = 0;
	uint32 u32permilAppAck = 0;
	if (sAppData.u16PerCount > 2) {
		u32permil = ((sAppData.u16PerCount - sAppData.u16PerSuccess) * 1000 + 5)
				/ (sAppData.u16PerCount);
		u32permilAppAck = ((sAppData.u16PerCount - sAppData.u16PerSuccessAppAck) * 1000 + 5)
				/ (sAppData.u16PerCount);
	}
	uint32 u32pgr = ((sAppData.u16PerCount) * 100)
			/ sAppData.u16PerCountMax;

	vfPrintf(&sLcdStream,
			//TEMPLATE:0123456701234567012345670123456E
			"\f< %s PER %d.%02d-%d >\n\r"
			"%s: %5d/%d%s     |%07x\n\r"
		    //12345670.456..23456701..E
			"%s: %5d           |Sh=%04x\n\r"
		    //12345670.4567012345670123456E
			"%s: %3d.%01d%% |Ch=%d Rt=%d Po=%d\n\r"
			//123456701234   5 670123 4567012 3456 E
			"",
			sAppData.u8Lang == MSG_LANG_JAPANESE ? (char*)au8Tocos : "MONO WIRELESS"
				, VERSION_MAIN, VERSION_SUB, VERSION_VAR,

			MSG_TBL_MESURE[0][sAppData.u8Lang],
			sAppData.u16PerCount, sAppData.u16PerCountMax, sAppData.u16PerCountMax < 1000 ? "  " : (sAppData.u16PerCountMax < 10000 ? " " : ""),
			sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u32addr : 0xFFFFFFF,

			MSG_TBL_MESURE[1][sAppData.u8Lang],
			sAppData.u16PerSuccess,
			sAppData.u8ChildSelect != 0xFF ? sAppData.asChilds[sAppData.u8ChildSelect].u16addr : 0xFFFF,

			MSG_TBL_MESURE[2][sAppData.u8Lang],
			u32permil / 10, u32permil % 10, sAppData.u8channel, sAppData.u8retry, sAppData.u8Power
			);

	if (sAppData.bPerAppAckMode) {
		int i;
		vfPrintf(&sLcdStream,
				"%s: %5d  |Py=%d Ti=%d\r\n"
				//12345670.4567012345 670123456E
				"%s: %3d.%01d%%\r\n"
				//123456701234   5 67012345670123456E
				"",
				MSG_TBL_MESURE[3][sAppData.u8Lang],
				sAppData.u16PerSuccessAppAck, sAppData.u8payload, sAppData.u8tick_ms,

				MSG_TBL_MESURE[2][sAppData.u8Lang],
				u32permilAppAck / 10, u32permilAppAck % 10
				);

		int iLgi = sAppData.u32PerLqiSum / sAppData.u16PerSuccessAppAck;
		int iLgiDb = -((7*iLgi-1970)/20);
		vfPrintf(&sLcdStream, "LQI  %3d/-%2ddBm ", iLgi, iLgiDb);
		if (sAppData.bPerFinish) {
			for (i = 0; i < 256; i += 16) {
				sLcdStream.bPutChar(0, sAppData.u32PerLqiSum / sAppData.u16PerSuccessAppAck >= i ? CHR_BLK : CHR_BLK_50);
			}
		} else {
			for (i = 0; i < 256; i += 16) {
				sLcdStream.bPutChar(0, sAppData.u8PerLqiLast >= i ? CHR_BLK : CHR_BLK_50);
			}
		}
		vfPrintf(&sLcdStream, "%c\r\n", CHR_BLK);
	} else {
		vfPrintf(&sLcdStream,
				"               |Py=%d Ti=%d\r\n"
				//123456701234567012345670123456E
				"",
				sAppData.u8payload, sAppData.u8tick_ms);
	}

	if (sAppData.bPerFinish) {
		uint8 u8msgidx = 0;
		uint8 u8percent = 100 - (sAppData.bPerAppAckMode ?
					(u32permilAppAck + 5) / 10 : (u32permil + 5) / 10);

		// PERFECT
		if (u8percent >= 98) u8msgidx++;
		// EXCELLENT
		if (u8percent >= 95) u8msgidx++;
		// FINE
		if (u8percent >= 85) u8msgidx++;
		// GOOD
		if (u8percent >= 70) u8msgidx++;
		// FAIR
		if (u8percent >= 50) u8msgidx++;
		// POOR
		if (u8percent >= 20) u8msgidx++;
		// VeryPOOR
		if (u8percent >= 1) u8msgidx++;
		// HOPELESS


		vfPrintf(&sLcdStreamBtm, " %s   %3d%%:%s  %s \n",
				MSG_TBL_BACK[sAppData.u8Lang],
				100 - u8percent,
				MSG_TBL_EVALUATION[u8msgidx][sAppData.u8Lang],
				MSG_TBL_START[sAppData.u8Lang]
				);
	} else {
		int i;
		vfPrintf(&sLcdStreamBtm, " %3d%% ", u32pgr);
		for (i = 5; i < 100; i += 10) {
			sLcdStreamBtm.bPutChar(0, u32pgr >= i ? CHR_BLK : CHR_BLK_50);
		}
		vfPrintf(&sLcdStreamBtm, "%c          %s \n", CHR_BLK, MSG_TBL_STOP[sAppData.u8Lang]);
		//        01234567012345670123456701234567

	}
}

#endif

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
