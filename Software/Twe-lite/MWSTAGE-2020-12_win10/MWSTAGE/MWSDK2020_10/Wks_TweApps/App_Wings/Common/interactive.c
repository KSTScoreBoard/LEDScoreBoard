#include <string.h>

// TWE common lib
#include "twecommon.h"
#include "tweutils.h"
#include "tweserial.h"
#include "tweprintf.h"

#include "twesercmd_gen.h"
#include "twesercmd_plus3.h"
#include "tweinputstring.h"
#include "twestring.h"

#include "twesettings.h"
#include "twesettings_std.h"
#include "twesettings_std_defsets.h"
#include "twesettings_cmd.h"
#include "twesettings_validator.h"

#include "tweinteractive.h"
#include "tweinteractive_defmenus.h"
#include "tweinteractive_settings.h"
#include "tweinteractive_nvmutils.h"

#include "twenvm.h"
#include "twesysutils.h"

// TWELITE hardware like
#include <jendefs.h>
#include <ToCoNet.h>
#include <AppHardwareApi.h>

// app specific
#include "App_Wings.h"
#include "config.h"
#include "common.h"

/**********************************************************************************
 * DEFINES
 **********************************************************************************/
#define STG_SAVE_BUFF_SIZE 64		//! セーブバッファサイズ

#define STGS_SET_VER 0x01			//! 設定バージョン
#define STGS_SET_VER_COMPAT 0x01	//! 互換性のある設定バージョン

#define STGS_MAX_SETTINGS_COUNT 16	//! 設定数の最大(確定設定リスト tsFinal の配列数を決める)

/**********************************************************************************
 * CONSTANTS
 **********************************************************************************/
/**
 * 追加設定の列挙体
 **/
typedef enum{
	E_TWESTG_DEFSETS_APPSTART = 0x80,
	E_TWESTG_DEFSETS_ENCKEY,
	E_TWESTG_DEFSETS_LAYER,
	E_TWESTG_DEFSETS_HIGHERADDRESS
} teTWESTG_APP_DEFSETS;

/*!
 * カスタムデフォルト(BASE)
 *   APPIDのデフォルト値を書き換えている
 */
uint8 au8CustomDefault_Base_Parent[] = {
	12,   // 総バイト数
	E_TWESTG_DEFSETS_APPID, (TWESTG_DATATYPE_UINT32 << 4) | 4, (APP_ID>>24)&0xFF,(APP_ID>>16)&0xFF,(APP_ID>>8)&0xFF,APP_ID&0xFF, // 6bytes
	E_TWESTG_DEFSETS_CHANNELS_3 , (TWESTG_DATATYPE_UINT16 << 4) | 2, 0x00, 0x80, // 6bytes
	E_TWESTG_DEFSETS_LOGICALID, TWESTG_DATATYPE_UNUSE,
};

uint8 au8CustomDefault_Base_Router[] = {
	10,   // 総バイト数
	E_TWESTG_DEFSETS_APPID, (TWESTG_DATATYPE_UINT32 << 4) | 4, 0x67,0x72,0x01,0x02, // 6bytes
	E_TWESTG_DEFSETS_CHANNELS_3 , (TWESTG_DATATYPE_UINT32 << 4) | 2, 0x00, 0x80,  // 6bytes
};

/**
 * 追加の設定
 **/
const TWESTG_tsElement SetParentSettings[] = {
	{ E_TWESTG_DEFSETS_ENCKEY,
		{TWESTG_DATATYPE_UINT32, sizeof(uint32), 0, 0, { .u32 = 0xA5A5A5A5 } },
		{ "KEY", "Encryption Key [HEX:32bit]", "" },
		{ E_TWEINPUTSTRING_DATATYPE_HEX, 8, 'k' },
		{ {.u32 = 0}, {.u32 = 0xFFFFFFFF}, TWESTGS_VLD_u32MinMax, NULL }
	},
	{ E_TWESTG_DEFSETS_LAYER,
		{ TWESTG_DATATYPE_UINT8, sizeof(uint8), 0, 0, { .u8 = 0} },
		{ "MOD", "Mode (Parent or Router)", "0:Parent\r\n1-63:Router(Layer)." },
		{ E_TWEINPUTSTRING_DATATYPE_DEC, 2, 'm' },
		{ {.u8 = 0}, {.u8 = 63}, TWESTGS_VLD_u32MinMax, NULL }
	},
	{ E_TWESTG_DEFSETS_HIGHERADDRESS,
		{ TWESTG_DATATYPE_UINT32, sizeof(uint32), 0 , 0, { .u32 = 0x00000000 } },
		{ "ADR", "Access point address [HEX:32bit]", "" },
		{ E_TWEINPUTSTRING_DATATYPE_HEX, 8, 'A' },
		{ {.u32 = 0}, {.u32 = 0xFFFFFFFF}, TWESTGS_VLD_u32MinMax, NULL }
	},
	{E_TWESTG_DEFSETS_VOID}
};

