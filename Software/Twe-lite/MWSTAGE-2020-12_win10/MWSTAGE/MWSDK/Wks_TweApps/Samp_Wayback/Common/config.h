/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef  CONFIG_H_INCLUDED
#define  CONFIG_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include <AppHardwareApi.h>
#include "utils.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Serial Configuration */
#define UART_BAUD   		115200
#define UART_PARITY_ENABLE	E_AHI_UART_PARITY_DISABLE
#define UART_PARITY_TYPE 	E_AHI_UART_ODD_PARITY // if enabled
#define UART_BITLEN			E_AHI_UART_WORD_LEN_8
#define UART_STOPBITS 		E_AHI_UART_1_STOP_BIT

/* Specify which serial port to use when outputting debug information */
#define UART_PORT			E_AHI_UART_0

/* Specify the PAN ID and CHANNEL to be used by tags, readers and gateway */
#define APP_ID              0x67726304
#define APP_NAME            "Samp_Wayback"
#define CHANNEL             12
//#define CHMASK              ((1UL << CHANNEL) | (1UL << (CHANNEL+5)) | (1UL << (CHANNEL+10)))
#define CHMASK              (1UL << CHANNEL)

/**
 * メッセージプールの送信周期
 */
#define PARENT_MSGPOOL_TX_DUR_s 10

/**
 * AES暗号化を使用する (0.7)
 */
#define USE_AES

/*************************************************************************/
/***        TARGET PCB                                                 ***/
/*************************************************************************/
#define TWX0003 1 // TWX0003 タグ基板
#define TWX0009 2 // TWX0009 タグ基板
#define SNSARM001 99 // 評価キットの基板(SENSOR ARMOUR001)

#define TAGPCB SNSARM001 // 上記より選択すること
// #define TAGPCB TWX0003 // 上記より選択すること

#ifndef TAGPCB
#error "TAGPCB is not defined."
#elif (TAGPCB == SNSARM001)
#define DIO_BUTTON (PORT_KIT_SW1)
#define DIO_VOLTAGE_CHECKER (0)
#elif (TAGPCB == TWX0003)
#warning "*** BUILD WITH TWX0003 ***"
#define DIO_BUTTON (8)
#define DIO_VOLTAGE_CHECKER (0)
#elif (TAGPCB == TWX0009)
#warning "*** BUILD WITH TWX0009 ***"
#define DIO_BUTTON (8)
#define DIO_VOLTAGE_CHECKER (20)
#define DIO_ANALOGUE_SECTION (19)
#define DIO_EXTERNAL_CLOCK_ENABLE (11)
#define DIO_EXTERNAL_CLOCK_INPUT (9)
#define EXTERNAL_CLOCK_CALIB 9666 // 10000*32000/32768
#else
#define TAGBCB SNSARM001
#define DIO_BUTTON (9)
#define DIO_VOLTAGE_CHECKER (0)
#error "TAGPCB is not defined..."
#endif

//#define PORT_INPUT_MASK (DIO_BUTTON)
#define PORT_INPUT_MASK (1UL << 2 | 1UL << 7 | 1UL << 8 | 1UL << 9)
	// DIO2, 7(UART0 RX), 8, 9

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* CONFIG_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
