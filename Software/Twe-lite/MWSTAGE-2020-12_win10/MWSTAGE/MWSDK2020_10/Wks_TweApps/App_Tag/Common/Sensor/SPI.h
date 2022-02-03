/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - 2012 all rights reserved.
 *
 * Condition to use:
 *   - The full or part of source code is limited to use for TWE (TOCOS
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from TOCOS.
 *
 ****************************************************************************/

#ifndef  SPI_INCLUDED
#define  SPI_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
// SPI Mode
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

//	有効にするポート数の指定
#define SLAVE_ENABLE1		(0)			//	DIO19を用いる
#define SLAVE_ENABLE2		(1)			//	DIO19とDIO0を用いる
#define SLAVE_ENABLE3		(2)			//	DIO19,0,1を用いる

//	ChipSelect
#define CS_DIO19			(0x01)		//	DIO19に接続したものを使う
#define CS_DIO0				(0x02)		//	DIO0に接続したものを使う
#define CS_DIO1				(0x04)		//	DIO1に接続したものを使

// ChipSelectをするマクロ
#define vSPIChipSelect(c) vAHI_SpiSelect(c)
#define vSPIStop() vAHI_SpiStop()

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/


/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
PUBLIC void vSPIInit( uint8 u8mode, uint8 u8SlaveEnable, uint8 u8ClockDivider );
PUBLIC void vSPIWrite( uint8 u8Com );
PUBLIC uint8 u8SPIRead( void );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* LIS3DH_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