/*!
 * 設定定義(tsSettings)
 *   スロット0..7までの定義を記述
 */
const TWESTG_tsSettingsListItem SetList[2][5] = {
	{
		{ STGS_KIND_PARENT, TWESTG_SLOT_DEFAULT, 
			{ TWESTG_DEFSETS_BASE, SetParentSettings, NULL, 
			au8CustomDefault_Base_Parent, TWESTG_DEFCUST_REMOVE_CHAN1, NULL } },
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
	},
	{
		{ STGS_KIND_ROUTER, TWESTG_SLOT_DEFAULT, 
			{ TWESTG_DEFSETS_BASE, NULL, NULL, 
			au8CustomDefault_Base_Router, TWESTG_DEFCUST_REMOVE_CHAN1, NULL } },
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
		{ TWESTG_KIND_VOID, TWESTD_SLOT_VOID, { NULL }}, // TERMINATE
	},
};

/**
 * @brief 設定リストのメニュー名
 */
const uint8 SetList_names[][8] = { "", "ROUTER" };

/**
 * @brief Kind名
 */
const uint8 Kind_names[][8] = { "", "" };

/**
 * @brief メニュー定義
 */
const TWEINTRCT_tsFuncs asFuncs[] = {
	{ 0, (uint8*)"ROOT MENU", TWEINTCT_vSerUpdateScreen_defmenus, TWEINTCT_vProcessInputByte_defmenus, TWEINTCT_vProcessInputString_defmenus, TWEINTCT_u32ProcessMenuEvent_defmenus }, // standard settings
	{ 1, (uint8*)"CONFIG MENU", TWEINTCT_vSerUpdateScreen_settings, TWEINTCT_vProcessInputByte_settings, TWEINTCT_vProcessInputString_settings, TWEINTCT_u32ProcessMenuEvent_settings }, // standard settings
	{ 2, (uint8*)"EEPROM UTIL", TWEINTCT_vSerUpdateScreen_nvmutils, TWEINTCT_vProcessInputByte_nvmutils, TWEINTCT_vProcessInputString_nvmutils, TWEINTCT_u32ProcessMenuEvent_nvmutils }, // standard settings
	{ 0xFF, NULL, NULL, NULL }
};

#define MENU_CONFIG 1 // 設定モード
#define MENU_OTA 2    // OTAは２番目

/**********************************************************************************
 * VARIABLES
 **********************************************************************************/

/*!
 * tsFinal 構造体のデータ領域を宣言する
 */
TWESTG_DECLARE_FINAL(MYDAT, STGS_MAX_SETTINGS_COUNT, 16, 4); // 確定設定リストの配列等の宣言

/*!
 * 確定設定リスト
 */
TWESTG_tsFinal sFinal;

/**
 * @brief 種別の保存
 */
static uint8 u8AppKind = STGS_KIND_PARENT;

/**
 * @brief 現在のスロットの保存
 */
static uint8 u8AppSlot = 0;

/**
 * @brief メニューモード
 *   MSB が１はインタラクティブモード終了。
 */
static uint8 u8MenuMode = 0;

extern TWE_tsFILE sSer;

/**********************************************************************************
 * STATIC FUNCTIONS
 **********************************************************************************/

/*!
 * 確定設定リスト(tsFinal)から各設定を読み出す。
 * ※ コード可読性の観点からイテレータ(TWESTG_ITER_tsFinal_*)を用いた読み出しを行う。
 */
