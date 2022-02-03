/* Copyright (C) 2019 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "SMBus.h"
#include "PCA9632.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define PCA9632_ADDRESS		0x62

#define PCA9632_MODE1		0x00
#define PCA9632_MODE2		0x01
#define PCA9632_PWM0		0x02
#define PCA9632_PWM1		0x03
#define PCA9632_PWM2		0x04
#define PCA9632_PWM3		0x05
#define PCA9632_GRPPWM		0x06
#define PCA9632_GRPFREQ		0x07
#define PCA9632_LEDOUT		0x08
#define PCA9632_SUBADDR1	0x09
#define PCA9632_SUBADDR2	0x0A
#define PCA9632_SUBADDR3	0x0B
#define PCA9632_ALLCALLADR	0x0C

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
/**
 * PCA9632 を初期化する
 **/
PUBLIC bool_t bPCA9632_Reset()
{
	bool_t bOk = TRUE;

	uint8 data = 0x91;
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_MODE1, 1, &data );
	data = 0x01;
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_MODE1, 1, &data );
	data = 0x00;
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_PWM0, 1, &data );
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_PWM1, 1, &data );
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_PWM2, 1, &data );
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_PWM3, 1, &data );
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_GRPFREQ, 1, &data );
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_LEDOUT, 1, &data );
	data = 0xFF;
	bOk &= bSMBusWrite( PCA9632_ADDRESS, PCA9632_GRPPWM, 1, &data );

	return bOk;
}
/**
 * 論理反転などの初期設定をする
 **/
PUBLIC bool_t bPCA9632_Init()
{
	uint8 data = 0x35;		// Hi/Loの反転、Blink有効、トーテムポール出力有効
	//uint8 data = 0x25;		// Blink有効
	return bSMBusWrite(PCA9632_ADDRESS, PCA9632_MODE2, 1, &data);	
}

/**
 * スリープするか否か
 **/
PUBLIC bool_t bPCA9632_Power( bool_t bPowerOn )
{
	uint8 data = 0x91;
	if(bPowerOn){
		data -= 0x10;
	}
	return bSMBusWrite(PCA9632_ADDRESS, PCA9632_MODE1, 1, &data);
}

/**
 * 点滅させるときのDuty
 **/
PUBLIC bool_t bPCA9632_BlinkDuty(uint8 u8duty)
{
	return bSMBusWrite(PCA9632_ADDRESS, PCA9632_GRPPWM, 1, &u8duty);
}

/**
 * 点滅させる周期
 * (u8cycle+1)/24 [sec] の周期になる
 **/
PUBLIC bool_t bPCA9632_BlinkCycle(uint8 u8cycle)
{
	return bSMBusWrite(PCA9632_ADDRESS, PCA9632_GRPFREQ, 1, &u8cycle);
}

/**
 * LEDの明るさを変える
 **/
PUBLIC bool_t bPCA9632_Duty( uint8 port, uint8 duty )
{
	return bSMBusWrite(PCA9632_ADDRESS, PCA9632_PWM0+port, 1, &duty);
}

/**
 * 点灯、点滅、消灯の制御をする
 *  引数 u8status は LSBから2bitずつ LED1、LED2... と並んでおり、
 *  0で消灯、1で点灯、2で点滅, 3で変化させない
 **/
PUBLIC bool_t bPCA9632_LEDStatus( uint8 u8status )
{
	uint8 readdata = 0;
	uint8 writedata = 0;

	bool_t bOk = bSMBusWrite(PCA9632_ADDRESS, PCA9632_LEDOUT, 0, NULL);
	bOk |= bSMBusSequentialRead(PCA9632_ADDRESS, 1, &readdata);

	writedata = ((u8status&0x03) == 0x03) ? (readdata&0x03) : ((u8status&0x03) ? (u8status&0x03)+0x01:0x00 );
	writedata |= ((u8status&0x0C) == 0x0C) ? (readdata&0x0C) : ((u8status&0x0C) ? (u8status&0x0C)+0x04:0x00 );
	writedata |= ((u8status&0x30) == 0x30) ? (readdata&0x30) : ((u8status&0x30) ? (u8status&0x30)+0x10:0x00 );
	writedata |= ((u8status&0xC0) == 0xC0) ? (readdata&0xC0) : ((u8status&0xC0) ? (u8status&0xC0)+0x40:0x00 );

	bOk = bSMBusWrite(PCA9632_ADDRESS, PCA9632_LEDOUT, 1, &writedata);

	// 0だったらスリープ状態にする
	bPCA9632_Power(u8status?TRUE:FALSE);

	return bOk;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
