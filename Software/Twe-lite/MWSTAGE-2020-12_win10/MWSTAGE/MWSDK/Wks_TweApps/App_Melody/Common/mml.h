/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 *  Created on: 2013/07/17
 *      Author: seigo13
 */

#ifndef MML_H_
#define MML_H_

#include <jendefs.h>
#include <utils.h>

#define MML_TONE_C 'C'
#define MML_TONE_D 'D'
#define MML_TONE_E 'E'
#define MML_TONE_F 'F'
#define MML_TONE_G 'G'
#define MML_TONE_A 'A'
#define MML_TONE_B 'B'

#define MML_OCT_MAX 9 //!< オクターブの最大
#define MML_OCT_MIN 3 //!< オクターブの最小

/**
 * u32MmlToneHz テーブルに対応するインデックスを計算する
 */
#define MML_TONE_CHAR_TO_IDX(c) (((c) - 'A') * 3 + 1)

/**
 * 各トーンの周波数
 */
extern const uint16 u16MmlToneHz[]; // TONE 周波数テーブル

/**
 * 各音符長指定に対応するカウント値 (64カウント＝全音符)
 */
extern const uint8 u8MmmlLenTotal[];

/**
 * 各音符長指定に対して実際に音が鳴るカウント値 (音の切れ目を明確にするため)
 */
extern const uint8 u8MmmlLenTone[];

/**
 * 各音符長指定に対して実際に音が鳴るカウント値 (音の切れ目を明確にするため)
 */
extern const uint8 u8MmmlLenToneStacc[];


/** @ingroup MML
 * MML管理構造体
 */
typedef struct {
	tsTimerContext sTimer; //! PWMタイマーコンテキスト

	uint8 au8MML[128]; //! MML文字列
	uint8 u8idx; //! MML 文字列の次の命令位置

	bool_t bLegart; //!< レガート再生中
	uint8 u8_pos_loop_start; //!< ループの開始位置
	uint8 u8_ct_loop; //!< ループ回数

	uint8 bRepeat; //!< リピート再生

	uint8 u8_tone_now; //!< 現在の音程
	uint8 u8_length_tone; //!< 長さ (64カウントを全音符とした再生長さで音が出る部分 80%)
	uint8 u8_length_total; //!< 長さ (64カウントを全音符とし音符の長さ)
	uint8 u8_progress; //!< 音符の進行

	uint8 u8_def_len; //!< 長さのデフォルト値
	uint8 u8_octave; //!< オクターブ (O4 がデフォルト, O5,6,7まで)

	bool_t bHoldPlay; //!< TRUEになってから割り込みハンドラが処理を開始するフラグ
} tsMML;

// 四分音符 l_total=16ct, l_tone=14
// 八分音符 l_total=8ct, l_tone=7
// １６分音符 l_total=4ct, l_tone=4

/*
 * 関数定義
 */

void MML_vInit(tsMML *psMML, tsTimerContext *psTimer);
void MML_vInt(tsMML *psMML);
void MML_vPlay(tsMML *psMML, const uint8 *pu8Lang);

#endif /* MML_H_ */
