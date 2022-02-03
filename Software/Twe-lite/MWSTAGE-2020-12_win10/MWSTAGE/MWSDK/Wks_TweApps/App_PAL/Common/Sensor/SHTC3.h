/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef  HUM_SHTC3_INCLUDED
#define  HUM_SHTC3_INCLUDED

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "sensor_driver.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define SHTC3_IDX_TEMP 0
#define SHTC3_IDX_HUMID 1
#define SHTC3_IDX_BEGIN 0
#define SHTC3_IDX_END (SHTC3_IDX_HUMID+1) // should be (last idx + 1)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// data
	int16 ai16Result[2];

	// working
	uint8 u8TickCount, u8TickWait;
	uint8 u8IdxMeasuruing;
} tsObjData_SHTC3;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/
void vSHTC3_Init(tsObjData_SHTC3 *pData, tsSnsObj *pSnsObj);
void vSHTC3_Final(tsObjData_SHTC3 *pData, tsSnsObj *pSnsObj);

#define i16SHTC3_GetTemp(pSnsObj) ((tsObjData_SHTC3 *)(pSnsObj->pData)->ai16Result[SHTC3_IDX_TEMP])
#define i16SHTC3_GetHumd(pSnsObj) ((tsObjData_SHTC3 *)(pSnsObj->pData)->ai16Result[SHTC3_IDX_HUMID])

#define SHTC3_DATA_NOTYET	(-32768)
#define SHTC3_DATA_ERROR	(-32767)

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
PUBLIC bool_t bSHTC3reset();
PUBLIC bool_t bSHTC3startRead();
PUBLIC int16 i16SHTC3readResult(int16*, int16*);

PUBLIC bool_t bSHTC3sleep();
PUBLIC bool_t bSHTC3wakeup();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#endif  /* HUM_SHTC3_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

