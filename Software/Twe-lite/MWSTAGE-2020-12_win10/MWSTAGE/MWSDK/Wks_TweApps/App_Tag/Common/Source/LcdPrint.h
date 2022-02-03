/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * LcdPrint.h
 *
 *  Created on: 2012/12/28
 *      Author: seigo13
 */

#ifndef LCDPRINT_H_
#define LCDPRINT_H_

#include <jendefs.h>

PUBLIC bool_t LCD_bTxChar(uint8 u8SerialPort, uint8 u8Data);
PUBLIC bool_t LCD_bTxBottom(uint8 u8SerialPort, uint8 u8Data);
PUBLIC void vDrawLcdDisplay(uint32 u32Xoffset, uint8 bClearShadow);
PUBLIC void vDrawLcdInit();
extern const uint8 au8TocosHan[];
extern const uint8 au8TocosZen[];
extern const uint8 au8Tocos[];

#define CHR_BLK 0x7f
#define CHR_BLK_50 0x80

#endif /* LCDPRINT_H_ */
