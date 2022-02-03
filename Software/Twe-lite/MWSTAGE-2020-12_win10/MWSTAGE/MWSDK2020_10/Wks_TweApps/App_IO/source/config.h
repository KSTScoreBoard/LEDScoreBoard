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

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* アプリケーション名 */
#define APP_NAME "TWE APP_IO"

/* Serial Configuration */
#define UART_BAUD			115200UL //!< UART のボーレート（デフォルト）
#define UART_BAUD_SAFE		38400UL //!< UART のボーレート（他の設定）
#define UART_PARITY_ENABLE	E_AHI_UART_PARITY_DISABLE //!< パリティは none
#define UART_PARITY_TYPE 	E_AHI_UART_EVEN_PARITY //!< E_AHI_UART_PARITY_ENABLE 時の指定
#define UART_STOPBITS 		E_AHI_UART_1_STOP_BIT //!< ストップビットは１

/* Specify which serial port to use when outputting debug information */
#define UART_PORT_MASTER    E_AHI_UART_0 //!< UARTポートの指定

/* Specify the PAN ID and CHANNEL to be used by tags, readers and gateway */
#define APP_ID              0x67720107 //!< アプリケーションID。同じIDでないと通信しない。

//#define CHANNEL             17
//#define CHMASK              ((1UL << 11) | (1UL << 17) | (1UL << 25))

#define CHANNEL 16 //!< 使用するチャネル
#define CHMASK (1UL << CHANNEL) //!< チャネルマスク（３つまで指定する事が出来る）

#define CHANNEL_1 12
#define CHMASK_1 (1UL << CHANNEL_1) //!< チャネルマスク（３つまで指定する事が出来る）
#define CHANNEL_2 21
#define CHMASK_2 (1UL << CHANNEL_2) //!< チャネルマスク（３つまで指定する事が出来る）
#define CHANNEL_3 25
#define CHMASK_3 (1UL << CHANNEL_3) //!< チャネルマスク（３つまで指定する事が出来る）

// Coordinator specific settings
#define PARENT_ADDR     	0x8001

// スリープ間隔
#define MODE4_SLEEP_DUR_ms 1000UL
#define MODE7_SLEEP_DUR_ms 0UL

/**
 * ボタン押し下げ時連続送信モード
 *
 * - DI=G 検知時に連続送信し、DI=H になった後も１秒間(ON_PRESS_TRANSMIT_KEEP_TX_ms)継続送信する。
 * - 受信側は、500ms(ON_PRESS_TRANSMIT_RESET_ms) 無線電波を受信しなかった場合、DO=H に戻す
 */
#define ON_PRESS_TRANSMIT

/**
 * ボタン押し下げ時連続送信モード時の、無受信時に DO=H に戻すまでの時間[ms]
 */
#define ON_PRESS_TRANSMIT_RESET_ms 500

/**
 * ボタン押し下げ時連続送信モード時の、DI=H に戻った後に継続送信する時間 [ms]
 */
#define ON_PRESS_TRANSMIT_KEEP_TX_ms 1000

/**
 * 低レイテンシモード時で割り込みタイミングから、再度送信する
 */
#define LOW_LATENCY_DELAYED_TRANSMIT_COUNTER 7

/**
 * リモコンモードで長押しがリリースされてからの再送回数
 */
#define REMOTE_MODE_ADDITIONAL_TX_COUNT 3

/**
 * 評価キット 002_L 使用時
 */
#ifdef USE_DEV_KIT_002_L
# warning "USE_DEV_KIT_002_L is defined... Undef this for official release."
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
