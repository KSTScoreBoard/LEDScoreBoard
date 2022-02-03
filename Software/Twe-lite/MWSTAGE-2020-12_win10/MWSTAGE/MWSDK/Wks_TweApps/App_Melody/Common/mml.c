/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <string.h>
#include <jendefs.h>
#include <utils.h>

#include "mml.h"
#include "common.h"
#include "modbus_ascii.h"

#define DBGMSG
#ifdef DBGMSG
extern tsModbusCmd sSerCmd; //!< シリアル入力系列のパーサー (modbus もどき)  @ingroup MASTER
extern tsFILE sSerStream; //!< シリアル出力ストリーム  @ingroup MASTER
#endif

extern tsTimerContext sTimerApp; // テンポ設定用に、本体アプリのタイマー管理構造体を徴用
                                 // 本来は、初期化関数などで構造体を渡されるべきである。

/**
 * 第３オクターブの各周波数[mHz]
 * ＊４オクターブ目で計算したので、２で割っている。
 */
static const uint32 au32MlToneHz[] = {
	415305/2, // A-
	440000/2, // A
	466164/2, // A+

	466164/2, // B-
	493883/2, // B
	523251/2, // B+

	246942/2, // C-
	261626/2, // C
	277183/2, // C+

	277183/2, // D-
	293665/2, // D
	311127/2, // D+

	311127/2, // E-
	329628/2, // E
	349228/2, // E+

	329628/2, // F-
	349228/2, // F
	369994/2, // F+

	369994/2, // G-
	391995/2, // G
	415305/2, // G+
};

/**
 * オクターブに対応する周波数を得る。
 * @param u8ToneIdx au32MlToneHzに対応する配列添字
 * @param u8Oct オクターブの番号
 * @return 周波数
 */
static inline uint32 u32GetHz(uint8 u8ToneIdx, uint8 u8Oct) {
	return (u8Oct < MML_OCT_MIN || u8Oct > MML_OCT_MAX) ? 0 :
			au32MlToneHz[u8ToneIdx] << (u8Oct - MML_OCT_MIN);
}

static const uint8 au8MmmlLenTotal[] = {
	255, // 1 (全音符)
	127, // 2
	63, // 4 (四分音符、デフォルト)
	31, // 8  (♪)
	15, // 16
	7, // 32
	3 // 終端
};

static const uint8 au8MmmlLenTone[] = {
	224, // 1 (全音符)
	112, // 2
	56, // 4 (四分音符、デフォルト)
	28, // 8  (♪)
	14, // 16
	7, // 32
	3 // 終端
};

static const uint8 au8MmmlLenToneStacc[] = {
	128, // 1 (全音符)
	64, // 2
	32, // 4 (四分音符、デフォルト)
	16, // 8  (♪)
	8, // 16
	4, // 32
	2, // 終端
};

static const uint8 au8MmmlLenToneLegat[] = {
	255, // 1 (全音符)
	127, // 2
	63, // 4 (四分音符、デフォルト)
	31, // 8  (♪)
	15, // 16
	7, // 32
	3 // 終端
};

/**
 * MML を初期化する
 * @param psTimer
 */
void MML_vInit(tsMML *psMML, tsTimerContext *psTimer) {
	memset(psMML, 0, sizeof(tsMML)); // 管理構造体の初期化
	psMML->u8_def_len = 2; // デフォルトの長さは四分音符
	memcpy(&psMML->sTimer, psTimer, sizeof(tsTimerContext)); // タイマ管理構造体をコピーする
}

/**
 * 長さ情報の取り出し（とりあえず２桁までの入力）
 * - 読み込んだあとの u8idx は数値データの最後
 * @param psMML
 */
static uint8 u8GetDigits(tsMML *psMML) {
	uint8 u8num = 0;
	uint8 c;

	c = psMML->au8MML[psMML->u8idx + 1];
	if (c  >= '0' && c <= '9') {
		u8num = c - '0';
		psMML->u8idx++;

		c = psMML->au8MML[psMML->u8idx + 1];
		if (c  >= '0' && c <= '9') {
			u8num = u8num * 10 + (c - '0');
			psMML->u8idx++;

			c = psMML->au8MML[psMML->u8idx + 1];
			if (c  >= '0' && c <= '9') {
				u8num = u8num * 10 + (c - '0');
				psMML->u8idx++;
			}
		}
	}

	return u8num;
}

