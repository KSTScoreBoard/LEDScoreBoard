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
#include "ADXL345.h"
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
#define ADXL345_ADDRESS     (0x1D)

#define ADXL345_X  (0x32)
#define ADXL345_Y  (0x34)
#define ADXL345_Z  (0x36)

#define ADXL345_WHO  (0x00)

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
 * NAME: bADXL345reset
 *
 * DESCRIPTION:
 *   to reset ADXL345 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345reset()
{
	bool_t	bOk = TRUE;
	uint8	u8data;
//	uint8 command = ADXL345_SOFT_COM;

	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_WHO, 0, NULL );
    bOk &= bSMBusSequentialRead(ADXL345_ADDRESS, 1, &u8data);
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
PUBLIC bool_t bADXL345startRead()
{
	bool_t bOk = TRUE;
	uint8 com = 0x08;	//	加速度を常に計測する

	// start conversion (will take some ms according to bits accuracy)
	bOk &= bSMBusWrite(ADXL345_ADDRESS, 0x2D, 1, &com );
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_X, 0, NULL);

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADXL345readResult
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
PUBLIC int16 i16ADXL345readResult()
{
	bool_t bOk = TRUE;
    int16 i16result=0;
    uint8 au8data[6];

    bOk &= bSMBusSequentialRead(ADXL345_ADDRESS, 2, au8data);
    if (bOk == FALSE) {
    	i16result = SENSOR_TAG_DATA_ERROR;
    }

    i16result = ((au8data[1] << 8) | au8data[0]);

    vfPrintf(&sSerStream, "\n\rADXL345 DATA %04x", i16result );
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rADXL345 DATA %x", *((uint16*)au8data) );
#endif

    return i16result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
