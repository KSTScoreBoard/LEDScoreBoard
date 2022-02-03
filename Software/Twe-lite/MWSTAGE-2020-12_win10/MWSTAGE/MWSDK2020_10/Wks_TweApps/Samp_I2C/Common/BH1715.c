/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"

#include "sensor_driver.h"
#include "BH1715.h"
#include "SMBus.h"

#include "ccitt8.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sDebugStream;
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define BH1715_ADDRESS     (0x23)

#define BH1715_TRIG        (0x23)

#define BH1715_SOFT_RST    (0x07)

#define BH1715_CONVTIME    (24+2) // 24ms MAX

#define BH1715_DATA_NOTYET  (-32768)
#define BH1715_DATA_ERROR   (-32767)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: bBH1715reset
 *
 * DESCRIPTION:
 *   to reset BH1715 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bBH1715reset()
{
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite(BH1715_ADDRESS, BH1715_SOFT_RST, 0, NULL);
	// then will need to wait at least 15ms

	return bOk;
}

/****************************************************************************
 *
 * NAME: vHTSstartReadTemp
 *
 * DESCRIPTION:
 * Wrapper to start a read of the temperature sensor.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC bool_t bBH1715startRead()
{
	bool_t bOk = TRUE;

	// start conversion (will take some ms according to bits accuracy)
	bOk &= bSMBusWrite(BH1715_ADDRESS, BH1715_TRIG, 0, NULL);

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16BH1715readResult
 *
 * DESCRIPTION:
 * Wrapper to read a measurement, followed by a conversion function to work
 * out the value in degrees Celcius.
 *
 * RETURNS:
 * int16: 0~10000 [1 := 5Lux], 100 means 500 Lux.
 *        0x8000, error
 *
 * NOTES:
 * the data conversion fomula is :
 *      ReadValue / 1.2 [LUX]
 *
 ****************************************************************************/
PUBLIC int16 i16BH1715readResult()
{
	bool_t bOk = TRUE;
    int32 i32result;
    uint8 au8data[4];

    bOk &= bSMBusSequentialRead(BH1715_ADDRESS, 2, au8data);
    if (bOk == FALSE) {
    	i32result = SENSOR_TAG_DATA_ERROR;
    }

    i32result = ((au8data[0] << 8) | au8data[1]);
    i32result = (i32result * 10 + 30) / 60; // i32result/1.2/5

#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rBH1715 DATA %x", *((uint16*)au8data) );
#endif

    return (int16)i32result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
