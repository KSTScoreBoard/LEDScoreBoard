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
//	センサを有効にするためのアドレスとオプション
#define LIS3DH_OPT			(0x77)
#define LIS3DH_SET_OPT		(0x20)

//	有効にすると高分解能モードになる(未確認)
//#define HR_MODE
#define LIS3DH_HR			(0x08)
#define LIS3DH_SET_HR		(0x23)

//	各軸の加速度が入ったアドレス
#define LIS3DH_X			(0x28)
#define LIS3DH_Y			(0x2A)
#define LIS3DH_Z			(0x2C)

//	確認用のレジスタのアドレス
#define LIS3DH_WHO			(0x0F)
#define LIS3DH_NAME			(0x33)

//	Read命令
#define READ				(0x80)
//	Write命令
#define WRITE				(0x00)

//	有効にするポート数の指定
#define SLAVE_ENABLE1		(0)			//	DIO19を用いる
#define SLAVE_ENABLE2		(1)			//	DIO19とDIO0を用いる
#define SLAVE_ENABLE3		(2)			//	DIO19,0,1を用いる

//	ChipSelect
#define CS_DIO19			(0x01)		//	DIO19に接続したものを使う
#define CS_DIO0				(0x02)		//	DIO0に接続したものを使う
#define CS_DIO1				(0x04)		//	DIO1に接続したものを使う

const uint8 LIS3DH_AXIS[] = {
		LIS3DH_X,
		LIS3DH_Y,
		LIS3DH_Z
};

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data );
PRIVATE bool_t bWhoAmI();

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
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
//	リセットというよりは初期化処理
PUBLIC bool_t bLIS3DHreset()
{
	bool_t bOk = TRUE;
	uint16 u16com;

    /* configure SPI interface */
	/*	SPI Mode3	*/
	vAHI_SpiConfigure(SLAVE_ENABLE2,
					  E_AHI_SPIM_MSB_FIRST,
					  TRUE,
					  TRUE,
					  1,
					  E_AHI_SPIM_INT_DISABLE,
					  E_AHI_SPIM_AUTOSLAVE_DSABL);

	//	Read Test
	bOk = bWhoAmI();

	//	センサの設定 基本的にこの流れでSPI通信を行う
	//	ChipSelect
	vAHI_SpiSelect(CS_DIO0);
	//	コマンドの設定
	u16com = ((LIS3DH_SET_OPT+WRITE)<<8)+LIS3DH_OPT;
	//	コマンド送信
	vAHI_SpiStartTransfer( 15, u16com );
	//	送信終了を待つ
	while(bAHI_SpiPollBusy());
	//	送受信終了
	vAHI_SpiStop();

	return bOk;
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
PUBLIC bool_t bLIS3DHstartRead()
{
	//	通信できたかどうかを知るすべがないのでそのままTRUEを返す
	bool_t bOk = TRUE;

#ifdef HR_MODE
	// HR Mode
	vAHI_SpiSelect(CS_DIO0);
	u8com = LIS3DH_SET_HR+WRITE;
	vAHI_SpiStartTransfer( 7, u8com );
	vAHI_SpiStartTransfer( 7, LIS3DH_HR );
	while(bAHI_SpiPollBusy());
	vAHI_SpiStop();
#endif

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16LIS3DHreadResult
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 * NOTES:
 *
 ****************************************************************************/
//	各軸の加速度を読みに行く
PUBLIC int16 i16LIS3DHreadResult( uint8 u8axis )
{
	bool_t	bOk = TRUE;
	int32	i32result=SENSOR_TAG_DATA_ERROR;
	uint8	au8data[2];

	//	各軸の読み込み
	bOk &= bGetAxis( u8axis, au8data );
#ifdef HR_MODE
	i32result = ((int16)((au8data[1] << 8) | au8data[0])/32);
#else
	i32result = ((int16)((au8data[1] << 8) | au8data[0])/128);
#endif

#ifdef SERIAL_DEBUG
	vfPrintf(&sDebugStream, "\n\rLIS3DH DATA %x", *((uint16*)au8data) );
#endif

    return (int16)i32result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
//	指定した軸の加速度を読みに行って返す
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data )
{
	uint8 i;
	uint16 u16com = 0x0000;
	bool_t bOk = TRUE;

	for( i=0; i<2; i++ ){
		//	以下SPI通信を行う
		//	Chip Select
		vAHI_SpiSelect(CS_DIO0);
		//	コマンドの設定
		u16com = ( LIS3DH_AXIS[u8axis]+i+READ ) << 8;
		//	コマンド送信
		vAHI_SpiStartTransfer( 15, u16com );
		//	終了待ち
		while(bAHI_SpiPollBusy());
		//	8bitデータの読み込み
		au8data[i] = u8AHI_SpiReadTransfer8();
		//	終わり
		vAHI_SpiStop();
	}

	return bOk;
}

//	テスト用レジスタを読みに行く
PRIVATE bool_t bWhoAmI()
{
	bool_t	bOk = TRUE;
	uint8 u8am_I = 0x00;
	uint16 u16com = 0x0000;

	vAHI_SpiSelect(CS_DIO0);
	u16com = (LIS3DH_WHO+READ)<<8;
	vAHI_SpiStartTransfer( 15, u16com );
	while(bAHI_SpiPollBusy());

	/* read slave status */
	u8am_I = u8AHI_SpiReadTransfer8();

	vAHI_SpiStop();

	if( u8am_I != 0x33 ){
		bOk = FALSE;
		vfPrintf( &sSerStream, LB"%02x", u8am_I );
	}


	return bOk;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
