/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "ToCoNet.h"
#include "config.h"
#include "utils.h"
#include "common.h"

/** @ingroup MASTER
 * MODE設定ビットからデフォルト割り当てされる論理ＩＤテーブル
 */
const uint8 au8IoModeTbl_To_LogicalID[8] = {
	120, // CHILD
	0,   // PARENT
	254, // ROUTER
	123, // 32fps mode (7B)
	124, // 1sec sleep (7C)
	125, // RESPMODE (7D)
	255, // nodef
	127  // 10sec sleep (7F)
};

/** @ingroup MBUSA
 * MODBUS ASCII シリアル出力用のバッファ
 */
extern TWESERCMD_tsSerCmd_Context sSerCmdOut;

/**
 * 暗号化カギ
 */
const uint8 au8EncKey[] = {
		0x00, 0x01, 0x02, 0x03,
		0xF0, 0xE1, 0xD2, 0xC3,
		0x10, 0x20, 0x30, 0x40,
		0x1F, 0x2E, 0x3D, 0x4C
};

/** @ingroup MBUSA
 * 自身のシリアル番号を出力する（起動時メッセージにも利用）
 * @param pSer 出力先ストリーム
 */
void vModbOut_MySerial(TWE_tsFILE *pSer) {
	uint8 *q = sSerCmdOut.au8data;

	S_OCTET(SERCMD_ADDR_FR_MODULE);
	S_OCTET(SERCMD_ID_INFORM_MODULE_ADDRESS);
	S_OCTET(VERSION_MAIN);
	S_OCTET(VERSION_SUB);
	S_OCTET(VERSION_VAR);

	S_BE_DWORD(ToCoNet_u32GetSerial());

	sSerCmdOut.u16len = q - sSerCmdOut.au8data;
	sSerCmdOut.vOutput( &sSerCmdOut, pSer );

	sSerCmdOut.u16len = 0;
	memset(sSerCmdOut.au8data, 0x00, sizeof(sSerCmdOut.au8data));
}

/**
 * 文字列を区切り文字で分割する。pstr は NUL 文字で分割される。
 */
uint8 u8StrSplitTokens(uint8 *pstr, uint8 **auptr, uint8 u8max_entry) {
	uint8 u8num = 0;

	uint8 *p = pstr;
	if (pstr == NULL || *p == 0) {
		return 0;
	} else {

		auptr[0] = pstr;
		u8num = 1;

		while (*p) {
			if (*p == ',') {
				*p = 0;
				auptr[u8num] = p + 1;
				u8num++;
				if (u8num >= u8max_entry) {
					break;
				}
			}

			p++;
		}
	}

	return u8num;
}

/**
 * 暗号化カギの登録
 * @param u32seed
 * @return
 */
 bool_t bRegAesKey(uint32 u32seed) {
	uint8 au8key[16], *p = (uint8 *)&u32seed;
	int i;

	for (i = 0; i < 16; i++) {
		// 元のキーと u32seed の XOR をとる
		au8key[i] = au8EncKey[i] ^ p[i & 0x3];
	}

	return ToCoNet_bRegisterAesKey((void*)au8key, NULL);
}