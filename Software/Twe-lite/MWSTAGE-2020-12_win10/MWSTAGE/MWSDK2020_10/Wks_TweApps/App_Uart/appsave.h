/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef APPSAVE_H_
#define APPSAVE_H_

#include <jendefs.h>

#define FLASH_APP_AES_KEY_SIZE 16
#define FLASH_APP_HANDLE_NAME_LEN 23

/** @ingroup FLASH
 * フラッシュ格納データ構造体
 */
typedef struct _tsFlashApp {
	uint32 u32appkey;		//!<
	uint32 u32ver;			//!<

	uint32 u32appid;		//!< アプリケーションID
	uint32 u32chmask;		//!< 使用チャネルマスク（３つまで）
	uint8 u8id;				//!< 論理ＩＤ (子機 1～100まで指定)
	uint8 u8ch;				//!< チャネル（未使用、チャネルマスクに指定したチャネルから選ばれる）
	uint8 u8role;			//!< 未使用(将来のための拡張)
	uint8 u8layer;			//!< 未使用(将来のための拡張)
	uint16 u16power;		//!< 無線設定 (下４ビット 出力 0:最小,1,2,3:最大/上４ビット Regular/Strongのみ高速モード 0:250kbps,1:500,2:667)

	uint32 u32baud_safe;	//!< ボーレート
	uint8 u8parity;         //!< パリティ 下2bit(パリティ 0:none, 1:odd, 2:even) 3bit(ストップビット 0:1, 1:2) 4bit(ビット 0:8bit, 1:7bit)

	uint8 u8uart_mode;		//!< UART の動作モード (0:透過, 1:テキスト電文, 2:バイナリ電文)

	uint16 u16uart_lnsep;		//!< UART の行セパレータ (透過モードでの伝送時)
	uint8 u8uart_lnsep_minpkt; //!<  行単位送信時の最小パケットサイズ
	uint8 u8uart_txtrig_delay; //!< Dモード時の送信トリガー時間（最小パケットサイズより優先する）

	uint8 au8ChatHandleName[FLASH_APP_HANDLE_NAME_LEN + 1]; //!< チャットモードのハンドル名

	uint8 u8Crypt;          //!< 暗号化を有効化するかどうか (1:AES128)
	uint8 au8AesKey[FLASH_APP_AES_KEY_SIZE + 1];    //!< AES の鍵

	uint32 u32Opt;			//!< その他オプション
} tsFlashApp;


#endif /* APPSAVE_H_ */
