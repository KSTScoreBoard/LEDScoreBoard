/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "utils.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "Interactive.h"
#include "config.h"

#include "accel_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define X(c) (accels[0][c])
#define Y(c) (accels[1][c])
#define Z(c) (accels[2][c])

#define ABS(c) ( (c<0) ? -c:c )

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
//PRIVATE inline uint32 u32Calc_FutureValue( int16 x, int16 y, int16 z );
//PRIVATE inline uint32 u32Sqrt(uint32 u32num);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
uint16 frequency = 0;
uint8 samples = 0;
int16 future_value[128];
int16 accels[3][128];
uint32 th_move = 300;
uint32 th_tap = 300;
uint8 TapFrame = 0;
uint16 TapFrame_ms = 10;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
// 平方根の近似値を計算する
PRIVATE inline uint32 u32Sqrt(uint32 u32num)
{
	uint32 i;
	int32 a = u32num;

	if( a == 0 ){
		return 0;
	}

	a >>= 1;
	for( i=1; a>0; i++ ) a -= i;

	return i;
}

// 特徴量(加速度のノルム)を計算する
PRIVATE inline uint32 u32Calc_FutureValue( int16 x, int16 y, int16 z )
{
	return u32Sqrt( (x*x)+(y*y)+(z*z) );
}

PUBLIC void vAccelEvent_Init( uint16 u16frequency )
{
	uint8 i;

	for( i=0; i<3; i++ ){
		memset( accels[i], 0, sizeof(int16)*128 );
	}

	memset( future_value, 0, sizeof(int16)*128 );
	frequency = u16frequency;
	if(u16frequency < 100){
		TapFrame = 1;
	}else{
		TapFrame = (u16frequency*TapFrame_ms)/1000;
	}
	samples = 0;
	return;
}

PUBLIC bool_t bAccelEvent_SetData( int16* ai16x, int16*ai16y, int16* ai16z, uint8 u8splnum )
{
	if( u8splnum+samples > 128 ){
		return FALSE;
	}

	int16* p[3];
	p[0] = ai16x;
	p[1] = ai16y;
	p[2] = ai16z;

	uint8 i;
	for( i=0; i<3; i++ ){
		memcpy( accels[i]+samples, p[i], sizeof(int16)*u8splnum );
	}
/*
	for( i=0; i<u8splnum; i++){
		V_PRINTF(LB" %d: %6d, %6d, %6d", i, accels[0][i], accels[1][i], accels[2][i]);
	}
*/
	for(i=0; i<u8splnum; i++){
		future_value[samples+i] = ABS(u32Calc_FutureValue( ai16x[i], ai16y[i], ai16z[i] )-1000);
	}

	samples += u8splnum;

	return TRUE;
}

// イベントの検出
PUBLIC tsAccelEventData tsAccelEvent_GetEvent()
{
	tsAccelEventData sTap, sMove;
	tsAccelEventData sAccelEventData;
	memset( &sTap, 0x00, sizeof(tsAccelEventData) );
	memset( &sMove, 0x00, sizeof(tsAccelEventData) );
	memset( &sAccelEventData, 0x00, sizeof(tsAccelEventData) );

	uint8 u8e = 0;
	u8e = bAccelEvent_IsTap(&sTap) ? 1:0;
	u8e |= bAccelEvent_IsMove(&sMove) ? 2:0;

	switch(u8e)
	{
	case 0:
		//sAccelEventData.eEvent = ACCEL_EVENT_NONE;
		sAccelEventData = sTap;
		sAccelEventData.eEvent = ACCEL_EVENT_TAP;
		break;
	case 1:
		sAccelEventData = sTap;
		break;
	case 2:
		sAccelEventData = sMove;
		break;
	case 3:
		_C{
			// 先に発生した方を採用する
			if( sTap.u8StartSample <= sMove.u8StartSample ){
				sAccelEventData = sTap;
			}else{
				sAccelEventData = sMove;
			}
		}
	
	default:
		sAccelEventData.eEvent = ACCEL_EVENT_TAP;
//		sAccelEventData.eEvent = ACCEL_EVENT_NONE;
		break;
	}

/*	
	if(!bAccelEvent_IsTap(&sAccelEventData)){
		if(!bAccelEvent_IsMove(&sAccelEventData)){
			sAccelEventData.eEvent = ACCEL_EVENT_NONE;
		}
	}
*/

	return sAccelEventData;
}

