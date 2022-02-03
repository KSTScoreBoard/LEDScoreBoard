/****************************************************************************
 *
 * MODULE:             24xx01 Driver functions header file
 *
 * COMPONENT:          $RCSfile: 24xx01.h,v $
 *
 * VERSION:            $Name: RD_RELEASE_6thMay09 $
 *
 * REVISION:           $Revision: 1.2 $
 *
 * DATED:              $Date: 2008/02/29 18:00:43 $
 *
 * STATUS:             $State: Exp $
 *
 * AUTHOR:             Lee Mitchell
 *
 * DESCRIPTION:
 * 24xx01 2 wire serial EEPROM driver (header file)
 *
 * CHANGE HISTORY:
 *
 * $Log: 24xx01.h,v $
 * Revision 1.2  2008/02/29 18:00:43  dclar
 * dos2unix
 *
 * Revision 1.1  2006/12/08 10:50:45  lmitch
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

#ifndef  EEPROM_24XX01_H_INCLUDED
#define  EEPROM_24XX01_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

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

PUBLIC bool_t b24xx01_Write(uint8 u8Address, uint8 *pu8Data, uint8 u8Length);
PUBLIC bool_t b24xx01_Read(uint8 u8Address, uint8 *pu8Dest, uint8 u8Length);

PUBLIC bool_t b24xx1025_Write(
		uint8 u8select,		// 0000:0abc => a:block, bc: chip select
		uint16 u16Address,	// Address in Byte
		uint8 *pu8Data,		// Buffer to save into EEPROM
		uint8 u8Length);	// Length to save

PUBLIC bool_t b24xx1025_Read(
		uint8 u8select,		// 0000:0abc => a:block, bc: chip select
		uint16 u16Address,  // Address in byte
		uint8 *pu8Dest,     // Buffer to read from EEPROM
		uint8 u8Length);    // Length to read

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* EEPROM_24XX01_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