void vQueryAppData() {
	// 設定のクエリ
	TWESTG_ITER_tsFinal sp;
	extern tsAppData sAppData; // アプリケーションデータ

	TWESTG_ITER_tsFinal_BEGIN(sp, &sFinal); // init iterator
	if (!TWESTG_ITER_tsFinal_IS_VALID(sp)) return; //ERROR DATA

	while (!TWESTG_ITER_tsFinal_IS_END(sp)) { // end condition of iter
		switch (TWESTG_ITER_tsFinal_G_ID(sp)) { // get data as UINT32
		case E_TWESTG_DEFSETS_APPID:
			sAppData.u32appid = TWESTG_ITER_tsFinal_G_U32(sp); break;
		case E_TWESTG_DEFSETS_CHANNELS_3:
			sAppData.u32chmask = TWESTG_ITER_tsFinal_G_U16(sp)<<11; break;
		case E_TWESTG_DEFSETS_OPTBITS:
			sAppData.u32opt = TWESTG_ITER_tsFinal_G_U32(sp); break;
		case E_TWESTG_DEFSETS_ENCKEY:
			sAppData.u32enckey = TWESTG_ITER_tsFinal_G_U32(sp); break;
		case E_TWESTG_DEFSETS_POWER_N_RETRY:
			{
				uint8 data = TWESTG_ITER_tsFinal_G_U8(sp);
				sAppData.u8pow = data&0x0F;
				sAppData.u8retry = data>>4;
			}
			break;
		case E_TWESTG_DEFSETS_UARTBAUD:
			{
				uint16 data = TWESTG_ITER_tsFinal_G_U16(sp);
				sAppData.u32baud = (data&0x0FFF)*100;
				sAppData.u8parity = data>>12;
			}
			break;
		case E_TWESTG_DEFSETS_LAYER:
			sAppData.u8layer = TWESTG_ITER_tsFinal_G_U8(sp); break;
		case E_TWESTG_DEFSETS_HIGHERADDRESS:
			sAppData.u32AddrHigherLayer = TWESTG_ITER_tsFinal_G_U32(sp); break;
		}
		TWESTG_ITER_tsFinal_INCR(sp); // incrment
	}
}

/*!
 * 確定設定データを再構築する。
 * 
 * \param u8kind  種別
 * \param u8slot  スロット
 * \param bNoLoad TRUEならslotに対応するセーブデータを読み込まない。
 */
void vAppLoadData(uint8 u8kind, uint8 u8slot, bool_t bNoLoad) {
	// 値のチェック
	bool_t bOk = FALSE;
	if (u8kind == STGS_KIND_PARENT && u8slot < STGS_KIND_PARENT_SLOT_MAX) bOk = TRUE;
	else if (u8kind < STGS_KIND_MAX && u8slot < STGS_KIND_ROUTER_SLOT_MAX) bOk = TRUE;
	if (!bOk) {
		return;
	}

	/// tsFinal 構造体の初期化とロード
	// tsFinal 構造体の初期化
	TWESTG_INIT_FINAL(MYDAT, &sFinal);
	// tsFinal 構造体に基本情報を適用する
	TWESTG_u32SetBaseInfoToFinal(&sFinal, APP_ID, APPVER, STGS_SET_VER, STGS_SET_VER_COMPAT);
	// tsFinal 構造体に kind, slot より、デフォルト設定リストを構築する
	TWESTG_u32SetSettingsToFinal(&sFinal, u8kind, u8slot, SetList[u8kind]);
	// セーブデータがあればロードする
	TWESTG_u32LoadDataFrAppstrg(&sFinal, u8kind, u8slot, APP_ID, STGS_SET_VER_COMPAT, bNoLoad ? TWESTG_LOAD_OPT_NOLOAD : 0);
}

/*!
 * セーブ用に最初のセクタを計算する。
 * 
 * \param u8kind 種別
 * \param u8slot スロット
 * \param u32Opt オプション
 * \return 0xFF:error, その他:セクタ番号
 */
static uint8 s_u8GetFirstSect(uint8 u8kind, uint8 u8slot) {
	uint8 u8sec = 1; // 最初のセクターは飛ばす
	if (u8kind == 0) {
		if (u8slot < STGS_KIND_PARENT_SLOT_MAX) {
			u8sec = 1 + u8slot * 2; // parent は 1-2 (1block)
		} else {
			u8sec = 0xFF;
		}
	} else {
		if (u8slot < STGS_KIND_ROUTER_SLOT_MAX) {
			u8sec = 3 + STGS_KIND_PARENT_SLOT_MAX * 2 + ((u8kind - 1) * STGS_KIND_ROUTER_SLOT_MAX + u8slot) * 2;
		} else {
			u8sec = 0xFF;
		}
	}
	if (u8sec > EEPROM_6X_USER_SEGMENTS) u8sec = 0xFF;

	return u8sec;
}

/*!
 * データセーブを行う。
 * twesettings ライブラリから呼び出されるコールバック関数。
 * 
 * \param pBuf   データ領域 pBuf->pu8buff[-16..-1] を利用することができる。
 * \param u8kind 種別
 * \param u8slot スロット
 * \param u32Opt オプション
 * \param ...
 * \return TWE_APIRET
 */
