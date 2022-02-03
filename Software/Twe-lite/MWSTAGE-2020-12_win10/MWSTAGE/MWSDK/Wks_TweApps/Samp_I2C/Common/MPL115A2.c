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

#include "ToCoNet.h"
#include "sensor_driver.h"
#include "MPL115A2.h"
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
#define MPL115_ADDRESS     (0x60)

#define PORT_OUT1 18

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
bool_t bMPL115startConvert(void);

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
 * NAME: bMPL115reset
 *
 * DESCRIPTION:
 *   to reset MPL115 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bMPL115reset()
{
	bool_t	bOk = TRUE;
	uint8	u8wait;

	vPortAsOutput(PORT_OUT1);
	vPortSetHi(PORT_OUT1);

	// then will need to wait at least 15ms
	u8wait = u32TickCount_ms;
    while(u32TickCount_ms-u8wait < 15){
    }

	vPortSetLo(PORT_OUT1);
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
PUBLIC bool_t bMPL115startRead()
{
	bool_t bOk = TRUE;

	// start conversion (will take some ms according to bits accuracy)
	//	変換命令
	bOk &= bMPL115startConvert();

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16MPL115readResult
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
PUBLIC int16 i16MPL115readResult()
{
	bool_t bOk = TRUE;		//	I2C通信が成功したかどうかのフラグ
	int32 i32result;		//	返戻値
	uint8 au8data[8];		//	補正値のバイナリデータを受け取るTemporary
	uint8 au8temp[4];		//	気圧・温度のバイナリデータを受け取るTemporary
	uint16 u16a0, u16b1, u16b2, u16c12;		//	補正値のビット列
	(void)u16a0;(void)u16b1;(void)u16b2;(void)u16c12;
	uint16 u16temp, u16pascal;				//	気圧・温度のビット列
	float a0, b1, b2, c12;					//	補正値

	/*	気圧・気温の読み込み命令	*/
	bOk &= bSMBusWrite(MPL115_ADDRESS, 0x00, 0, NULL);
	if (bOk == FALSE) {
		i32result = SENSOR_TAG_DATA_ERROR;
	}else{
		bOk &= bSMBusSequentialRead(MPL115_ADDRESS, 4, au8temp);
		if (bOk == FALSE) {
			i32result = SENSOR_TAG_DATA_ERROR;
		}else{
			//	10bitのデータに変換
			u16pascal = ((au8temp[0]<<8)|au8temp[1]) >> 6;
			u16temp = ((au8temp[2]<<8)|au8temp[3]) >> 6;

			/*	補正値の読み込み命令	*/
			bOk &= bSMBusWrite(MPL115_ADDRESS, 0x04, 0, NULL);
			bOk &= bSMBusSequentialRead(MPL115_ADDRESS, 8, au8data);
			if (bOk == FALSE) {
				i32result = SENSOR_TAG_DATA_ERROR;
			}else{
				/*	補正値の計算 詳しくはデータシート参照 もっときれいにしたい	*/
				u16a0 = (au8data[0]<<8)|au8data[1];
				a0 = (au8data[0] << 5) + (au8data[1] >> 3) + (au8data[1] & 0x07) / 8.0;
//				a0 = (u16a0&0x8000 ? -1 : 1)*( ((u16a0&0x7800)>>11)*16*16 + ((u16a0&0x0780)>>7)*16 + ((u16a0&0x0078)>>3) + (float)((u16a0&0x0007)<<1)/16.0 );
				u16b1 = (au8data[2]<<8)|au8data[3];
				b1 = ( ( ( (au8data[2] & 0x1F) * 0x100 ) + au8data[3] ) / 8192.0 ) - 3 ;
//				b1 = (u16b1&0x8000 ? -1 : 1)*( ((u16b1&0x6000)>>13) + ((u16b1&0x1E00)>>9)/16.0 + ((u16b1&0x01E0)>>5)/(16.0*16.0) + ((u16b1&0x001E)>>1)/(16.0*16.0*16.0) + ((u16b1&0x0001)<<3)/(16.0*16.0*16.0*16.0));
				u16b2 = (au8data[4]<<8)|au8data[5];
				b2 = ( ( ( ( au8data[4] - 0x80) << 8 ) + au8data[5] ) / 16384.0 ) - 2 ;
//				b2 = (u16b2&0x8000 ? -1 : 1)*( ((u16b2&0x4000)>>14) + ((u16b2&0x2A00)>>10)/16.0 + ((u16b2&0x02A0)>>6)/(16.0*16.0) + ((u16b2&0x002A)>>2)/(16.0*16.0*16.0) + ((u16b2&0x0002)<<2)/(16.0*16.0*16.0*16.0));
				u16c12 = (au8data[6]<<8)|au8data[7];
				c12 = ( ( ( au8data[6] * 0x100 ) + au8data[7] ) / 16777216.0 )  ;
//				c12 = (u16c12&0x8000 ? -1 : 1)*( ((u16c12&0x7000)>>12)/(16.0*16.0*16.0) + ((u16b2&0x0F00)>>8)/(16.0*16.0*16.0*16.0) + ((u16b2&0x00F0)>>4)/(16.0*16.0*16.0*16.0*16.0) + (u16b2&0x000A)/(16.0*16.0*16.0*16.0*16.0*16.0));

				/*	気圧の計算	*/
				i32result = (int32)(((a0+(b1+c12*u16temp)*u16pascal+b2*u16temp)*((115.0-50.0)/1023.0)+50)*10.0);
			}
		}
	}

#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rMPL115 DATA %x", *((uint16*)au8data) );
#endif

    return (int16)i32result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PUBLIC bool_t bMPL115startConvert()
{
	bool_t bOk = TRUE;
    uint8 tmp=0;
    uint8 u8wait=0;


	bOk &= bSMBusWrite(MPL115_ADDRESS, 0x12, 1, &tmp);
	u8wait = u32TickCount_ms;
    while(u32TickCount_ms-u8wait < 3){
    }

	return bOk;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
