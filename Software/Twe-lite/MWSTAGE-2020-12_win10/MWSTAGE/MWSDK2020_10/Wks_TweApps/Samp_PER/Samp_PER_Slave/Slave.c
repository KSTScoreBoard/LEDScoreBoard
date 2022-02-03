/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>


#include "utils.h"

#include "Slave.h"
#include "config.h"

// DEBUG options
#define USE_SERIAL
#ifdef USE_SERIAL
#define USE_SERIAL1
# include <serial.h>
# include <fprintf.h>
#endif

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
#undef ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#define ToCoNet_USE_MOD_NBSCAN_SLAVE

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

typedef struct
{
    //
    uint16 u16RndCt;

    // Transmit Power
    uint8 u8Power;

    // MAC retry
    uint8 u8MacRetry;

    // AppAck
    uint8 bAppAck;

    // MAC
    uint8 u8channel;
    uint16 u16addr;

    // PER
    uint8 u8payload_size;

    // LED Counter
    uint32 u32LedCt;
} tsAppData;


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

PRIVATE void vInitHardware(int f_warm_start);

#ifdef USE_SERIAL
void vSerialInit(void);
PRIVATE void vHandleSerialInput(void);
#endif

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
/* Version/build information. This is not used in the application unless we
   are in serial debug mode. However the 'used' attribute ensures it is
   present in all binary files, allowing easy identifaction... */

/* Local data used by the tag during operation */
PRIVATE tsAppData sAppData;

#ifdef USE_SERIAL
    PUBLIC tsFILE sSerStream;
    tsSerialPortSetup sSerPort;
#endif

// PER
const uint8 au8payload[] =
		"0123456789a123456789b123456789c123456789d123456789e123456789f123456789g123456789h123456789i123456789j123456789k123456789";

// Wakeup port
const uint32 u32DioPortWakeUp = 1UL << 7; // UART Rx Port