TWE_APIRET TWESTG_cbu32SaveSetting(TWE_tsBuffer *pBuf, uint8 u8kind, uint8 u8slot, uint32 u32Opt, TWESTG_tsFinal *psFinal) {
	uint8 u8sect = s_u8GetFirstSect(u8kind, u8slot);
	if (u8sect != 0xFF) {
		bool_t bRes = TWENVM_bWrite(pBuf, u8sect); //先頭セクターはコントロールブロックとして残し、2セクター単位で保存
		return bRes ? TWE_APIRET_SUCCESS : TWE_APIRET_FAIL;
	} else return TWE_APIRET_FAIL;
}

/**
 * データロードを行う。
 * twesettings ライブラリから呼び出されるコールバック関数。
 * 
 * @param pBuf 		データ領域 pBuf->pu8buff[-16..-1] を利用することができる。
 * @param u8kind 	種別
 * @param u8slot 	スロット
 * @param u32Opt 	オプション
 * @param ... 
 * @return TWE_APIRET 
 */
TWE_APIRET TWESTG_cbu32LoadSetting(TWE_tsBuffer *pBuf, uint8 u8kind, uint8 u8slot, uint32 u32Opt, TWESTG_tsFinal *psFinal) {
	uint8 u8sect = s_u8GetFirstSect(u8kind, u8slot);
	if (u8sect != 0xFF) {
		bool_t bRes = TWENVM_bRead(pBuf, u8sect); //先頭セクターはコントロールブロックとして残し、2セクター単位で保存
		return bRes ? TWE_APIRET_SUCCESS : TWE_APIRET_FAIL;
	} else return TWE_APIRET_FAIL;
}

/*!
 * 諸処理を行うコールバック。
 * 主としてインタラクティブモードから呼び出されるが、一部は他より呼び出される。
 * 
 * \param pContext インタラクティブモードのコンテキスト(NULLの場合はインタラクティブモード以外からの呼び出し)
 * \param u32Op    コマンド番号
 * \param u32Arg1  引数１（役割はコマンド依存）
 * \param u32Arg2  引数２（役割はコマンド依存）
 * \param vpArg    引数３（役割はコマンド依存、主としてデータを戻す目的で利用する）
 * \return コマンド依存の定義。TWE_APIRET_FAILの時は何らかの失敗。
 */
