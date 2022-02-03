/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "ToCoNet.h"

#include "config.h"
#include "utils.h"

#include "common.h"

/**
 * DI のポート番号のテーブル
 */
const uint8 au8PortTbl_DIn[4] = {
	PORT_INPUT1,
	PORT_INPUT2,
	PORT_INPUT3,
	PORT_INPUT4
};

/**
 * DO のポート番号のテーブル
 */
const uint8 au8PortTbl_DOut[4] = {
	PORT_OUT1,
	PORT_OUT2,
	PORT_OUT3,
	PORT_OUT4
};

/**
 * MODE設定ビットからデフォルト割り当てされる論理ＩＤテーブル
 */
const uint8 au8IoModeTbl_To_LogicalID[8] = {
	120, // CHILD
	0,   // PARENT
	120, // CHILD
	254 // Dedicated ROUTER
};


/**
 * シリアルポートの構造体
 *   (serial.c の内部構造体でアクセスできないのでここで定義)
 */
typedef struct
{
    tsQueue sRxQueue; 				/** RX Queue */
    tsQueue sTxQueue; 				/** TX Queue */
    uint16 u16AHI_UART_RTS_LOW;  	/** RTS Low Mark */
    uint16 u16AHI_UART_RTS_HIGH; 	/** RTS High Mark */
} tsSerialPort_redef;
extern tsSerialPort_redef asSerialPorts[];

/**
 * UARTのFIFOキュー(RX)にあるカウント数を取得する
 */
uint16 SERIAL_u16RxQueueCount(uint8 u8port) {
	return QUEUE__u16Count(&asSerialPorts[u8port].sRxQueue);
}

/**
 * UARTのFIFOキュー(TX)にあるカウント数を取得する
 */
uint16 SERIAL_u16TxQueueCount(uint8 u8port) {
	return QUEUE__u16Count(&asSerialPorts[u8port].sTxQueue);
}

/**
 * シリアルの出力を UART0, 1 共に出力
 */
bool_t   SERIAL_bTxCharDuo(uint8 u8SerialPort, uint8 u8Chr) {
	SERIAL_bTxChar(UART_PORT_MASTER, u8Chr);
	SERIAL_bTxChar(UART_PORT_SLAVE, u8Chr);
	return TRUE;
}

/**
 * 文字列を区切り文字で分割する。pstr は NUL 文字で分割される。
 */
uint8 u8StrSplitTokens(uint8 *pstr, uint8 **auptr, uint8 u8max_entry) {
	uint8 u8num = 0;

	uint8 *p = pstr;
	if (pstr == NULL || *p == 0) {
		return 0;
	} else {

		auptr[0] = pstr;
		u8num = 1;

		while (*p) {
			if (*p == ',') {
				*p = 0;
				auptr[u8num] = p + 1;
				u8num++;
				if (u8num >= u8max_entry) {
					break;
				}
			}

			p++;
		}
	}

	return u8num;
}
