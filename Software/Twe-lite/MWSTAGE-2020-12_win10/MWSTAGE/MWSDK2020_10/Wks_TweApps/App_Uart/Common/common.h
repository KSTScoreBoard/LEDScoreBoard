/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef COMMON_H_
#define COMMON_H_

#include <serial.h>
#include <fprintf.h>

#include "config.h"
#include "flash.h"

/*
 * IOポートの定義
 */
#define PORT_OUT1 18
#define PORT_OUT2 19
#define PORT_OUT3 4
#define PORT_OUT4 9

#define PORT_INPUT1 12
#define PORT_INPUT2 13
#define PORT_INPUT3 11
#define PORT_INPUT4 16

#define PORT_CONF1 10
#define PORT_CONF2 2
#define PORT_CONF3 3

#define PORT_BAUD 17

#define PORT_CONF_EX1 0
#define PORT_CONF_EX2 1

#define PORT_OUT_MASK ((1UL << PORT_OUT1) | (1UL << PORT_OUT2) | (1UL << PORT_OUT3) | (1UL << PORT_OUT4))
#define PORT_INPUT_MASK ((1UL << PORT_INPUT1) | (1UL << PORT_INPUT2) | (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4))

// UART ポートの割り当て
#define PORT_UART_RTS 5
#define PORT_UART_TX 6
#define PORT_UART_RX 7

#define PORT_UART_TX_SUB 14
#define PORT_UART_RX_SUB 15

#ifdef USE_DIO_SLEEP
// スリープ起床のためのポート
#define PORT_SLEEP PORT_CONF3
#define PORT_SLEEP_WAKE_MASK ((1UL << PORT_SLEEP) | (1UL << PORT_UART_RX)) // スリープピンまたは UART0 RX
#endif

// マップテーブル
extern const uint8 au8PortTbl_DOut[4]; //!< IO番号(出力)のテーブル
extern const uint8 au8PortTbl_DIn[4]; //!< IO番号(入力)のテーブル

/**
 * PORT_CONF1 ～ 2 による定義
 */
typedef enum {
	E_IO_MODE_CHILD = 0,          //!< E_IO_MODE_CHILD
	E_IO_MODE_PARNET,             //!< E_IO_MODE_PARNET
	E_IO_MODE_REPEAT_CHILD,       //!< E_IO_MODE_REPEAT_CHILD
	E_IO_MODE_REPEATER,           //!< E_IO_MODE_REPEATER
} tePortConf2Mode;
extern const uint8 au8IoModeTbl_To_LogicalID[8]; //!< tePortConf2Mode から論理アドレスへの変換

#define LOGICAL_ID_PARENT 0 //!< 親機
#define LOGICAL_ID_CHILDREN 120 //!< 子機
#define LOGICAL_ID_REPEATER 254 //!< 中継機
#define LOGICAL_ID_BROADCAST 255 //!< ブロードキャストアドレス(TODO)
#define LOGICAL_ID_EXTENDED_ADDRESS 0x80 //!< 拡張アドレスを用いた送信を行う

#define IS_LOGICAL_ID_CHILD(s) (s>0 && s<=120) //!< 論理アドレスが子機の場合
#define IS_LOGICAL_ID_PARENT(s) (s == LOGICAL_ID_PARENT) //!< 論理アドレスが親機の場合
#define IS_LOGICAL_ID_REPEATER(s) (s == LOGICAL_ID_REPEATER) //!< 簡易アドレスがリピータの場合
#define IS_LOGICAL_ID_EXTENDED(s) (s == LOGICAL_ID_EXTENDED_ADDRESS) //!< 簡易アドレスが拡張指定

/*
 * シリアルコマンドの定義
 */
#define APP_PROTOCOL_VERSION 0x12 //!< プロトコルバージョン,5bit (0x11 ～ v102, 0x12: v110～)

#define SERCMD_ADDR_TO_MODULE 0xDB //!< Device -> 無線モジュール
#define SERCMD_ADDR_TO_PARENT 0x00 //!< Device
#define SERCMD_ADDR_FR_MODULE 0xDB //!< 無線モジュール -> Device
#define SERCMD_ADDR_FR_PARENT 0x0 //!< ...-> (無線送信) -> Device

#define SERCMD_ADDR_CONV_TO_SHORT_ADDR(c) (c + 0x100) //!< 0..255 の論理番号をショートアドレスに変換する
#define SERCMD_ADDR_CONV_FR_SHORT_ADDR(c) (c - 0x100) //!< ショートアドレスを論理番号に変換する

