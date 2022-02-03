/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <string.h>
#include "AddrKeyAry.h"

/** @ingroup ADDRKEYA
 * 重複チェックの初期化
 * @param p 管理構造体
 */
void ADDRKEYA_vInit(tsAdrKeyA_Context *p) {
	memset(p, 0, sizeof(tsAdrKeyA_Context));
}

/** @ingroup ADDRKEYA
 * リストの探索とタイムアウト処理を行う。
 *
 * @param p 管理構造体
 * @param u32Addr 探索アドレス。
 * @param pu32Key キー情報。NULLの場合はタイムアウト処理のみ行う
 * @return
 */
bool_t ADDRKEYA_bFind(tsAdrKeyA_Context *p, uint32 u32Addr, uint32 *pu32Key) {
	int i;
	for(i = 0; i < ADDRKEYA_MAX_HISTORY; i++) {
		if (u32TickCount_ms - p->au32ScanListTick[i] > ADDRKEYA_TIMEOUT) {
			p->au32ScanListTick[i] = 0;
			p->au32ScanListAddr[i] = 0;
		} else {
			if (pu32Key && (u32Addr == p->au32ScanListAddr[i])) {
				*pu32Key = p->au32ScanListKey[i];
				return TRUE;
			}
		}
	}
	return FALSE;
}

/** @ingroup ADDRKEYA
 * リストへの追加を行う。
 *
 * @param p 管理構造体
 * @param u32Addr 探索アドレス
 * @param u32Key キー情報
 * */
void ADDRKEYA_vAdd(tsAdrKeyA_Context *p, uint32 u32Addr, uint32 u32Key) {
	int i, idxPrimary = ADDRKEYA_MAX_HISTORY, idxSecondary = ADDRKEYA_MAX_HISTORY, idxOldest = ADDRKEYA_MAX_HISTORY;
	uint32 u32TickDelta = 0;
	for(i = 0; i < ADDRKEYA_MAX_HISTORY; i++) {
		if (p->au32ScanListAddr[i] == u32Addr) {
			// 同じアドレスが見つかったら、こちらに登録する（アルゴリズム上無いはずだが）
			idxPrimary = i;
		}
		if (p->au32ScanListAddr[i] == 0) {
			// 空のエントリ
			idxSecondary = i;
		}

		if (p->au32ScanListAddr[i]) {
			// 一番古いエントリ
			uint32 u32diff = u32TickCount_ms - p->au32ScanListTick[i];
			if (u32diff >= u32TickDelta) {
				idxOldest = i;
				u32TickDelta = u32diff;
			}
		}
	}

	if (idxPrimary < ADDRKEYA_MAX_HISTORY) {
		i = idxPrimary;
	} else if (idxSecondary < ADDRKEYA_MAX_HISTORY){
		i = idxSecondary;
	} else {
		i = idxOldest;
	}

	if (i < ADDRKEYA_MAX_HISTORY) {
		p->au32ScanListAddr[i] = u32Addr;
		p->au32ScanListTick[i] = u32TickCount_ms;
		p->au32ScanListKey[i] = u32Key;
	}
}

