/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "utils.h"

#include "SPI.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

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

/*
 * SPIの初期化処理
 */
PUBLIC void vSPIInit( uint8 u8mode, uint8 u8SlaveEnable, uint8 u8ClockDivider )
{
    /* configure SPI interface */
	switch(u8mode){
		case SPI_MODE0:		/*	SPI Mode0	*/
			vAHI_SpiConfigure( u8SlaveEnable, E_AHI_SPIM_MSB_FIRST, FALSE, FALSE, u8ClockDivider, E_AHI_SPIM_INT_DISABLE, E_AHI_SPIM_AUTOSLAVE_DSABL);
			break;
		case SPI_MODE1:		/*	SPI Mode1	*/
			vAHI_SpiConfigure( u8SlaveEnable, E_AHI_SPIM_MSB_FIRST, FALSE, TRUE, u8ClockDivider, E_AHI_SPIM_INT_DISABLE, E_AHI_SPIM_AUTOSLAVE_DSABL);
			break;
		case SPI_MODE2:		/*	SPI Mode2	*/
			vAHI_SpiConfigure( u8SlaveEnable, E_AHI_SPIM_MSB_FIRST, TRUE, FALSE, u8ClockDivider, E_AHI_SPIM_INT_DISABLE, E_AHI_SPIM_AUTOSLAVE_DSABL);
			break;
		case SPI_MODE3:		/*	SPI Mode3	*/
			vAHI_SpiConfigure( u8SlaveEnable, E_AHI_SPIM_MSB_FIRST, TRUE, TRUE, u8ClockDivider, E_AHI_SPIM_INT_DISABLE, E_AHI_SPIM_AUTOSLAVE_DSABL);
			break;
	}
}

//	書き込み
PUBLIC void vSPIWrite( uint8 u8Com )
{
	//	以下SPI通信を行う
	//	コマンド送信
	vAHI_SpiStartTransfer( 7, u8Com );
	//	終了待ち
	vAHI_SpiWaitBusy();
}

// 8bit 書き込み
PUBLIC void vSPIWrite8( uint8 u8CS, uint8 u8Reg, uint8 u8Com )
{
	vSPIChipSelect(u8CS);
	vSPIWrite(u8Reg);
	vSPIWrite(u8Com);
	vSPIStop();
}

//	読み込み
PUBLIC uint8 u8SPIRead( void )
{
	//	以下SPI通信を行う
	return u8AHI_SpiReadTransfer8();
}

// 8bit 読み込み
PUBLIC uint8 u8SPIRead8( uint8 u8CS, uint8 u8Reg )
{
	vSPIChipSelect(u8CS);
	vSPIWrite(u8Reg);
	vSPIWrite(0x00);
	uint8 u8val = u8SPIRead();
	vSPIStop();

	return u8val;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
