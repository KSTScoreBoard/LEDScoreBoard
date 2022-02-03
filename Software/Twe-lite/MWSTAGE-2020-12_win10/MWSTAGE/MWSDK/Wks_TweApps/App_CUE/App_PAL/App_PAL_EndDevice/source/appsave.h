/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef APPSAVE_H_
#define APPSAVE_H_

#include <jendefs.h>

/** @ingroup FLASH
 * フラッシュ格納データ構造体
 */
typedef struct _tsFlashApp {
	uint32 u32appkey;		//!<
	uint32 u32ver;			//!<

	uint32 u32appid;		//!< アプリケーションID
	uint32 u32chmask;		//!< 使用チャネルマスク（３つまで）

	uint32 u32baud_safe;	//!< ボーレート
	uint8 u8parity;         //!< パリティ 0:none, 1:odd, 2:even

	uint8 u8id;				//!< 論理ＩＤ (子機 1～100まで指定)
	uint8 u8ch;				//!< チャネル（未使用、チャネルマスクに指定したチャネルから選ばれる）
	uint8 u8pow;			//!< 出力パワー (0-3)

	uint8 u8wait;
	uint32 u32Slp;			//!< スリープ周期

	uint32 u32EncKey;		//!< 暗号化キー(128bitだけど、32bitの値から鍵を生成)
	uint32 u32Opt;			//!< 色々オプション

	uint16 u16RcClock;		//!< RCクロックキャリブレーション

	uint32 u32param;		//!< 選択したセンサ特有のパラメータ

#ifndef USE_CUE
	uint8 au8Event[137];		//!< イベントの内容
	uint8 u8EventNum;		//!< 設定したイベントの数
#endif
} tsFlashApp;


#endif /* APPSAVE_H_ */
