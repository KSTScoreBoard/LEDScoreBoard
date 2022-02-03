/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#ifndef COMMON_H_
#define COMMON_H_

#include <serial.h>
#include <fprintf.h>

#include "ToCoNet.h"

void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep);
void vResetWithMsg(tsFILE *psSerStream, string str);

bool_t bTransmitToParent(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len);
bool_t bRegAesKey(uint32 u32seed);
bool_t bGetPALOptions( void );

extern const uint8 au8EncKey[];
extern uint32 u32DioPortWakeUp;

/*
 * パケット識別子
 */
#define PKT_ID_NOCONNECT 0x00
#define PKT_ID_MAG 0x01
#define PKT_ID_AMB 0x02
#define PKT_ID_MOT 0x03
#define PKT_ID_LED 0x04
#define PKT_ID_IRC 0x05

/*
 * 標準ポート定義 (TWELITE PAL)
 */
// 子機用配置
//#warning "IO CONF IS FOR ENDDEVICE"
#define INPUT_DIP1 1
#define INPUT_DIP2 2
#define INPUT_DIP3 3
#define INPUT_DIP4 4

#define EH_BOOT 4

#define OUTPUT_LED 5

#define INPUT_D0 0
#define INPUT_D8 8
#define INPUT_PC 8

#define CLK_IN 9
#define CLK_EN 10

#define CLD_IN 11
#define INPUT_SWSET 12
#define INPUT_SW 12
#define INPUT_SET 12
#define WDT_OUT 13

#define SNS_EN 16
#define SNS_INT 17

#endif /* COMMON_H_ */