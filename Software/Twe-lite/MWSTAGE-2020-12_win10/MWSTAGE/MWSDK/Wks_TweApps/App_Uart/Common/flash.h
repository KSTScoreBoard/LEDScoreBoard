/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/** @file
 *
 * @defgroup FLASH FLASHメモリの読み書き関数群
 * FLASH への読み書き関数
 */

#ifndef FLASH_H_
#define FLASH_H_

#define FLASH_TYPE E_FL_CHIP_INTERNAL
#define FLASH_SECTOR_SIZE (32L* 1024L) // 32KB
#define FLASH_SECTOR_NUMBER 5 // 0..4

#include "appsave.h"

/** @ingroup FLASH
 * フラッシュデータ構造体
 * - u32Magic と u8CRC により、書き込みデータが有為かどうか判定する
 * - u8CRC は データ中の CRC8 チェックサム
 */
typedef struct _tsFlash {
	uint32 u32Magic;
	tsFlashApp sData;
	uint8 u8CRC;
} tsFlash;

bool_t bFlash_Read(tsFlash *psFlash, uint8 sector, uint32 offset);
bool_t bFlash_Write(tsFlash *psFlash, uint8 sector, uint32 offset);
bool_t bFlash_Erase(uint8 sector);

bool_t bFlash_DataRecalcHeader(tsFlash *psFlash);
bool_t bFlash_DataValidateHeader(tsFlash *psFlash);

#endif /* FLASH_H_ */
