/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef  MC3630_INCLUDED
#define  MC3630_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
// Sensor Registry Address
#define MC3630_EXT_STAT_1       (0x00)
#define MC3630_EXT_STAT_2       (0x01)
#define MC3630_XOUT_LSB         (0x02)
#define MC3630_XOUT_MSB         (0x03)
#define MC3630_YOUT_LSB         (0x04)
#define MC3630_YOUT_MSB         (0x05)
#define MC3630_ZOUT_LSB         (0x06)
#define MC3630_ZOUT_MSB         (0x07)
#define MC3630_STATUS_1         (0x08)
#define MC3630_STATUS_2         (0x09)
#define MC3630_FREG_1           (0x0D)
#define MC3630_FREG_2           (0x0E)
#define MC3630_INIT_1           (0x0F)
#define MC3630_MODE_C           (0x10)
#define MC3630_RATE_1           (0x11)
#define MC3630_SNIFF_C          (0x12)
#define MC3630_SNIFFTH_C        (0x13)
#define MC3630_IO_C             (0x14)
#define MC3630_RANGE_C          (0x15)
#define MC3630_FIFO_C           (0x16)
#define MC3630_INTR_C           (0x17)
#define MC3630_CHIP_ID          (0x18)
#define MC3630_INIT_3           (0x1A)
#define MC3630_PMCR             (0x1C)
#define MC3630_DMX              (0x20)
#define MC3630_DMY              (0x21)
#define MC3630_DMZ              (0x22)
#define MC3630_RESET            (0x24)
#define MC3630_INIT_2           (0x28)
#define MC3630_TRIGC            (0x29)
#define MC3630_XOFFL            (0x2A)
#define MC3630_XOFFH            (0x2B)
#define MC3630_YOFFL            (0x2C)
#define MC3630_YOFFH            (0x2D)
#define MC3630_ZOFFL            (0x2E)
#define MC3630_ZOFFH            (0x2F)
#define MC3630_XGAIN            (0x30)
#define MC3630_YGAIN            (0x31)
#define MC3630_ZGAIN            (0x32)

#define MC3630_WRITE            (0x40)
#define MC3630_READ             (0xC0)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
PUBLIC bool_t bMC3630reset();
PUBLIC bool_t bMC3630startRead();
PUBLIC uint8 u8MC3630readResult( int16** au16Accel );
PUBLIC uint8 u8MC3630InterruptStatus();
PUBLIC void vMC3630ClearInterrupReg();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* MC3630_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