/****************************************************************************
 *
 * NAME: AppColdStart
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
PUBLIC void cbAppColdStart(bool_t bAfterAhiInit)
{
	//static uint8 u8WkState;
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
#if 0
		u8WkState = 0;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
    	if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
    		u8WkState = 1;
		}
#endif

		// Register modules
		ToCoNet_REG_MOD_ALL();

	} else {
#if 0
		// Check for Sleep RAM ON
	    if (u8AHI_PowerStatus() & 0x01) {
	    	u8WkState |= 0x80;
	    }
#endif

		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// clear application context
		memset (&sAppData, 0x00, sizeof(sAppData));
		sAppData.u8channel = 18;
		sAppData.u8Power = 3; // MAX
		sAppData.u8MacRetry = 3; // DEFAULT

		// ToCoNet configuration
		sToCoNet_AppContext.u32AppId = APP_ID;
		sToCoNet_AppContext.u8Channel = CHANNEL;

		sToCoNet_AppContext.bRxOnIdle = TRUE;

		// Register
		ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// Others
		vInitHardware(FALSE);

		// MAC start
		ToCoNet_vMacStart();

#if 0 // USE_SERIAL
		vfPrintf(&sSerStream, LB "WkStatus %02x.", u8WkState);
#endif
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
PUBLIC void cbAppWarmStart(bool_t bAfterAhiInit)
{
	static bool_t bWakeupByButton;(void)bWakeupByButton;

	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.
		if(u8AHI_WakeTimerFiredStatus()) {
			bWakeupByButton = FALSE;
		} else
		if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
			bWakeupByButton = TRUE;
		} else {
			bWakeupByButton = FALSE;
		}
	} else {
		//
		vAHI_DioInterruptEnable(u32DioPortWakeUp, 0);
		vAHI_DioInterruptEnable(0, u32DioPortWakeUp);

		// Initialize hardware
		vInitHardware(TRUE);

		// MAC start
		ToCoNet_vMacStart();

#if 0 // USE_SERIAL
		vfPrintf(&sSerStream, LB "Wake up by %s.", bWakeupByButton ? "UART PORT" : "WAKE TIMER");
#endif
	}
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
PUBLIC void cbToCoNet_vMain(void)
{
	/* handle uart input */
	vHandleSerialInput();
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
	default:
		break;
	}
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
	//uint8 *p = pRx->auData;

	// print coming payload
	vfPrintf(&sSerStream, "\n\r[PKT Ad:%04x,Ln:%03d,Cmd:%1d,Seq:%03d,Lq:%03d,Tms:%05d \"HHS",
			pRx->u32SrcAddr,
			pRx->u8Len+4, // Actual payload byte: the network layer uses additional 4 bytes.
			pRx->u8Cmd,
			pRx->u8Seq,
			pRx->u8Lqi,
			pRx->u32Tick & 0xFFFF);
	for (i = 0; i < pRx->u8Len; i++) {
		if (i < 32) {
			sSerStream.bPutChar(sSerStream.u8Device,
					(pRx->auData[i] >= 0x20 && pRx->auData[i] <= 0x7f) ? pRx->auData[i] : '.');
		} else {
			vfPrintf(&sSerStream, "..");
			break;
		}
	}
	vfPrintf(&sSerStream, "C\"]");

	// Channel change command
	if (pRx->u8Cmd == PKT_CMD_CONTRL && pRx->u8Len > 0) {
		uint8 *p = &pRx->auData[1];

		OCTET(sAppData.u8channel);
		OCTET(sAppData.bAppAck);
		OCTET(sAppData.u8MacRetry);
		OCTET(sAppData.u8Power);

		if (   sToCoNet_AppContext.u8Channel != sAppData.u8channel
			|| sToCoNet_AppContext.u8TxMacRetry != sAppData.u8MacRetry
			|| sToCoNet_AppContext.u8TxPower != sAppData.u8Power) {
			vfPrintf(&sSerStream, "\r\n[RfConfRequest Ch%02d/Re%d/Po%d]", sAppData.u8channel, sAppData.u8MacRetry, sAppData.u8Power);

			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			sToCoNet_AppContext.u8TxMacRetry = sAppData.u8MacRetry;
			sToCoNet_AppContext.u8TxPower = sAppData.u8Power;
			ToCoNet_vRfConfig();
		}
	} else {
		// AppAck 要求が来た時
		if (pRx->u8Cmd == PKT_CMD_APPACK) {
			// transmit Ack back
			tsTxDataApp tsTx;
			memset(&tsTx, 0, sizeof(tsTxDataApp));

			tsTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress; // short address mode.
			tsTx.u32DstAddr = pRx->u32SrcAddr; // fixed address

			tsTx.bAckReq = TRUE;
			tsTx.u8Retry = 0;
			tsTx.u8CbId = pRx->u8Seq;
			tsTx.u8Seq = pRx->u8Seq;
			tsTx.u8Len = pRx->u8Len;
			tsTx.u8Cmd = PKT_CMD_APPACK;
			if (tsTx.u8Len > 0) {
				memcpy(tsTx.auData, pRx->auData, tsTx.u8Len);
			}
			vfPrintf(&sSerStream, "\r\nPARROT %d!", pRx->u8Seq);
			ToCoNet_bMacTxReq(&tsTx);
		}

		// turn on Led a while
		sAppData.u32LedCt = u32TickCount_ms;
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
	return;
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
PUBLIC void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap)
{
    switch (u32DeviceId) {
    case E_AHI_DEVICE_SYSCTRL:
		if (u32ItemBitmap & E_AHI_SYSCTRL_RNDEM_MASK) {
			// read random number
			sAppData.u16RndCt = (uint32)u16AHI_ReadRandomNumber();
		}
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
	return FALSE;
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
PRIVATE void vInitHardware(int f_warm_start)
{
	// Serial Initialize
#ifdef USE_SERIAL
	vSerialInit();
	ToCoNet_vDebugInit(&sSerStream);
	ToCoNet_vDebugLevel(0);
#endif

	/// IOs
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);
	vPortSetLo(PORT_KIT_LED1); // TODO
	vPortSetHi(PORT_KIT_LED2);

    // activate random generator
	vAHI_StartRandomNumberGenerator(
			E_AHI_RND_SINGLE_SHOT,
			E_AHI_INTS_ENABLED);

#ifdef USE_SERIAL1
    vfPrintf(&sSerStream, "\r\n*** TWELITE NET PER SLAVE %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
    vfPrintf(&sSerStream, "\r\n*** %08x ***", ToCoNet_u32GetSerial());

    SERIAL_vFlush(sSerStream.u8Device);
#endif
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
	sSerPort.u8SerialPort = UART_PORT_SLAVE;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInit(&sSerPort);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_SLAVE;
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
PRIVATE void vHandleSerialInput(void)
{
    // handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		vfPrintf(&sSerStream, "\n\r# [%c] --> ", i16Char);
	    SERIAL_vFlush(sSerStream.u8Device);

		switch(i16Char) {

		case '>': case '.':
			/* channel up */
			sAppData.u8channel++;
			if (sAppData.u8channel > 26) sAppData.u8channel = 11;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
			break;

		case '<': case ',':
			/* channel down */
			sAppData.u8channel--;
			if (sAppData.u8channel < 11) sAppData.u8channel = 26;
			sToCoNet_AppContext.u8Channel = sAppData.u8channel;
			ToCoNet_vRfConfig();
			vfPrintf(&sSerStream, "set channel to %d.", sAppData.u8channel);
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

		case 's': // SLEEP TEST
			_C {
				const uint16 u16SleepDur_ms = 5000;

				// Send Sleep Request to the event machine.
				ToCoNet_Event_Process(E_EVENT_SLEEP_REQUEST, 0, vProcessEvCore);

				// print message.
				vfPrintf(&sSerStream, "sleeping periodic %dms, RAM OFF, WK0", u16SleepDur_ms);
				SERIAL_vFlush(sSerStream.u8Device);
				vAHI_UartDisable(sSerStream.u8Device);

				// stop interrupt source, if interrupt source is still running.
				;

				// set UART Rx port as interrupt source
				vAHI_DioSetDirection(u32DioPortWakeUp, 0); // set as input
				(void)u32AHI_DioInterruptStatus(); // clear interrupt register
				vAHI_DioWakeEnable(u32DioPortWakeUp, 0); // also use as DIO WAKE SOURCE
				// vAHI_DioWakeEnable(0, u32DioPortWakeUp); // DISABLE DIO WAKE SOURCE

				// wake up using wakeup timer as well.
				ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u16SleepDur_ms, TRUE, TRUE); // PERIODIC RAM OFF SLEEP USING WK0
			}
			break;

		case 'S': // SLEEP TEST
			_C {
				const uint16 u16SleepDur_ms = 1000;

				// Send Sleep Request to the event machine.
				ToCoNet_Event_Process(E_EVENT_SLEEP_REQUEST, 1, vProcessEvCore);

				// print message.
				vfPrintf(&sSerStream, "short sleep for %dms, RAM ON, WK1", u16SleepDur_ms);
				SERIAL_vFlush(sSerStream.u8Device);
				vAHI_UartDisable(sSerStream.u8Device);

				// stop interrupt source, if interrupt source is still running.
				;

				// set wake up interrupt
				vAHI_DioSetDirection(u32DioPortWakeUp, 0); // set those ports as input
				// (void)u32AHI_DioInterruptStatus(); // clear interrupt register
					// if DIO state was changed during sleep, u32AHI_DioWakeStatus()
					// would have value... (ONLY for RAN_OFF sleep)
				vAHI_DioWakeEnable(0, u32DioPortWakeUp); // disable DIO WAKE SOURCE

				// wake up using wakeup timer as well.'
				ToCoNet_vSleep(E_AHI_WAKE_TIMER_1, u16SleepDur_ms, FALSE, FALSE); // RAM ON SLEEP USING WK1
			}
			break;

#if 0
		case 'i':
			_C {
				extern uint8 u8WkIdx;
				extern uint32 u32WkId[];
				extern uint32 u32WkBm[];
				int i;

				for (i = 0; i < u8WkIdx; i++) {
					vfPrintf(&sSerStream, LB "%d, %08x", u32WkId[i], u32WkBm[i]);
				}
				u8WkIdx = 0;
			}
			break;
#endif

		default:
			break;
		}

		vfPrintf(&sSerStream, "\n\r");
	    SERIAL_vFlush(sSerStream.u8Device);
	}
}
#endif

/****************************************************************************
 *
 * NAME: vProcessEvent
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
PRIVATE void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	// sleeping behavior
	if (eEvent == E_EVENT_SLEEP_REQUEST && u32evarg == 1) {
		ToCoNet_Event_vKeepStateOnRamHoldSleep(pEv); // hold state
		return;
	}

    switch (pEv->eState)
    {
        case E_STATE_IDLE:
        	if (eEvent == E_EVENT_START_UP) {
#ifdef USE_SERIAL
				vfPrintf(&sSerStream, LB "[E_STATE_IDLE]");
#endif

				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			} else {
				;
			}
        	break;

        case E_STATE_RUNNING:
    		if (eEvent == E_EVENT_NEW_STATE) {
#ifdef USE_SERIAL
				vfPrintf(&sSerStream, LB "[E_STATE_RUNNING]");
#endif
    		} else
    		if (eEvent == E_EVENT_START_UP) {
#ifdef USE_SERIAL
				vfPrintf(&sSerStream, LB "[START UP ON E_STATE_RUNNING]");
#endif
        	}  else {
        		;
        	}
        	break;

        default:
            break;
    }
}



/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
