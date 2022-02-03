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
/* Serial Configuration */
#define UART_BAUD			115200UL //!< UART のボーレート（デフォルト）
#define UART_BAUD_SAFE		38400UL //!< UART のボーレート（他の設定）
#define UART_PARITY_ENABLE	E_AHI_UART_PARITY_DISABLE //!< パリティは none
#define UART_PARITY_TYPE 	E_AHI_UART_EVEN_PARITY //!< E_AHI_UART_PARITY_ENABLE 時の指定
#define UART_STOPBITS 		E_AHI_UART_1_STOP_BIT //!< ストップビットは１

#define UART_BUFFER_RX      4096UL //!< 4KB バッファ(入力側)
#define UART_BUFFER_TX      4096UL //!< 1KB バッファ(出力側)
#define UART_BUFFER_RX_LIMIT_STOP (UART_BUFFER_RX * 7 / 8) //!< 受信不可(RTS=HI)開始条件
#define UART_BUFFER_RX_LIMIT_START (UART_BUFFER_RX * 6 / 8) //!< 受信可(RTS=LO)開始条件 (ヒステリシスを設ける)

/* Specify which serial port to use when outputting debug information */
#define UART_PORT_MASTER    E_AHI_UART_0 //!< UARTポートの指定

#if UART_PORT_MASTER != E_AHI_UART_0
# warning "UART1 is used."
#endif

/* Specify the PAN ID and CHANNEL to be used by tags, readers and gateway */
#define APP_ID              0x67720103 //!< アプリケーションID。同じIDでないと通信しない。
//#define CHANNEL             17
//#define CHMASK              ((1UL << 11) | (1UL << 17) | (1UL << 25))
#define CHANNEL 18 //!< 使用するチャネル
#define CHMASK (1UL << CHANNEL) //!< チャネルマスク（３つまで指定する事が出来る）

// SERIAL BUFFERS
#define SERCMD_SER_PKTLEN 80 //!< シリアルメッセージのデータ部の最大バイト数
#define SERCMD_SER_PKTLEN_MINIMUM (SERCMD_SER_PKTLEN - 0) //!< 確実に１パケットに格納できるサイズ
#define SERCMD_SER_PKTNUM 8 //!< シリアルメッセージの最大送信パケット数
#define SERCMD_MAXPAYLOAD (SERCMD_SER_PKTLEN*SERCMD_SER_PKTNUM) //!< シリアルメッセージのバッファサイズ

// その他の設定
#define USE_MODE_PIN
#define USE_BPS_PIN
#define UART_MODE_DEFAULT 3 //!< 0:Transparent, 1:Ascii format, 2:Binary, 3:Chat

#define DEFAULT_TX_FFFF_COUNT 0x82 //!< デフォルトの再送回数
#define DEFAULT_TX_FFFF_DUR_ms 4 //!< 再送時の間隔
#define DEFAULT_TX_FFFF_DELAY_ON_REPEAT_ms 20 //!< 中継時の遅延

#define NWK_LAYER //!< ネットワーク層を利用する
#undef NWK_LAYER_FORCE //!< デフォルトでネットワーク層を使用する

#define USE_AES //!< AES を利用する
#define USE_DIO_SLEEP //!< IOポート監視スリープを利用する

#define DEFAULT_OPT_BITS 0x00000000UL //!< デフォルトのオプションビット

/* このセットでは、
 *   UART1, 38400bps 8N1 で動作させる
 */
#ifdef CONFIG_000_1
#define CONFIG_CUSTOM
#warning "Custom configuration 000_1"
#undef UART_BAUD
#define UART_BAUD			38400UL //!< UART のボーレート（デフォルト）
#undef UART_BAUD_SAFE
#define UART_BAUD_SAFE		9600UL //!< UART のボーレート（他の設定）
#undef UART_PORT_MASTER
#define UART_PORT_MASTER    E_AHI_UART_1 //!< UARTポートの指定
#undef USE_MODE_PIN
//#undef USE_BPS_PIN
#undef UART_MODE_DEFAULT
#define UART_MODE_DEFAULT 1 //!< 0:Transparent, 1:Ascii format, 2:Binary
#endif

#ifdef CONFIG_000_0
#define CONFIG_CUSTOM
#warning "Custom configuration 000_0"
#undef UART_BAUD
#define UART_BAUD			38400UL //!< UART のボーレート（デフォルト）
#undef UART_BAUD_SAFE
#define UART_BAUD_SAFE		9600UL //!< UART のボーレート（他の設定）
#undef UART_PORT_MASTER
#define UART_PORT_MASTER    E_AHI_UART_0 //!< UARTポートの指定
//#undef USE_MODE_PIN
//#undef USE_BPS_PIN
#undef UART_MODE_DEFAULT
#define UART_MODE_DEFAULT 2 //!< 0:Transparent, 1:Ascii format, 2:Binary
#define NWK_LAYER
#endif

#ifdef CONFIG_001
#define CONFIG_CUSTOM
#warning "Custom configuration 001"
#undef UART_PORT_MASTER
#define UART_PORT_MASTER    E_AHI_UART_0 //!< UARTポートの指定
#undef USE_MODE_PIN
#undef USE_BPS_PIN
#undef UART_MODE_DEFAULT
#define UART_MODE_DEFAULT 1 //!< 0:Transparent, 1:Ascii format, 2:Binary
#endif

#ifdef CONFIG_002
#define CONFIG_CUSTOM
#warning "Custom configuration 002"
#undef UART_BAUD
#define UART_BAUD			38400UL //!< UART のボーレート（デフォルト）"
#define USE_MODE_PIN
#undef USE_BPS_PIN
#undef UART_MODE_DEFAULT
#define UART_MODE_DEFAULT 2 //!< 0:Transparent, 1:Ascii format, 2:Binary
#undef NWK_LAYER
#endif

/*
 * UART1 デフォルト
 */
#ifdef CONFIG_003
#define CONFIG_CUSTOM
#warning "Custom configuration 003"
#undef DEFAULT_OPT_BITS
#define DEFAULT_OPT_BITS 0x60000UL
#endif

#ifdef CONFIG_38400BPS_BINARY
#define CONFIG_CUSTOM
#warning "Custom configuration 003"
#undef UART_BAUD
#define UART_BAUD			38400UL //!< UART のボーレート（デフォルト）
#undef UART_MODE_DEFAULT
#define UART_MODE_DEFAULT 2 //!< 0:Transparent, 1:Ascii format, 2:Binary
#endif

#ifdef CONFIG_NORMAL
#define CONFIG_CUSTOM
#endif

#ifndef CONFIG_CUSTOM
# error "NO_CONFIG_OPTION, SET APP_UART_CONFIG=CONFIG_NORMAL, OR ELSE IN YOUR MAKE PARAMETER "
#endif
/**
 * サブ UART ポートの定義 (MASTER が定義されてから)
 */
#define UART_PORT_SLAVE (1-UART_PORT_MASTER)

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
