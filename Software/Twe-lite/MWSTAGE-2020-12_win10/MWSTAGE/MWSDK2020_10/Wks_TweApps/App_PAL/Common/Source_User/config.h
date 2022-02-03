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

#include "common.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Serial Configuration */
#define UART_BAUD   		115200
#define UART_BAUD_SAFE		38400
#define UART_PARITY_ENABLE	E_AHI_UART_PARITY_DISABLE
#define UART_PARITY_TYPE 	E_AHI_UART_ODD_PARITY // if enabled
#define UART_BITLEN			E_AHI_UART_WORD_LEN_8
#define UART_STOPBITS 		E_AHI_UART_1_STOP_BIT

/* Specify which serial port to use when outputting debug information */
#define UART_PORT			E_AHI_UART_0

/* Specify the PAN ID and CHANNEL to be used by tags, readers and gateway */
#define APP_ID				0x67726305

#define APP_NAME            "App_PAL"
#define CHANNEL				15
#define CHMASK              (1UL << CHANNEL)

#define DEFAULT_ENC_KEY     (0xA5A5A5A5)

/**
 * 子機のデフォルトスリープ周期
 */
#define DEFAULT_SLEEP_DUR (60UL)

/*************************************************************************/
/***        TARGET PCB                                                 ***/
/*************************************************************************/
#define LED_ON() vPortSetLo(OUTPUT_LED)
#define LED_OFF() vPortSetHi(OUTPUT_LED)

#define PORT_INPUT_MASK ( 1UL << INPUT_SWSET )

//# define sAppData sAppData_Ed

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
