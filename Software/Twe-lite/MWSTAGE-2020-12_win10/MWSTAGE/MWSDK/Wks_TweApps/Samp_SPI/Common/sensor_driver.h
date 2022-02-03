/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/** @file
 *
 * @defgroup SNSDRV センサー入力処理
 * センサー入力時の入力状態処理およびセンサー間の抽象化を行う。
 */

#ifndef SENSOR_DRIVER_H_
#include "jendefs.h"
#include <AppHardwareApi.h>

#include "ToCoNet_event.h"

#define SENSOR_TAG_DATA_NOTYET (-32768) //!< センサー入力がまだ @ingroup SNSDRV
#define SENSOR_TAG_DATA_ERROR  (-32767)  //!< センサー入力がエラー @ingroup SNSDRV
#define IS_SENSOR_TAG_DATA_ERR(c) (c < -32760) //!< その他エラー @ingroup SNSDRV

/** @ingroup SNSDRV
 * センサーの処理状態
 */
typedef enum
{
	E_SNSOBJ_STATE_IDLE = 0x00,    //!< 開始前
	E_SNSOBJ_STATE_CALIBRATE,      //!< 調整中
	E_SNSOBJ_STATE_MEASURING,      //!< 測定中
	E_SNSOBJ_STATE_MEASURE_NEXT,   //!< 次の測定処理中
	E_SNSOBJ_STATE_COMPLETE = 0x80,//!< 測定完了
	E_SNSOBJ_STATE_INACTIVE = 0xff //!< 動作しない状態
} teState_SnsObj;

#define SENSOR_DRIVER_H_


#endif /* SENSOR_DRIVER_H_ */