/**
 * 長さ情報の取り出し
 * @param psMML
 */
static uint8 u8GetLenIdx(tsMML *psMML) {
	uint8 u8len;
	uint8 u8len_idx;

	// 数値の取り出し
	u8len = u8GetDigits(psMML);
#ifdef DBGMSG
		//V_PRINT("(%d)", u8len);
#endif
	if (u8len == 0) {
		return psMML->u8_def_len;
	}

	// 長さの判定 (1,2,4,8,16,32 を想定、ビットの位置を検出するだけ）
	for (u8len_idx = 0; u8len_idx <= 5; u8len_idx++) {
		if (u8len & (1 << u8len_idx)) {
			break;
		}
	}
	if (u8len_idx > 5) u8len_idx = psMML->u8_def_len;

	return u8len_idx;
}

/**
 * 音を出さない
 * @param psMML
 */
static inline void MML_vMute(tsMML *psMML) {
	psMML->sTimer.u16duty = 0;
	vTimerChangeHz(&psMML->sTimer);
}

/**
 * テンポの設定
 * @param psMML
 * @param u8tempo
 */
static void MML_vTempo(tsMML *psMML, uint8 u8tempo) {
	if (u8tempo == 0) u8tempo = 60; // デフォルトは60

	int t = u8tempo * 64 / 60;
	sTimerApp.u16Hz = t;
	vTimerChangeHz(&sTimerApp);
}

/**
 * MML の主処理部
 * @param psMML
 */
