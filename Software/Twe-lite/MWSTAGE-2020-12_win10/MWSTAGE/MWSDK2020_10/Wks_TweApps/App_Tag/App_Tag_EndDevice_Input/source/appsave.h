/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef APPSAVE_H_
#define APPSAVE_H_

#include <jendefs.h>

#define PARAM_MAX_LEN 18

typedef struct _tsADXL345Param{
	uint16	u16ThresholdTap;
	uint16	u16Duration;
	uint16	u16Latency;
	uint16	u16Window;
	uint16	u16ThresholdFreeFall;
	uint16	u16TimeFreeFall;
	uint16	u16ThresholdActive;
	uint16	u16ThresholdInactive;
	uint16	u16TimeInactive;
}tsADXL345Param;

typedef union{
	uint8 au8Param[PARAM_MAX_LEN];
	tsADXL345Param sADXL345Param;
}tuParam;

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

	uint8 u8wait;			//!< センサーの時間待ち

	uint32 u32Slp;			//!< スリープ周期

	uint32 u32EncKey;		//!< 暗号化キー(128bitだけど、32bitの値から鍵を生成)
	uint32 u32Opt;			//!< 色々オプション

	uint16 u16RcClock;		//!< RCクロックキャリブレーション

	uint8 u8mode;			//!< センサの種類
	int16 i16param;			//!< 選択したセンサ特有のパラメータ
	uint8 bFlagParam;		//!< sADXL345Param の格納状況
	tuParam uParam;
} tsFlashApp;


#endif /* APPSAVE_H_ */
