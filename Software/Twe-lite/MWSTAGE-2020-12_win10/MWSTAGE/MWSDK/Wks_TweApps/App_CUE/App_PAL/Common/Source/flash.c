/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>
#include <string.h>

#include <AppHardwareApi.h>
#include <AppApi.h>

#define USE_EEPROM //!< JN516x でフラッシュを使用する

#include "flash.h"
#include "ccitt8.h"

#include "config.h"

#ifdef USE_EEPROM
# include "eeprom_6x.h"
#endif

#define FLASH_MAGIC_NUMBER (0xA501EF5A ^ APP_ID) //!< フラッシュ書き込み時のマジック番号  @ingroup FLASH

/** @ingroup FLASH
 * フラッシュの読み込み
 * @param psFlash 読み込み格納データ
 * @param sector 読み出しセクタ
 * @param offset セクタ先頭からのオフセット
 * @return TRUE:読み出し成功 FALSE:失敗
 */
bool_t bFlash_Read(tsFlash *psFlash, uint8 sector, uint32 offset) {
    bool_t bRet = FALSE;
    offset += (uint32)sector * FLASH_SECTOR_SIZE; // calculate the absolute address


#ifdef USE_EEPROM
    if (EEP_6x_bRead(offset, sizeof(tsFlash), (uint8 *)psFlash)) {
    	bRet = TRUE;
    }
#else
    if (bAHI_FlashInit(FLASH_TYPE, NULL) == TRUE) {
        if (bAHI_FullFlashRead(offset, sizeof(tsFlash), (uint8 *)psFlash)) {
            bRet = TRUE;
        }
    }
#endif

    // validate content
    if (bRet && psFlash->u32Magic != FLASH_MAGIC_NUMBER) {
    	bRet = FALSE;
    }
    if (bRet && psFlash->u8CRC != u8CCITT8((uint8*)&(psFlash->sData), sizeof(tsFlashApp))) {
    	bRet = FALSE;
    }

    return bRet;
}

/**  @ingroup FLASH
 * フラッシュ書き込み
 * @param psFlash 書き込みたいデータ
 * @param sector 書き込みセクタ
 * @param offset 書き込みセクタ先頭からのオフセット
 * @return TRUE:書き込み成功 FALSE:失敗
 */
bool_t bFlash_Write(tsFlash *psFlash, uint8 sector, uint32 offset)
{
    bool_t bRet = FALSE;
    offset += (uint32)sector * FLASH_SECTOR_SIZE; // calculate the absolute address

#ifdef USE_EEPROM
	psFlash->u32Magic = FLASH_MAGIC_NUMBER;
	psFlash->u8CRC = u8CCITT8((uint8*)&(psFlash->sData), sizeof(tsFlashApp));
	if (EEP_6x_bWrite(offset, sizeof(tsFlash), (uint8 *)psFlash)) {
		bRet = TRUE;
	}
#else
    if (bAHI_FlashInit(FLASH_TYPE, NULL) == TRUE) {
        if (bAHI_FlashEraseSector(sector) == TRUE) { // erase a corresponding sector.
        	psFlash->u32Magic = FLASH_MAGIC_NUMBER;
        	psFlash->u8CRC = u8CCITT8((uint8*)&(psFlash->sData), sizeof(tsFlashApp));
            if (bAHI_FullFlashProgram(offset, sizeof(tsFlash), (uint8 *)psFlash)) {
                bRet = TRUE;
            }
        }
    }
#endif

    return bRet;
}

/** @ingroup FLASH
 * フラッシュの読み込み
 * @param pu8Data 読み込み格納データ
 * @param length 読み込む長さ
 * @param sector 読み出しセクタ
 * @param offset セクタ先頭からのオフセット
 * @return TRUE:読み出し成功 FALSE:失敗
 */
bool_t bFlash_RandomRead(uint8* pu8Data, uint16 length, uint8 sector, uint32 offset)
{
    bool_t bRet = FALSE;
    offset += (uint32)sector * FLASH_SECTOR_SIZE; // calculate the absolute address

    if ( EEP_6x_bRead(offset, length, pu8Data )) {
    	bRet = TRUE;
    }

    return bRet;
}

/**  @ingroup FLASH
 * フラッシュ書き込み
 * @param pu8Data 書き込みたいデータ
 * @param length 書き込むバイト数
 * @param sector 書き込みセクタ
 * @param offset 書き込みセクタ先頭からのオフセット
 * @return TRUE:書き込み成功 FALSE:失敗
 */
bool_t bFlash_RandomWrite(uint8* pu8Data, uint16 length, uint8 sector, uint32 offset)
{
    bool_t bRet = FALSE;
    offset += (uint32)sector * FLASH_SECTOR_SIZE; // calculate the absolute address

	if ( EEP_6x_bWrite(offset, length, pu8Data) ) {
		bRet = TRUE;
	}

    return bRet;
}

/**  @ingroup FLASH
 * フラッシュセクター消去
 * @return TRUE:書き込み成功 FALSE:失敗
 */
bool_t bFlash_Erase(uint8 sector)
{
    bool_t bRet = FALSE;

#ifdef USE_EEPROM
	int i;
	// EEPROM の全領域をクリアする。セグメント単位で消去する
	uint8 au8buff[FLASH_SECTOR_SIZE];
	memset (au8buff, 0xFF, FLASH_SECTOR_SIZE);

	bRet = TRUE;

	if(sector == 0xFF){
		for (i = 0; i < EEPROM_6X_USER_SEGMENTS; i++) {
			bRet &= EEP_6x_bWrite(i * EEPROM_6X_SEGMENT_SIZE, EEPROM_6X_SEGMENT_SIZE, au8buff);
		}
	}else{
		bRet &= EEP_6x_bWrite(sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE, au8buff);
	}
#else
    if (bAHI_FlashInit(FLASH_TYPE, NULL) == TRUE) {
        if (bAHI_FlashEraseSector(sector) == TRUE) { // erase a corresponding sector.
        	bRet = TRUE;
        }
    }
#endif

    return bRet;
}
