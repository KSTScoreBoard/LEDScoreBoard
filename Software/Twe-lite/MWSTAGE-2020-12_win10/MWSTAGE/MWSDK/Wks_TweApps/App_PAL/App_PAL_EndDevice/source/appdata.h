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
	// ToCoNet version
	uint32 u32ToCoNetVersion;
	uint8 u8DebugLevel; //!< ToCoNet のデバッグ

	// wakeup status
	bool_t bWakeupByButton; //!< DIO で RAM HOLD Wakeup した場合 TRUE
	uint8 u8WakeupByTimer;	//!< DIOで起きた場合0、WakeTimer0は1、Waketimer1は2
	uint32 u32WakeupDIOStatus;	//!< 起床したときのDIO
	bool_t bColdStart; //!< TRUEだったらColdStart

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
	uint8 u8Batt;
	uint16 u16Adc[4];
	uint16 u16PC;

	uint32 u32SleepCount;

	uint8 u8SnsID;
	uint8 u8DIPSW;
	uint8 u8LID;

	uint32 u32DIO_startup;
	uint32 u32Sleep_min;
	uint8 u8Sleep_sec;

	// その他
	tsFlash sFlash; //!< フラッシュの情報
	bool_t bFlashLoaded; //!< フラッシュにデータが合った場合は TRUE

	uint8 u8LedState; //!< LED状態 (0: 消灯 1: 点灯 2: ブリンク)
	uint32 u32LedTick; //!< Led Tick から一定期間点灯する

	uint8 u8SettingsID; //!< 設定の保存先のIDを保持する

} tsAppData;

extern tsAppData sAppData;

#endif /* APPDATA_H_ */