void MML_vInt(tsMML *psMML) {
	if (!psMML->bHoldPlay) return;

	if (psMML->u8_progress == psMML->u8_length_total) {
		// 新しい音符を解釈
		if (psMML->au8MML[psMML->u8idx] == 0) {
			// 終端ならおしまい
			if (psMML->bRepeat) {
				// 先頭に巻き戻して
				psMML->u8idx = 0;
				psMML->u8_length_total = 0;
				psMML->u8_progress = 0;
			} else {
				// 音を止めて return
				MML_vMute(psMML);
				return; // なにもしない
			}
		}

		// 音符を一つ取り出す [A-G]
		bool_t bFlag = TRUE;
		uint8 u8Tone = 0;
		int8 i8halftone = 0;
		uint8 u8len_idx = 0;

		while(bFlag) {
			uint8 c = psMML->au8MML[psMML->u8idx];

			// toupper
			if (c >= 'a' && c <= 'z') {
				c = c - 'a' + 'A';
			}

			if (c >= 'A' && c <= 'G') {
				bFlag = FALSE;
				u8Tone = psMML->au8MML[psMML->u8idx];

				// 半音の判定
				if (psMML->au8MML[psMML->u8idx + 1] == '#' || psMML->au8MML[psMML->u8idx + 1] == '+') {
					i8halftone = 1;
					psMML->u8idx++;
				} else
				if (psMML->au8MML[psMML->u8idx + 1] == '-') {
					i8halftone = -1;
					psMML->u8idx++;
				}

				// 音符の長さを取り出す
				u8len_idx = u8GetLenIdx(psMML);
				psMML->u8idx++;
				break; // while ループの離脱
			}

			switch(c) {
			case 'R':
				bFlag = FALSE;
				u8Tone = 0;

				// 休符の長さを取り出す
				u8len_idx = u8GetLenIdx(psMML);
				break;

			case '(': // レガート再生開始
				psMML->bLegart = TRUE;
				break;

			case ')': // レガート再生中止
				psMML->bLegart = FALSE;
				break;

			case '[': // Loop
				psMML->u8_ct_loop = u8GetDigits(psMML);
				if (psMML->u8_ct_loop == 0) {
					psMML->u8_ct_loop = 1;
				} else
				if (psMML->u8_ct_loop == 1) {
					psMML->u8_ct_loop = 0;
				} else {
					psMML->u8_ct_loop--;
				}
				psMML->u8_pos_loop_start = psMML->u8idx;
				break;

			case ']': // Loop
				if (psMML->u8_ct_loop) {
					psMML->u8_ct_loop--;
					psMML->u8idx = psMML->u8_pos_loop_start; // ループポイントへ移動
				}
				break;

			case 'L': // 未指定時の音符/休符の長さ指定
				psMML->u8_def_len = u8GetLenIdx(psMML);
				break;

			case 'O': // オクターブ
				_C {
					uint8 u8num = u8GetDigits(psMML);
					if (u8num >= MML_OCT_MIN && u8num <= MML_OCT_MAX) {
						psMML->u8_octave = u8num;
					}
				}
				break;

			case '<': // オクターブ up
				if (psMML->u8_octave < MML_OCT_MAX)
					psMML->u8_octave++;
				break;

			case '>': // オクターブ dn
				if (psMML->u8_octave > MML_OCT_MIN)
					psMML->u8_octave--;
				break;

			case 'T': // テンポ
				_C {
					uint8 u8num = u8GetDigits(psMML);
					if (u8num >= 32) {
						MML_vTempo(psMML, u8num);
					}
				}
				break;

			case 0x00:
				// 音を止めて return
				psMML->sTimer.u16duty = 0;
				vTimerChangeHz(&psMML->sTimer);
				return; // なにもしない
				break;

			default:
				break;
			}

			psMML->u8idx++;
		}

		// .
		bool_t bDot = FALSE;
		if (psMML->au8MML[psMML->u8idx] == '.') {
			bDot = TRUE;
			psMML->u8idx++;
		}

		// タイ符号
		bool_t bTie = FALSE;
		if (psMML->au8MML[psMML->u8idx] == '&') {
			bTie = TRUE;
			psMML->u8idx++;
		}

		// 再生
		psMML->u8_length_total = au8MmmlLenTotal[u8len_idx];
		psMML->u8_length_tone = (bTie || psMML->bLegart) ? au8MmmlLenTotal[u8len_idx] : au8MmmlLenTone[u8len_idx];
		psMML->u8_progress = 0;

		if (bDot && u8len_idx) {
			// 全音符の . は無効！
			psMML->u8_length_total += au8MmmlLenTotal[u8len_idx + 1] + 1;
			psMML->u8_length_tone += au8MmmlLenTotal[u8len_idx + 1] + 1;
		}

		if (u8Tone) {
			// 音符
			psMML->u8_tone_now = MML_TONE_CHAR_TO_IDX(u8Tone) + i8halftone;
		} else {
			// 休符
			psMML->u8_tone_now = 255;
		}


		if (psMML->u8_tone_now != 255) {
			// 音符
			psMML->sTimer.u16duty = 512;

			// 440Hzと441Hzの差を聞き取れる人も居るそうなので、高精度APIを使用する。
			vTimerChangeHzEx(
					&psMML->sTimer,
					u32GetHz(psMML->u8_tone_now, psMML->u8_octave));
#ifdef DBGMSG
			/*
			V_PRINT(LB"TIM %08X ct:%d dt:%d sc:%d",
					psMML->sTimer.u32reg,
					psMML->sTimer.u16ct_total,
					psMML->sTimer.u16duty,
					psMML->sTimer.u8PreScale
					); */
			V_PRINT(LB"%c[%dhz] %d/%d:", u8Tone, u32GetHz(psMML->u8_tone_now, psMML->u8_octave), psMML->u8_length_tone, psMML->u8_length_total);
#endif
		} else {
			// 休符
#ifdef DBGMSG
			V_PRINT(LB"R %d/%d:", psMML->u8_length_tone, psMML->u8_length_total);
#endif
			MML_vMute(psMML);
		}
	} else {
		psMML->u8_progress++;

		if (psMML->u8_progress > psMML->u8_length_tone) {
			// 音を止めて return
			MML_vMute(psMML);
#ifdef DBGMSG
			V_PRINT("X");
#endif
		} else {
#ifdef DBGMSG
			V_PRINT(".");
#endif
		}
	}
}

void MML_vPlay(tsMML *psMML, const uint8 *pu8Lang) {
	psMML->bHoldPlay = FALSE; // 割り込みハンドラの処理を止める(TODO:和音対応時のインタフェース整備)

	strncpy((void*)psMML->au8MML, (void*)pu8Lang, sizeof(psMML->au8MML));

	// 先頭に巻き戻す
	psMML->u8idx = 0;
	psMML->u8_length_total = 0;
	psMML->u8_progress = 0;
	psMML->u8_def_len = 2;
	psMML->u8_octave = 4;
	psMML->bLegart = FALSE;

	MML_vTempo(psMML, 60); // デフォルトのテンポに戻す

	psMML->bHoldPlay = TRUE;
}