#define SERCMD_ADDR_CONV_TO_SHORT_ADDR_CHILD_IN_PAIR(c) (c + 0x200 + 1) //!< 透過モードの子機のアドレス
#define SERCMD_ADDR_CONV_TO_SHORT_ADDR_PARENT_IN_PAIR(c) (c + 0x300 + 1) //!< 透過モードの親機のアドレス
#define SERCMD_ADDR_CONV_FR_SHORT_ADDR_CHILD_IN_PAIR(c) (c - 0x200 - 1) //!< 透過モードの子機のアドレス
#define SERCMD_ADDR_CONV_FR_SHORT_ADDR_PARENT_IN_PAIR(c) (c - 0x300 - 1) //!< 透過モードの親機のアドレス

/**
 * コマンド群
 */
#define SERCMD_ID_ACK 0xF0
#define SERCMD_ID_GET_MODULE_ADDRESS 0xF1
#define SERCMD_ID_SET_MODULE_SETTING 0xF2
#define SERCMD_ID_GET_MODULE_SETTING 0xF3
#define SERCMD_ID_CLEAR_SAVE_CONTENT 0xF4


#define SERCMD_ID_DO_FACTORY_DEFAULT 0xFD
#define SERCMD_ID_SAVE_AND_RESET 0xFE
#define SERCMD_ID_RESET 0xFF

#define SERCMD_ID_MODULE_CONTROL 0xF8

#define SERCMD_ID_MODULE_CONTROL_RELEASE_SILENT 0x10
#define SERCMD_ID_MODULE_CONTROL_INFORM_SILENT 0x11

/**
 * 拡張パケット送信
 *
 * - 書式
 *   - OCTET    １バイトアドレス
 *   - OCTET    コマンド 0xA0
 *   - OCTET    要求番号
 *   - OCTET    送信オプション
 *      - ANY   オプションの引数
 *   - OCTET N  ペイロード
 *
 *   - OCTET    拡張アドレス LOGICAL_ID_EXTENDED_ADDRESS (0x80)
 *   - OCTET    コマンド 0xA0
 *   - OCTET    要求番号
 *   - DWORD    送信先アドレス (TOCONET形式)
 *   - OCTET    送信オプション
 *      - ANY   オプションの引数
 *   - OCTET N  ペイロード
 *
 */
#define SERCMD_ID_TRANSMIT_EX 0xA0

/**
 * 拡張パケット送信の応答
 * - 書式
 *   - OCTET 要求番号
 *   - OCTET 成功フラグ (0x0: 失敗, 0x1: 成功)
 */
#define SERCMD_ID_TRANSMIT_EX_RESP 0xA1

/**
 * MAC ACK を用いた送信を行う。
 *
 * - 802.15.4 のパケットのあて先は、ショートアドレスまたは拡張アドレスとなり ACK 要求を行う。
 * - １ホップ中継機は利用できない。
 */
#define TRANSMIT_EX_OPT_SET_MAC_ACK 0x01

/**
 * アプリケーション再送の設定を行う。設定しなければ、デフォルト値が選択される。
 *
 * - 続いて OCTET の再送回数を指定する。
 *   - 0x00 ～ 0x0F: 0 から 16 回の再送を行う。再送成功を持って終了する。
 *       MAC_ACK を用いない場合は、１回送信すれば終了する事になる。
 *   - 0x81 ～ 0x8F: 1 から 16 回の再送を行う。回数分送信を試みる。
 *       MAC_ACK を用いない場合、この設定を使用する。
 */
#define TRANSMIT_EX_OPT_SET_APP_RETRY 0x02

/**
 * 最初の送信に対する遅延の最小値。
 *
 * - 続いて BE_WORD の遅延時間[ms]を設定する。
 * - 遅延する事が有る。
 */
#define TRANSMIT_EX_OPT_SET_DELAY_MIN_ms 0x03

/**
 * 最初の送信に対する遅延の最大値。乱数でタイミングが決定される。
 * - 続いて BE_WORD の遅延時間[ms]を設定する。
 * - 遅延する事が有る。
 */
#define TRANSMIT_EX_OPT_SET_DELAY_MAX_ms 0x04

