/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "MC3630.h"
#include "SPI.h"

#include "ccitt8.h"

#include "utils.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sDebugStream;
#endif
tsFILE sSerStream;

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: bMC3630reset
 *
 * DESCRIPTION:
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
//	リセットというよりは初期化処理
PUBLIC bool_t bMC3630reset()
{
	bool_t bOk = TRUE;

    /* configure SPI interface */
	/*	SPI Mode3	*/
	vSPIInit( SPI_MODE3, SLAVE_ENABLE1, 3 );

	// Standby
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_MODE_C | MC3630_WRITE);
	vSPIWrite(0x01);
	vSPIStop();

	// Reset
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_RESET | MC3630_WRITE);
	vSPIWrite(0x40);
	vSPIStop();

	// よくわからんけどおまじない
//#ifdef CHARM
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(0x1B | MC3630_WRITE);
	vSPIWrite(0x00);
	vSPIStop();
//#endif
	// Wait at least 1ms.
	vWait(5000);

	// SPIに設定
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_FREG_1 | MC3630_WRITE);
	vSPIWrite(0x80);
	vSPIStop();

	// ここからおまじない
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_INIT_1 | MC3630_WRITE);
	vSPIWrite(0x42);
	vSPIStop();

	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_DMX | MC3630_WRITE);
	vSPIWrite(0x1);
	vSPIStop();

	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_DMY | MC3630_WRITE);
	vSPIWrite(0x80);
	vSPIStop();

	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_INIT_2 | MC3630_WRITE);
	vSPIWrite(0x00);
	vSPIStop();

	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_INIT_3 | MC3630_WRITE);
	vSPIWrite(0x00);
	vSPIStop();
	// ここまでおまじない

	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_CHIP_ID | MC3630_READ);
	vSPIWrite(0x00);
	uint8 u8Result = u8SPIRead();
	vSPIStop();
	if( u8Result != 0x71 ){
		bOk = FALSE;
		vfPrintf(&sSerStream, "\n\rMC3630 Not Connected. %02X", u8Result );
	}

	// 一旦STANBYモードにする
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_MODE_C | MC3630_WRITE);
	vSPIWrite(0x01);
	vSPIStop();

	// SPIのクロックと加速度計測をUltraLowPowerに変更する
	vSPIChipSelect(CS_DIO19);
	//vSPIWrite(MC3630_PMCR | MC3630_WRITE);
	vSPIWrite(0x80|0x30|0x03);
	vSPIWrite(0x30|0x03);
	vSPIStop();

	// SPIをハイスピードモードに変更したのでクロックの周波数を変更する
	// 一応8MHzで動くらしいが、動かないことがあるので、4MHzで動かす
	vSPIInit( SPI_MODE3, SLAVE_ENABLE1, 2 );

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_PMCR | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rPMCR = 0x%02X", u8Result );
#endif

	// サンプリング周波数を100Hzにする
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_RATE_1 | MC3630_WRITE);
	vSPIWrite(0x08);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_RATE_1 | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rRATE1 = 0x%02X", u8Result );
#endif

	// レンジを16g、12bitの分解能にする
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_RANGE_C | MC3630_WRITE);
	vSPIWrite(0x30|0x04);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_RANGE_C | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rRANGE_C = 0x%02X", u8Result );
#endif

	// 10サンプルのFIFOを使用する
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_FIFO_C | MC3630_WRITE);
	vSPIWrite(0x40|0x0A);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_FIFO_C | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rFIFO_C = 0x%02X", u8Result );
#endif

	// 新しいデータが取れたら割り込み + 割り込みピンをプッシュプルに変える
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_INTR_C | MC3630_WRITE);
	vSPIWrite(0x41);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_INTR_C | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rINTR_C = 0x%02X", u8Result );
#endif

	// 読み書きするときのレジスタ指定のところで加速度センサのステータスを返すようにする
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_FREG_2 | MC3630_WRITE);
	vSPIWrite(0x04);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_FREG_2 | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rFREG_2 = 0x%02X", u8Result );
#endif

	// 連続で測定する。
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_MODE_C | MC3630_WRITE);
	vSPIWrite(0x05);
	vSPIStop();

#ifdef DEBUG
	vSPIChipSelect(CS_DIO19);
	vSPIWrite(MC3630_MODE_C | MC3630_READ);
	vSPIWrite(0x00);
	u8Result = u8SPIRead();
	vSPIStop();
	vfPrintf(&sSerStream, "\n\rMODE_C = 0x%02X", u8Result );
#endif

	return bOk;
}

/****************************************************************************
 *
 * NAME: vHTSstartReadTemp
 *
 * DESCRIPTION:
 *
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
//	Samp_I2Cの名残
PUBLIC bool_t bMC3630startRead()
{
	//	通信できたかどうかを知るすべがないのでそのままTRUEを返す
	bool_t bOk = TRUE;

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16MC3630readResult
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 * NOTES:
 *
 ****************************************************************************/
//	各軸の加速度を読みに行く
PUBLIC uint8 u8MC3630readResult( int16** au16Accel )
{
	uint8	au8data[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	uint8	i;
	uint8	u8status;
	uint8	u8num = 0;

	//	各軸の読み込み
	while(1){
		vSPIChipSelect(CS_DIO19);
	   	vSPIWrite(MC3630_READ|MC3630_XOUT_LSB);
		u8status = u8SPIRead();
		if(u8status&0x10){
			vSPIStop();
			break;
		}

		for(i=0;i<6;i++){
			vSPIWrite(0x00);
			au8data[i] = u8SPIRead();
		}
		vSPIStop();

		au16Accel[0][u8num] = (au8data[1]<<8|au8data[0])*8;
		au16Accel[1][u8num] = (au8data[3]<<8|au8data[2])*8;
		au16Accel[2][u8num] = (au8data[5]<<8|au8data[4])*8;
		u8num++;
	}
	vMC3630ClearInterrupReg();
    return u8num;
}

PUBLIC uint8 u8MC3630InterruptStatus()
{
	vSPIChipSelect(CS_DIO19);
	vSPIWrite( MC3630_STATUS_2|MC3630_READ );
	uint8 data = u8SPIRead();
	vSPIWrite( 0x00 );
	data = u8SPIRead();
	vSPIStop();

	return data;
}

PUBLIC void vMC3630ClearInterrupReg()
{
	vSPIChipSelect(CS_DIO19);
	vSPIWrite( MC3630_STATUS_2|MC3630_WRITE );
	u8SPIRead();
	vSPIWrite( 0x00 );
	vSPIStop();
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
