/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * common.h
 *
 *  Created on: 2013/01/23
 *      Author: seigo13
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <serial.h>
#include <fprintf.h>
#include "serialInputMgr.h"

#include "ToCoNet.h"

void vDispInfo(tsFILE *psSerStream, tsToCoNet_NwkLyTr_Context *pc);

extern const uint32 u32DioPortWakeUp;
void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep);

bool_t bTransmitToParent(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len);

extern const uint8 au8EncKey[];

#endif /* COMMON_H_ */