TWE_APIRET TWEINTRCT_cbu32GenericHandler(TWEINTRCT_tsContext *pContext, uint32 u32Op, uint32 u32Arg1, uint32 u32Arg2, void *vpArg) {
	uint32 u32ApiRet = TWE_APIRET_SUCCESS;

	switch (u32Op) {
	case E_TWEINTCT_MENU_EV_LOAD:
		u8MenuMode = (uint8)u32Arg1;
		// メニューロード時の KIND/SLOT の決定。
		if (u8MenuMode == MENU_CONFIG) {
			// 通常メニュー
			u8AppKind = STGS_KIND_PARENT;
			u8AppSlot = TWESTG_SLOT_DEFAULT;
			vAppLoadData(u8AppKind, u8AppSlot, FALSE); // 設定を行う
			u32ApiRet = TWE_APIRET_SUCCESS_W_VALUE((uint32)u8AppKind << 8 | u8AppSlot);
		} else if (u8MenuMode == MENU_OTA) {
			// OTAモードに入る
			if (u8AppKind == STGS_KIND_PARENT) {
				u8AppKind = STGS_KIND_ROUTER;
				u8AppSlot = TWESTG_SLOT_DEFAULT;
			}
			vAppLoadData(u8AppKind, u8AppSlot, FALSE); // 設定を行う
			u32ApiRet = TWE_APIRET_SUCCESS_W_VALUE((uint32)u8AppKind << 8 | u8AppSlot);
		}
		break;

	case E_TWEINRCT_OP_UNHANDLED_CHAR: // 未処理文字列があった場合、呼び出される。
		break;

	case E_TWEINRCT_OP_RESET: // モジュールリセットを行う
		TWE_fprintf(&sSer, "\r\n!INF RESET SYSTEM...");
		TWE_fflush(&sSer);
		vAHI_SwReset();
		break;

	case E_TWEINRCT_OP_REVERT: // 設定をもとに戻す。ただしセーブはしない。
		vAppLoadData(u8AppKind, u8AppSlot, u32Arg1);
		break;

	case E_TWEINRCT_OP_CHANGE_KIND_SLOT: 
		// KIND/SLOT の切り替えを行う。切り替え後 pContext->psFinal は、再ロードされること。
		// u32Arg1,2 0xFF: no set, 0xFF00: -1, 0x0100: +1, 0x00?? Direct Set

		//メニューモードが CONFIG の時は、Parent の設定のみ行う
		if (u8MenuMode == MENU_CONFIG) {
			// 0, 0 決め打ち
			u8AppKind = STGS_KIND_PARENT;
			u8AppSlot = TWESTG_SLOT_DEFAULT;
		} else { // OTA モード
			// KIND の設定
			if (u32Arg1 != 0xFF) {
				if ((u32Arg1 & 0xff00) == 0x0000) {
					u8AppSlot = u32Arg1 & 0xFF;
				}
				else {
					if ((u32Arg1 & 0xff00) == 0x0100) {
						u8AppKind++;
					}
					else {
						u8AppKind--;
					}
				}

				// KIND 数のチェック (0 は Parent なので飛ばす)
				if (u8AppKind == 0xFF || u8AppKind == 0) u8AppKind = STGS_KIND_MAX - 1;
				else if (u8AppKind >= STGS_KIND_MAX) u8AppKind = 1; 
				
				// SLOT は指定がない場合は０に戻す
				if (u32Arg2 == 0xFF) u8AppSlot = 0;
			}
			
			// SLOT の設定
			if (u32Arg2 != 0xFF) {
				if ((u32Arg2 & 0xff00) == 0x0000) {
					u8AppSlot = u32Arg2 & 0x7;
				}
				else {
					if ((u32Arg2 & 0xff00) == 0x0100) {
						u8AppSlot++;
					}
					else {
						u8AppSlot--;
					}
				}

				// SLOT 数のチェック
				if (u8AppKind == STGS_KIND_PARENT) {
					if (u8AppSlot == 0xFF) u8AppSlot = STGS_KIND_PARENT_SLOT_MAX - 1;
					else if (u8AppSlot >= STGS_KIND_PARENT_SLOT_MAX) u8AppSlot = 0;
				} else {
					if (u8AppSlot == 0xFF) u8AppSlot = STGS_KIND_ROUTER_SLOT_MAX - 1;
					if (u8AppSlot >= STGS_KIND_ROUTER_SLOT_MAX) u8AppSlot = 0;
				}
			}
		}
		
		// データの再ロード（同じ設定でも再ロードするのが非効率だが・・・）
		vAppLoadData(u8AppKind, u8AppSlot, FALSE); // 設定を行う

		// 値を戻す。
		// ここでは設定の失敗は実装せず、SUCCESS としている。
		// VALUE は現在の KIND と SLOT。
		u32ApiRet = TWE_APIRET_SUCCESS_W_VALUE((uint16)u8AppKind << 8 | u8AppSlot);
		break;

	case E_TWEINRCT_OP_WAIT: // 一定時間待つ（ポーリング処理）
		TWESYSUTL_vWaitPoll(u32Arg1);
		break;

	case E_TWEINRCT_OP_GET_APPNAME: // CONFIG行, アプリ名
		if (vpArg != NULL) {
			// &(char *)vpArg: には、バッファ16bytesのアドレスが格納されているため strcpy() でバッファをコピーしてもよいし、
			// 別の固定文字列へのポインタに書き換えてもかまわない。
			*((uint8**)vpArg) = (uint8*)"App_Wings";
		}
		break;

	case E_TWEINRCT_OP_GET_KINDNAME: // CONFIG行, KIND種別名
		if (vpArg != NULL) {
			// &(char *)vpArg: には、バッファ16bytesのアドレスが格納されているため strcpy() でバッファをコピーしてもよいし、
			// 別の固定文字列へのポインタに書き換えてもかまわない。

			// このスコープ内に const uint8 SetList_names[][8] = { .. }; としても、うまくいかない。理由不明。
			*((uint8**)vpArg) = (uint8*)SetList_names[u32Arg1];
		}
		break;

	case E_TWEINTCT_OP_GET_OPTMSG:
		if (vpArg != NULL) {
			// &(char *)vpArg: には、バッファ32bytesのアドレスが格納されているため strcpy() でバッファをコピーしてもよいし、
			// 別の固定文字列へのポインタに書き換えてもかまわない。

			// このコードは -Os 最適化では不具合を起こす場合がある（case 節内にあるのが原因？）
			TWE_snprintf(*((char**)vpArg), 32, "v%d-%02d-%d/SID=%08X", VERSION_MAIN, VERSION_SUB, VERSION_VAR, ToCoNet_u32GetSerial() );
		}
		break;

	case E_TWEINTCT_OP_GET_SID: // シリアル番号
		if (vpArg != NULL) {
			// シリアル値を書き込む
			*((uint32*)vpArg) = ToCoNet_u32GetSerial();
		}
		break;

	default:
		break;
	}

	return u32ApiRet;
}