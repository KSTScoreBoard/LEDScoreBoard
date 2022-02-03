/****************************************************************************
 *
 * MODULE:             24xx01 EEPROM Driver functions
 *
 * COMPONENT:          $RCSfile: 24xx01.c,v $
 *
 * VERSION:            $Name: RD_RELEASE_6thMay09 $
 *
 * REVISION:           $Revision: 1.3 $
 *
 * DATED:              $Date: 2008/02/29 18:02:04 $
 *
 * STATUS:             $State: Exp $
 *
 * AUTHOR:             Lee Mitchell
 *
 * DESCRIPTION:
 * 24xx01 2 wire serial EEPROM driver
 *
 * CHANGE HISTORY:
 *
 * $Log: 24xx01.c,v $
 * Revision 1.3  2008/02/29 18:02:04  dclar
 * dos2unix
 *
 * Revision 1.2  2007/05/17 11:48:33  lmitch
 * Make variables used in for(i=0;i<n;i++) delay loops volatile to prevent
 * ba-elf compiler optimising them out.
 *
 * Revision 1.1  2006/12/08 10:51:16  lmitch
 * Added to repository
 *
 *
 *
 * LAST MODIFIED BY:   $Author: dclar $
 *                     $Modtime: $
 *
 ****************************************************************************
 *
 * This software is owned by Jennic and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on Jennic products. You, and any third parties must reproduce
 * the copyright and warranty notice and any other legend of ownership on
 * each copy or partial copy of the software.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS". JENNIC MAKES NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * ACCURACY OR LACK OF NEGLIGENCE. JENNIC SHALL NOT, IN ANY CIRCUMSTANCES,
 * BE LIABLE FOR ANY DAMAGES, INCLUDING, BUT NOT LIMITED TO, SPECIAL,
 * INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON WHATSOEVER.
 *
 * Copyright Jennic Ltd 2005, 2006. All rights reserved
 *
 ***************************************************************************/

/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "SMBus.h"
#include "24XX00.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define EEPROM_24XX01_ADDRESS		0x50
#define EEPROM_24XX01_PAGESIZE		8

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

PUBLIC bool_t b24xx01_Write(uint8 u8Address, uint8 *pu8Data, uint8 u8Length)
{

	uint8 n;
	bool_t bOk = TRUE;
	volatile int x;

	for(n = 0; n < u8Length; n++){

		bOk &= bSMBusWrite(EEPROM_24XX01_ADDRESS, u8Address++, 1, pu8Data++);

		for(x = 0; x < 16000; x++);

	}

	return(bOk);

}


PUBLIC bool_t b24xx01_Read(uint8 u8Address, uint8 *pu8Dest, uint8 u8Length)
{

	bool_t bOk = TRUE;

	bOk &= bSMBusWrite(EEPROM_24XX01_ADDRESS, u8Address, 0, NULL);

	if(u8Length > 1){

		bOk &= bSMBusSequentialRead(EEPROM_24XX01_ADDRESS, u8Length, pu8Dest);

	}

	return(bOk);

}

#if 0
PUBLIC bool_t b24xx1025_Write(
		uint8 u8select,		// 0000:0abc => a:block, bc: chip select
		uint16 u16Address,	// Address in Byte
		uint8 *pu8Data,		// Buffer to save into EEPROM
		uint8 u8Length)		// Length to save
{

	uint8 n;
	bool_t bOk = TRUE;
	volatile int x;

	uint8 u8dat[2];

	for(n = 0; n < u8Length; n++){
		u8dat[0] = u16Address & 0xff;
		u8dat[1] = *pu8Data;

		// Set Address and wrie a byte
		//   [Chip ID(Write)][Address High Byte][Address Low Byte][Byte]
		bOk &= bSMBusWrite(EEPROM_24XX01_ADDRESS | u8select, u16Address >> 8, 2, u8dat);

		// Increment address
		u16Address++;

		// Wait until completion of saving.
		for(x = 0; x < 16000; x++);
	}

	return(bOk);
}

PUBLIC bool_t b24xx1025_Read(
		uint8 u8select,		// 0000:0abc => a:block, bc: chip select
		uint16 u16Address,  // Address in byte
		uint8 *pu8Dest,     // Buffer to read from EEPROM
		uint8 u8Length)     // Length to read
{
	bool_t bOk = TRUE;

	uint8 u8dat[1];
	u8dat[0] = u16Address & 0xff; // address low byte

	// Set Address
	//   [Chip ID(Write)][Address High Byte][Address Low Byte]
	bOk &= bSMBusWrite(EEPROM_24XX01_ADDRESS | u8select, u16Address >> 8, 1, u8dat);

	if(u8Length > 1){
		// Read sequentially
		//   [Chip ID(Read)][Byte1][Byte2]...[Byte u8Length]
		bOk &= bSMBusSequentialRead(EEPROM_24XX01_ADDRESS | u8select, u8Length, pu8Dest);
	}
	return(bOk);
}
#endif

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