// 今回のサンプル列がタップ(もしくはダブルタップ)かどうか判定する
PUBLIC bool_t bAccelEvent_IsTap( tsAccelEventData* sData )
{
	uint8 i;

	uint8 count = 0;
	uint8 tapcount = 0;

	uint32 peakpower = 0;
	uint32 peaklength = 0;
	uint8 startsample = 0;

	for( i=0; i<samples; i++ ){
#if 1
		if( future_value[i] >= th_tap ){
			count++;
			if(startsample == 0){
				startsample = i;
			}
			if( peakpower < future_value[i] ){
				peakpower = future_value[i];
			}
			if( peaklength < count ){
				peaklength = count;
			}
#else
		if( ABS(X(i)) >= th_tap || ABS(Y(i)) >= th_tap || ABS(Z(i)) >= th_tap ){
			count++;
			if(startsample == 0){
				startsample = i;
			}
			if( peakpower < future_value[i] ){
				peakpower = future_value[i];
			}
			if( peaklength < count ){
				peaklength = count;
			}
#endif
		}else{
			// この設定では実質1のみだが、幅を持たせられるように...
			if( 0 < count && count <= TapFrame ){
				sData->u8StartSample = startsample;
				tapcount++;
				break;
			}else{
				count = 0;
				startsample = 0;
			}
		}
	}

	if( tapcount ){
		sData->eEvent = ACCEL_EVENT_TAP;
		sData->u32PeakPower = peakpower;
		sData->u8PeakLength = peaklength;

		return TRUE;
	}
	return FALSE;
}

// 今回のサンプル列がMoveかどうか判定する
PUBLIC bool_t bAccelEvent_IsMove( tsAccelEventData* sData )
{
	uint8 i;

	uint8 count = 0;
	uint32 peakpower = 0;
	uint32 peaklength = 0;
	uint8 startsample = 0;

	for( i=0; i<samples; i++ ){
#if 1
		if( future_value[i] >= th_move ){
			count++;
			if(startsample == 0){
				startsample = i;
			}
			if( peakpower < future_value[i] ){
				peakpower = future_value[i];
			}
			if( peaklength < count ){
				peaklength = count;
			}
		}else{
			// 2サンプル以下はノイズとみなす
			if(peaklength > 5){
				sData->u8StartSample = startsample;
				break;
			}else{
				startsample = 0;
				count = 0;
			}
		}
#else
		if( ABS( X(i) ) >= th_move || ABS( Y(i) ) >= th_move|| ABS( Z(i) ) >= th_move ){
			count++;
			if( peakpower < future_value[i] ){
				peakpower = future_value[i];
			}
			if( peaklength < count ){
				peaklength = count;
			}
		}else{
			count == 0;
		}
#endif
	}

	if( peaklength > 5 ){
		sData->eEvent = ACCEL_EVENT_MOVE;
		sData->u32PeakPower = peakpower;
		sData->u8PeakLength = peaklength;

		return TRUE;
	}
	return FALSE;
}

// 現在持っているサンプル群の最後のサンプルを使用して上面を判定する
PUBLIC uint8 u8AccelEvent_Top( void )
{
	uint32 u32norm = u32Calc_FutureValue( accels[0][samples-1], accels[1][samples-1], accels[2][samples-1] );
	if( u32norm < 800 || 1200 < u32norm ){
		V_PRINTF(LB"Out of range : %d/ %d, %d, %d", u32norm, accels[0][28], accels[1][28], accels[2][28] );
		return 0xFF;		// 3軸の加速度の大きさが範囲を超えているため、おそらく動いているから判定不能
	}

	uint16 u16val = ABS(accels[0][samples-1]);
	uint16 u16y = ABS(accels[1][samples-1]);
	uint16 u16z = ABS(accels[2][samples-1]);
	uint8 u8face = 0;		// 0:x, 1:y, 2:z

	if( u16val < u16y ){
		u16val = u16y;
		u8face = 1;
	}
	if( u16val < u16z ){
		u16val = u16z;
		u8face = 2;
	}

	switch (u8face){
	case 0:
		if( accels[0][samples-1] >=0 ){
			return 4;
		}else{
			return 3;
		}
		break;
	case 1:
		if( accels[1][samples-1] >=0 ){
			return 2;
		}else{
			return 5;
		}
		break;
	case 2:
		if( accels[2][samples-1] >=0 ){
			return 1;
		}else{
			return 6;
		}
		break;
	
	default:
		break;
	}

	return 0xFF;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
