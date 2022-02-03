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
#include "MAX31855.h"

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
//	有効にするポート数の指定
#define SLAVE_ENABLE1		(0)			//	DIO19を用いる
#define SLAVE_ENABLE2		(1)			//	DIO19とDIO0を用いる
#define SLAVE_ENABLE3		(2)			//	DIO19,0,1を用いる

//	ChipSelect
#define CS_DIO19			(0x01)		//	DIO19に接続したものを使う
#define CS_DIO0				(0x02)		//	DIO0に接続したものを使う
#define CS_DIO1				(0x04)		//	DIO1に接続したものを使う

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/

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
 * NAME: bMAX31855reset
 *
 * DESCRIPTION:
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
//	リセットというよりは初期化処理
PUBLIC bool_t bMAX31855reset()
{
	//bool_t bOk = TRUE;
	//uint16 u16com;

    /* configure SPI interface */
	/*	SPI Mode1	*/
	vAHI_SpiConfigure(SLAVE_ENABLE2,
					  E_AHI_SPIM_MSB_FIRST,
					  FALSE,
					  TRUE,
					  2,
					  E_AHI_SPIM_INT_DISABLE,
					  E_AHI_SPIM_AUTOSLAVE_DSABL);

	return TRUE;
}

/****************************************************************************
 *
 * NAME: vHTSstartReadTemp
 *
 * DESCRIPTION:
 *
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
//	Samp_I2Cの名残
PUBLIC bool_t bMAX31855startRead()
{
	//	通信できたかどうかを知るすべがないのでそのままTRUEを返す
	//bool_t bOk = TRUE;

	return TRUE;
}

/****************************************************************************
 *
 * NAME: u16MAX31855readResult
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 * NOTES:
 *
 ****************************************************************************/
//	各軸の加速度を読みに行く
PUBLIC int32 i32MAX31855readResult()
{

	int32	i32result=SENSOR_TAG_DATA_ERROR;
	union{
		uint32 u32;
		uint8 au8[4];
	} udata;
	udata.u32 = 0xFFFFFFFF;

	vAHI_SpiSelect(CS_DIO19);
	vAHI_SpiStartTransfer( 31, udata.u32 );
	while(bAHI_SpiPollBusy());
	udata.u32 = u32AHI_SpiReadTransfer32();
	vAHI_SpiStop();

	bool_t bSigned = udata.au8[0]&0x80 ? TRUE:FALSE;
	int8 i8Dec = ((udata.au8[1]&0x0C)>>2)*25;

	i32result = ((udata.au8[0]&0xFF)<<8)+(udata.au8[1]&0xF0);
	if(bSigned){
		uint16 temp = i32result&0xFFFF;
		temp = ~temp + 1;   // 正数に戻す
		temp = temp>>4;
		i32result = temp*-100;
	}else{
		i32result = (i32result>>4)*100;
	}
	i32result += i8Dec;

    return i32result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
