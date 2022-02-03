/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#ifndef COMMON_H_
#define COMMON_H_

#include <serial.h>
#include <fprintf.h>

#include "flash.h"
#include "serialInputMgr.h"

#include "config.h"

/** @ingroup MASTER
 * 最大のIO数入力
 */
extern uint8 u8_PORT_INPUT_COUNT;
/** @ingroup MASTER
 * 最大のIO数出力
 */
extern uint8 u8_PORT_OUTPUT_COUNT;

#define MAX_IO_TBL 16

/** @ingroup MASTER
 * 使用する無線チャネル数の最大値 (複数設定すると Channel Agility を利用する)
 */
#define MAX_CHANNELS 3

/*
 * Version 番号
 */
#define VERSION_U32 ((VERSION_MAIN << 16) | (VERSION_SUB << 8) | (VERSION_VAR))

/*
 * IOポートの定義
 * TWE-Lite DIP (TWELITE の標準構成)
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

#define PORT_I2C_CLK 14
#define PORT_I2C_DAT 15

#define PORT_EI1 0 // AI2
#define PORT_EI2 1 // AI4
#define PORT_EO1 5 // PWM1
#define PORT_EO2 8 // PWM4

#define PORT_OUT_MASK ((1UL << PORT_OUT1) | (1UL << PORT_OUT2) | (1UL << PORT_OUT3) | (1UL << PORT_OUT4) | \
					   (1UL << PORT_INPUT1) | (1UL << PORT_INPUT2) | (1UL << PORT_INPUT3) | (1UL << PORT_INPUT4) | \
					   (1UL << PORT_I2C_CLK) | (1UL << PORT_I2C_DAT) | (1UL << PORT_EO1) | (1UL << PORT_EO2))
#define PORT_INPUT_MASK PORT_OUT_MASK

// IOテーブル
#define MAX_IOTBL_SETS 4 //!<  @ingroup MASTER IO設定テーブルの数
extern uint8 au8PortTbl_DIn[MAX_IO_TBL];
extern uint8 u8_PORT_INPUT_COUNT;
extern uint32 u32_PORT_INPUT_MASK;
extern uint8 au8PortTbl_DOut[MAX_IO_TBL];
extern uint8 u8_PORT_OUTPUT_COUNT;
extern uint32 u32_PORT_OUTPUT_MASK;

bool_t bPortTblInit(uint8 u8tbl, bool_t bParent);

/**
 * PORT_CONF1 ～ 3 による定義
 */
typedef enum {
	E_IO_MODE_CHILD = 0,          //!< E_IO_MODE_CHILD
	E_IO_MODE_PARNET,             //!< E_IO_MODE_PARNET
	E_IO_MODE_ROUTER,             //!< E_IO_MODE_ROUTER
	E_IO_MODE_CHILD_CONT_TX,      //!< E_IO_MODE_CHILD_CONT_TX
	E_IO_MODE_CHILD_SLP_1SEC,     //!< E_IO_MODE_CHILD_SLP_1SEC
	E_IO_MODE_CHILD_SLP_10SEC = 7,//!< E_IO_MODE_CHILD_SLP_10SEC
} tePortConf2Mode;
extern const uint8 au8IoModeTbl_To_LogicalID[8]; //!< tePortConf2Mode から論理アドレスへの変換

#define LOGICAL_ID_PARENT (0)
#define LOGICAL_ID_CHILDREN (120)
#define LOGICAL_ID_PAIRING (240)
#define LOGICAL_ID_REPEATER (254)
#define LOGICAL_ID_BROADCAST (255)

#define IS_LOGICAL_ID_CHILD(s) (s>0 && s<128) //!< 論理アドレスが子機の場合 (Parent=0, Router=254)
#define IS_LOGICAL_ID_PARENT(s) (s == 0) //!< 論理アドレスが親機の場合
#define IS_LOGICAL_ID_REPEATER(s) (s == 254) //!< 論理アドレスがリピータの場合

/**
 * チャネルマスクのプリセット
 */
extern const uint32 au32ChMask_Preset[];

/*
 * シリアルコマンドの定義
 */
#define APP_PROTOCOL_VERSION 0x02 //!< プロトコルバージョン (v2 は 1.2 以降)

#define SERCMD_ADDR_TO_MODULE 0xDB //!< Device -> 無線モジュール
#define SERCMD_ADDR_TO_PARENT 0x00 //!< Device
#define SERCMD_ADDR_FR_MODULE 0xDB //!< 無線モジュール -> Device
#define SERCMD_ADDR_FR_PARENT 0x0 //!< ...-> (無線送信) -> Device

#define SERCMD_ADDR_CONV_TO_SHORT_ADDR(c) (c + 0x100) //!< 0..255 の論理番号をショートアドレスに変換する
#define SERCMD_ADDR_CONV_FR_SHORT_ADDR(c) (c - 0x100) //!< ショートアドレスを論理番号に変換する

#define SERCMD_ID_ACK 0xF0

#define SERCMD_ID_REQUEST_IO_DATA 0x80
#define SERCMD_ID_INFORM_IO_DATA 0x81

#define SERCMD_ID_I2C_COMMAND 0x88
#define SERCMD_ID_I2C_COMMAND_RESP 0x89

#define SERCMD_ID_GET_MODULE_ADDRESS 0x90
#define SERCMD_ID_INFORM_MODULE_ADDRESS 0x91
#define SERCMD_ID_GET_NETWORK_CONFIG 0x92
#define SERCMD_ID_INFORM_NETWORK_CONFIG 0x93
#define SERCMD_ID_SET_NETWORK_CONFIG 0x94

#define SERCMD_ID_SERCMD_EX_SIMPLE 0xA4

// Packet CMD IDs
#define TOCONET_PACKET_CMD_APP_USER_IO_DATA (TOCONET_PACKET_CMD_APP_USER+0) //!< IO の入出力データ
#define TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT (TOCONET_PACKET_CMD_APP_USER+1) //!< IO の入出力データ
#define TOCONET_PACKET_CMD_APP_USER_SERIAL_MSG (TOCONET_PACKET_CMD_APP_USER+2) //!< IO の入出力データ

/* Modbus ASCII output functions */
void vModbOut_AckNack(tsFILE *pSer, bool_t bAck);
void vModbOut_MySerial(tsFILE *pSer);

bool_t bModbIn_Config(uint8 *p,  tsFlashApp *pConfig);
void vModbOut_Config(tsFILE *pSer, tsFlashApp *pConfig);

/*
 * 出力マクロ
 */
#define V_PRINT(...) if(sSerCmdIn.bverbose) vfPrintf(&sSerStream,__VA_ARGS__) //!< VERBOSE モード時の printf 出力
#define S_PRINT(...) if(!sSerCmdIn.bverbose) vfPrintf(&sSerStream,__VA_ARGS__) //!< 非VERBOSE モード時の printf 出力
#define V_PUTCHAR(c) if(sSerCmdIn.bverbose) sSerStream.bPutChar(sSerStream.u8Device, c)  //!< VERBOSE モード時の putchar 出力
#define S_PUTCHAR(c) if(!sSerCmdIn.bverbose) sSerStream.bPutChar(sSerStream.u8Device, c) //!< 非VERBOSE モード時の putchar 出力
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

#endif /* COMMON_H_ */
