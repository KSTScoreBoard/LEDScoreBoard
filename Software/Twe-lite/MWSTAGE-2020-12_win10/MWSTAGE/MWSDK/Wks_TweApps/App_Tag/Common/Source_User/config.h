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
#define APP_TWELITE_ID		0x67720102
#define APP_ID				0x67726305

#define APP_NAME            "App_Tag"
#define APP_TWELITE_CHANNEL	18
#define CHANNEL				15

// ネコッター用設定
#define NEKO_ID				0x67726308
#define NEKO_NAME           "Samp_Nekotter"

// リモート設定用
#define APP_ID_OTA       0x67726405
#define CHANNEL_OTA		25
#define SHORTADDR_OTA	0x0F0F

// デフォルトセンサー
#ifdef LITE2525A
#define DEFAULT_SENSOR		0x35
#elif OTA
#define DEFAULT_SENSOR		0x35
#elif SWING
#define DEFAULT_SENSOR		0xFE
#else
#define DEFAULT_SENSOR		0x10
#endif
//#define CHMASK              ((1UL << CHANNEL) | (1UL << (CHANNEL+5)) | (1UL << (CHANNEL+10)))
#define CHMASK              (1UL << CHANNEL)

#define DEFAULT_ENC_KEY     (0xA5A5A5A5)
/**
 * メッセージプールの送信周期
 */
#define PARENT_MSGPOOL_TX_DUR_s 10

/**
 * 子機のデフォルトスリープ周期
 */
#ifdef LITE2525A
#define DEFAULT_SLEEP_DUR_ms (500UL)
#elif OTA
#define DEFAULT_SLEEP_DUR_ms (500UL)
#else
#define DEFAULT_SLEEP_DUR_ms (5000UL)
#endif
/**
 * 温度センサーの利用
 */
#undef USE_TEMP_INSTDOF_ADC2

/**
 * DIO_SUPERCAP_CONTROL の制御閾値
 *
 * - ADC1 に二次電池・スーパーキャパシタの電圧を 1/2 に分圧して入力
 * - 本定義の電圧以上になると、DIO_SUPERCAP_CONTROL を LO に設定する
 *   (1300なら 2.6V を超えた時点で IO が LO になる)
 * - 外部の電源制御回路の制御用
 */
#define VOLT_SUPERCAP_CONTROL 1300

/*************************************************************************/
/***        TARGET PCB                                                 ***/
/*************************************************************************/
#ifdef SWING
#define DIO_BUTTON (0)						// DI1
#else
#define DIO_BUTTON (PORT_INPUT1)			// DI1
#endif
#define DIO_VOLTAGE_CHECKER (PORT_OUT1)		// DO1: 始動後速やかに LO になる
#define DIO_SUPERCAP_CONTROL (PORT_OUT2)	// DO2: SUPER CAP の電圧が上昇すると LO に設定

#if defined(LITE2525A)
#define LED_ON(c) vPortSetHi(c);
#define LED_OFF(c) vPortSetLo(c);
#else
#define LED_ON(c) vPortSetLo(c);
#define LED_OFF(c) vPortSetHi(c);
#endif

#define LED (5)

#define SETTING_BIT1 8
#define SETTING_BIT2 9
#define SETTING_BIT3 10
#define SETTING_BIT4 16

#define PORT_INPUT_MASK ( 1UL << DIO_BUTTON )
#define PORT_INPUT_SUBMASK ( 1UL << PORT_INPUT2 )
#define PORT_INPUT_MASK_ADXL345 ( (1UL << DIO_BUTTON) | (1UL << PORT_INPUT2) | (1UL <<  PORT_INPUT3))
#define PORT_INPUT_MASK_AIRVOLUME ( (1UL << PORT_INPUT2) | (1UL <<  PORT_INPUT3))
#define PORT_INPUT_MASK_ACL ( (1UL << PORT_INPUT2 ) | (1UL <<  DIO_BUTTON) )

#ifdef PARENT
# define sAppData sAppData_Pa
#endif
#ifdef ROUTER
# define sAppData sAppData_Ro
#endif
#ifdef ENDDEVICE_INPUT
# define sAppData sAppData_Ed
#endif
#ifdef ENDDEVICE_REMOTE
# define sAppData sAppData_Re
#endif

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
