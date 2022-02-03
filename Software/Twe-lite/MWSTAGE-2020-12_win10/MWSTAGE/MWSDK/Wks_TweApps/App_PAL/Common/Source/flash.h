/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef FLASH_H_
#define FLASH_H_

#define FLASH_TYPE E_FL_CHIP_INTERNAL
#define FLASH_SECTOR_SIZE (128L) // 128B
//#define FLASH_SECTOR_SIZE (32L* 1024L) // 32KB
#define FLASH_SECTOR_NUMBER 5 // 0..4

#include "appsave.h"

typedef struct _tsFlash {
	uint32 u32Magic;
	tsFlashApp sData;
	uint8 u8CRC;
} tsFlash;

bool_t bFlash_Read(tsFlash *psFlash, uint8 sector, uint32 offset);
bool_t bFlash_Write(tsFlash *psFlash, uint8 sector, uint32 offset);
bool_t bFlash_RandomRead(uint8* pu8Data, uint16 length, uint8 sector, uint32 offset);
bool_t bFlash_RandomWrite(uint8* pu8Data, uint16 length, uint8 sector, uint32 offset);
bool_t bFlash_Erase(uint8 sector);

#endif /* FLASH_H_ */
