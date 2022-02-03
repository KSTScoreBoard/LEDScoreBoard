/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#ifndef  MULTISENSOR_H_INCLUDED
#define  MULTISENSOR_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
// センサ類のインクルードファイル
#include "sensor_driver.h"
#include "SHT21.h"
#include "SHT31.h"
#include "SHTC3.h"
#include "ADT7410.h"
#include "MPL115A2.h"
#include "LIS3DH.h"
#include "ADXL345.h"
#include "TSL2561.h"
#include "L3GD20.h"
#include "S1105902.h"
#include "BME280.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define USE_SHT21		0x00000001
#define USE_ADT7410		0x00000002
#define USE_MPL115A2	0x00000004
#define USE_LIS3DH		0x00000008
#define USE_ADXL34x		0x00000010
#define USE_TSL2561		0x00000020
#define USE_L3GD20		0x00000040
#define USE_S1105920	0x00000080
#define USE_BME280		0x00000100
#define USE_SHT31		0x00000200
#define USE_SHTC3		0x00000400

#define MAX_SNS 11

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	uint8 u8SnsName;		// PKT_ID_ ... を入れる
	bool_t bSnsEnable;		// 該当のセンサが有効かどうか
	tsSnsObj sSnsObj;		// センサの管理構造体
	void* tsObjData;		// センサの読み値などを入れる構造体
} tsSnsObjAll;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
// 各センサごとの取得した値などを入れる構造体
tsObjData_SHT21 sObjSHT21;
tsObjData_ADT7410 sObjADT7410;
tsObjData_MPL115A2 sObjMPL115A2;
tsObjData_LIS3DH sObjLIS3DH;
tsObjData_ADXL345 sObjADXL345;
tsObjData_TSL2561 sObjTSL2561;
tsObjData_L3GD20 sObjL3GD20;
tsObjData_S1105902 sObjS1105902;
tsObjData_BME280 sObjBME280;
tsObjData_SHT31 sObjSHT31;
tsObjData_SHTC3 sObjSHTC3;

#endif  /* MULTISENSOR_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
