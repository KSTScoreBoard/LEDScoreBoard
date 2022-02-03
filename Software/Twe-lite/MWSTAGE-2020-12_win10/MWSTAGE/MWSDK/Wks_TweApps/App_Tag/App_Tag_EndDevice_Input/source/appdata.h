/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef APPDATA_H_
#define APPDATA_H_

#include <jendefs.h>

#include "config.h"
#include "ToCoNet.h"
#include "flash.h"
#include "sensor_driver.h"
#include "btnMgr.h"

#include "adc.h"

typedef struct {
	// sensor data
	uint8 u8Batt;
	uint16 u16Adc1, u16Adc2, u16Adc3, u16Adc4;
	uint16 u16PC1, u16PC2;
	uint16 u16Temp, u16Humid;
} tsSensorData;

typedef struct {
	// ToCoNet version
	uint32 u32ToCoNetVersion;
	uint8 u8DebugLevel; //!< ToCoNet のデバッグ

	// wakeup status
	bool_t bWakeupByButton; //!< DIO で RAM HOLD Wakeup した場合 TRUE

	// Network context
	tsToCoNet_Nwk_Context *pContextNwk;
	tsToCoNet_NwkLyTr_Config sNwkLayerTreeConfig;
	uint8 u8NwkStat;

	// frame count
	uint16 u16frame_count;

	//	Retry
	uint8 u8Retry;

	// ADC
	tsObjData_ADC sObjADC; //!< ADC管理構造体（データ部）
	tsSnsObj sADC; //!< ADC管理構造体（制御部）
	uint8 u8AdcState; //!< ADCの状態 (0xFF:初期化前, 0x0:ADC開始要求, 0x1:AD中, 0x2:AD完了)

	// config mode
	bool_t bConfigMode; // 設定モード

	// センサ情報
	tsSensorData sSns;
	uint32 u32DI1_Dur_Opened_ms; //!< DI1 が Hi になってからの経過時間
	uint16 u16DI1_Ct_PktFired; //! DI1 の無線パケット発射回数
	bool_t bDI1_Now_Opened; //!< DI1 が Hi なら TRUE

	// その他
	tsFlash sFlash; //!< フラッシュの情報
	bool_t bFlashLoaded; //!< フラッシュにデータが合った場合は TRUE
	uint8 u8LedState; //!< LED状態 (0: 消灯 1: 点灯 2: ブリンク)
	uint32 u32LedTick; //!< Led Tick から一定期間点灯する
	uint8 u8SettingsID; //!< 設定の保存先のIDを保持する

	// button manager
	tsBTM_Config sBTM_Config; //!< ボタン入力（連照により状態確定する）管理構造体
	PR_BTM_HANDLER pr_BTM_handler; //!< ボタン入力用のイベントハンドラ (TickTimer 起点で呼び出す)
} tsAppData_Ed;

extern tsAppData_Ed sAppData_Ed;

#endif /* APPDATA_H_ */
