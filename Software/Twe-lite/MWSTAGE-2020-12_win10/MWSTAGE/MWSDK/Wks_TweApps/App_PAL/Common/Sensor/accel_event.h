/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef  ACCELEVENT_INCLUDED
#define  ACCELEVENT_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
// Event
typedef enum{
	ACCEL_EVENT_NONE = 0,
	ACCEL_EVENT_MOVE = 1,
	ACCEL_EVENT_TAP = 2,
	ACCEL_EVENT_DTAP = 3,
	ACCEL_EVENT_FREEFALL = 4,
	ACCEL_EVENT_NOTANALYZED = 0xFE,
	ACCEL_EVENT_END = 0xFF
} teAccelEvent;

// イベントの推定結果を返すための構造体
typedef struct
{
	teAccelEvent eEvent;
	uint8 u8TapCount;
	uint8 u8TapTime;
	uint8 u8PeakLength;
	uint32 u32PeakPower;
	uint8 u8StartSample;
} tsAccelEventData;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
PUBLIC void vAccelEvent_Init( uint16 u16frequency );
PUBLIC bool_t bAccelEvent_SetData( int16* ai16x, int16*ai16y, int16* ai16z,  uint8 u8splnum );
PUBLIC tsAccelEventData tsAccelEvent_GetEvent();
PUBLIC bool_t bAccelEvent_IsTap( tsAccelEventData* sData );
PUBLIC bool_t bAccelEvent_IsMove( tsAccelEventData* sData );
PUBLIC uint8 u8AccelEvent_Top( void );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* ACCELEVENT_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