/**
 * 再送間隔。
 * - 続いて BE_WORD の遅延時間[ms]を設定する。
 * - パケット分割が行われる場合は、最小設定値以下にはならない。
 * - 間隔は遅延する事が有る。
 */
#define TRANSMIT_EX_OPT_SET_RETRY_DUR_ms 0x05

/**
 *  送信要求が完了する前に、次の送信要求を受け付ける。
 *
 *  例えば再送の遅延を大きくして複数のメッセージを併行的に送信する場合、最初の要求が完全に完了するまで次の要求を受け付けないが、
 *  本オプションを設定する事で、次の要求が受け付けられる。ただし、
 *
 *  - １パケット分に収まるデータサイズでないといけない。
 *  - 同時並行数は５。送信順は保証しない。
 */
#define TRANSMIT_EX_OPT_SET_PARALLEL_TRANSMIT 0x06

/**
 * 応答メッセージを出力しない
 */
#define TRANSMIT_EX_OPT_SET_NO_RESPONSE 0x07

/**
 * 送信後スリープする
 *
 * 但しスリープが実行されるのは以下の条件
 * - PARALLEL TRANSMIT が有効になっていない事
 * - 遅延によるタイムアウトが発生しない事
 */
#define TRANSMIT_EX_OPT_SET_SLEEP_AFTER_TRANSMIT 0x08

/**
 *  オプション系列の終端子
 */
#define TRANSMIT_EX_OPT_TERM 0xFF

/**
 * チャットモードのコマンド番号
 */
#define SERCMD_ID_CHAT_TEXT 0xC0

/*
 * 出力マクロ
 */
#define V_PRINT(...) if(sSerCmd_P3.bverbose) vfPrintf(&sSerStream,__VA_ARGS__) //!< VERBOSE モード時の printf 出力
#define S_PRINT(...) if(!sSerCmd_P3.bverbose) vfPrintf(&sSerStream,__VA_ARGS__) //!< 非VERBOSE モード時の printf 出力
#define V_PUTCHAR(c) if(sSerCmd_P3.bverbose) sSerStream.bPutChar(sSerStream.u8Device, c)  //!< VERBOSE モード時の putchar 出力
#define S_PUTCHAR(c) if(!sSerCmd_P3.bverbose) sSerStream.bPutChar(sSerStream.u8Device, c) //!< 非VERBOSE モード時の putchar 出力
#ifdef DEBUG_OUTPUT
#define DBGOUT(lv, ...) if(sAppData.u8DebugLevel >= lv) vfPrintf(&sSerStream, __VA_ARGS__) //!< デバッグ出力
#else
#define DBGOUT(lv, ...) //!< デバッグ出力
#endif

/*
 * ビルド時の sizeof() チェックを行うマクロ
 *   BUILD_BUG_ON(sizeof(myStruct)>100);
 *   と書いておいて、これが成立するとコンパイル時エラーになる
 */
#define BUILD_BUG_ON(cond) ((void)sizeof(char[1-2*!!(cond)]))

/*
 * XMODEM 設定データ出力機能
 */
#define ASC_SOH 0x01
#define ASC_EOT 0x04
#define ASC_ACK 0x06
#define ASC_CR  0x0D
#define ASC_LF  0x0A
#define ASC_NAK 0x15
#define ASC_SUB 0x1A
#define ASC_CTRL_Z ASC_SUB
#define ASC_ESC 0x1B
#define XMODEM_BLOCK_SIZE 128


/**
 * シリアルポートのバッファ数を取得する
 * @param u8port
 * @return
 */
uint16 SERIAL_u16RxQueueCount(uint8 u8port);

/**
 * シリアルポートのバッファ数を取得する
 * @param u8port
 * @return
 */
uint16 SERIAL_u16TxQueueCount(uint8 u8port);

/**
 * UART0,1 両方にバイトを出力する
 * @param u8SerialPort 値は反映されない
 * @param u8Chr 出力したいバイト
 * @return TRUEのみ
 */
bool_t   SERIAL_bTxCharDuo(uint8 u8SerialPort, uint8 u8Chr);

/**
 * 文字列を区切り文字で分割する。pstr は NUL 文字で分割される。
 *
 * @param pstr 分割したい文字列
 * @param auptr 区切り文字のポインタ
 * @param u8max_entry 最大の区切り数
 * @return 分割数
 */
uint8 u8StrSplitTokens(uint8 *pstr, uint8 *auptr[], uint8 u8max_entry);

#endif /* COMMON_H_ */
