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
#include "ADT7410.h"
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
#define ADT7410_ADDRESS     (0x48)

#define ADT7410_TRIG        (0x23)

#define ADT7410_SOFT_RST    (0x2F)

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
 * NAME: bADT7410reset
 *
 * DESCRIPTION:
 *   to reset ADT7410 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bADT7410reset( bool_t bMode16 )
{
	bool_t bOk = TRUE;
	uint8 u8conf = 0x80;		//	16bit mode

	bOk &= bSMBusWrite(ADT7410_ADDRESS, ADT7410_SOFT_RST, 0, NULL);

	//	16bitモードの場合 設定変更
	if( bMode16 == TRUE ){
		bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x03, 1, &u8conf );
	}
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
PUBLIC bool_t bADT7410startRead()
{
	bool_t bOk = TRUE;

	// start conversion (will take some ms according to bits accuracy)
	//	レジスタ0x00を読み込む宣言
	bOk &= bSMBusWrite(ADT7410_ADDRESS, 0x00, 0, NULL);

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADT7410readResult
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
PUBLIC int16 i16ADT7410readResult( bool_t bMode16 )
{
	bool_t bOk = TRUE;
	uint16 u16result;
    int32 i32result;
    float temp;
    uint8 au8data[2];

    bOk &= bSMBusSequentialRead(ADT7410_ADDRESS, 2, au8data);
    if (bOk == FALSE) {
    	i32result = SENSOR_TAG_DATA_ERROR;
    }

	u16result = ((au8data[0] << 8) | au8data[1]);	//	読み込んだ数値を代入
    if( bMode16 == FALSE ){		//	13bitモード
    	i32result = (int32)u16result >> 3;
    	//	符号判定
    	if(u16result & 0x8000 ){
    		i32result -= 8192;
    	}
    	temp = (float)i32result/16.0;
    }else{		//	16bitモード
    	i32result = (int32)u16result;
    	//	符号判定
    	if(u16result & 0x8000){
    		i32result -= 65536;
    	}
    	temp = (float)i32result/128.0;
    }


#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rADT7410 DATA %x", *((uint16*)au8data) );
#endif

    return (int16)(temp*100);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
