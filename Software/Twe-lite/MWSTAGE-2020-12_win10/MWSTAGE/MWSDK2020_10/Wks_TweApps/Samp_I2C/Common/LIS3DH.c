/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "LIS3DH.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sDebugStream;
#endif
tsFILE sSerStream;

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define LIS3DH_ADDRESS     (0x18)

#define LIS3DH_TRIG        (0x77)

#define LIS3DH_SOFT_RST    (0x24)
#define LIS3DH_SOFT_COM    (0x80)

#define LIS3DH_X  (0x28)
#define LIS3DH_Y  (0x2A)
#define LIS3DH_Z  (0x2C)

#define LIS3DH_WHO  (0x5F)

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
 * NAME: bLIS3DHreset
 *
 * DESCRIPTION:
 *   to reset LIS3DH device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bLIS3DHreset()
{
	bool_t bOk = TRUE;
	uint8 command = LIS3DH_SOFT_COM;

	bOk &= bSMBusWrite(LIS3DH_ADDRESS, LIS3DH_SOFT_RST, 1, &command );
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
PUBLIC bool_t bLIS3DHstartRead()
{
	bool_t bOk = TRUE;
	uint8 com = LIS3DH_TRIG;
//	uint8 com2 = 0x44;

	// start conversion (will take some ms according to bits accuracy)
//	bOk &= bSMBusWrite(LIS3DH_ADDRESS, LIS3DH_WHO, 0, NULL);
	bOk &= bSMBusWrite(LIS3DH_ADDRESS, 0x20, 1, &com );
//	bOk &= bSMBusWrite(LIS3DH_ADDRESS, 0x27, 1, &com2 );
	bOk &= bSMBusWrite(LIS3DH_ADDRESS, LIS3DH_X, 0, NULL);

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16LIS3DHreadResult
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
PUBLIC int16 i16LIS3DHreadResult()
{
	bool_t bOk = TRUE;
    int32 i32result=0;
    int32 i32result2=0;
    int32 i32result3=0;
    uint8 au8data[6];

    bOk &= bSMBusSequentialRead(LIS3DH_ADDRESS, 2, au8data);
    if (bOk == FALSE) {
    	i32result = SENSOR_TAG_DATA_ERROR;
    }

    i32result = ((au8data[1] << 8) | au8data[0]);
//    i32result2 = ((au8data[3] << 8) | au8data[2]);
//    i32result3 = ((au8data[5] << 8) | au8data[4]);
//    i32result = au8data[0];

    vfPrintf(&sSerStream, "\n\rLIS3DH DATA %04x, %04x, %04x", i32result, i32result2, i32result3 );
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rLIS3DH DATA %x", *((uint16*)au8data) );
#endif

    return (int16)i32result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
