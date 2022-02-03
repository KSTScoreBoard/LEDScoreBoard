/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * common.c
 *
 *  Created on: 2013/01/23
 *      Author: seigo13
 */

#include <jendefs.h>
#include <string.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "common.h"

#include "utils.h"
#include "serialInputMgr.h"

// Serial options
#include "serial.h"
#include "fprintf.h"

// ToCoNet Header
#include "ToCoNet.h"

/**
 * Sleep の DIO wakeup 用
 */
const uint32 u32DioPortWakeUp = PORT_INPUT_MASK;


/**
 * 暗号化カギ
 */
const uint8 au8EncKey[] = {
		0x00, 0x01, 0x02, 0x03,
		0xF0, 0xE1, 0xD2, 0xC3,
		0x10, 0x20, 0x30, 0x40,
		0x1F, 0x2E, 0x3D, 0x4C
};

/**
 * ToCoNet のネットワーク情報を UART に出力する
 *
 * @param psSerStream 出力先
 * @param pc ネットワークコンテキスト
 *
 */
void vDispInfo(tsFILE *psSerStream, tsToCoNet_NwkLyTr_Context *pc) {
	if (pc) {
		vfPrintf(psSerStream, LB "* Info: la=%d ty=%d ro=%02x st=%02x",
				pc->sInfo.u8Layer, pc->sInfo.u8NwkTypeId, pc->sInfo.u8Role, pc->sInfo.u8State);
		vfPrintf(psSerStream, LB "* Parent: %08x", pc->u32AddrHigherLayer);
		vfPrintf(psSerStream, LB "* LostParent: %d", pc->u8Ct_LostParent);
		vfPrintf(psSerStream, LB "* SecRescan: %d, SecRelocate: %d", pc->u8Ct_Second_To_Rescan, pc->u8Ct_Second_To_Relocate);
	}
}

/**
 *
 * 親機に対してテスト送信する
 *
 * @param pNwk ネットワークコンテキスト
 * @param pu8Data ペイロード
 * @param u8Len ペイロード長
 *
 */
bool_t bTransmitToParent(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len) {
	static uint8 u8Seq;
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	sTx.u32DstAddr = TOCONET_NWK_ADDR_PARENT;
	sTx.u32SrcAddr = ToCoNet_u32GetSerial(); // Transmit using Long address
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_DATA; // data packet.

	sTx.u8Seq = u8Seq++;
	sTx.u8CbId = sTx.u8Seq;

	sTx.u8Retry = 3; // one application retry
	sTx.u16RetryDur = 33; // retry every 32ms

	if (u8Len == 0) {
		// assume null terminated string.
		u8Len = strlen((char*)pu8Data);
	}

	memcpy(sTx.auData, pu8Data, u8Len);
	sTx.u8Len = u8Len;

	return ToCoNet_Nwk_bTx(pNwk, &sTx);
}


/** @ingroup MASTER
 * スリープの実行
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep) {
	// print message.
	SERIAL_vFlush(UART_PORT);
	vAHI_UartDisable(UART_PORT); // UART を解除してから(このコードは入っていなくても動作は同じ)

	// stop interrupt source, if interrupt source is still running.
	;

	// set UART Rx port as interrupt source
	vAHI_DioSetDirection(PORT_INPUT_MASK, 0); // set as input

	(void)u32AHI_DioInterruptStatus(); // clear interrupt register
	vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
	// vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
	vAHI_DioWakeEdge(PORT_INPUT_MASK, 0); // 割り込みエッジ（立上がりに設定）
	// vAHI_DioWakeEnable(0, PORT_INPUT_MASK); // DISABLE DIO WAKE SOURCE

	// wake up using wakeup timer as well.
	ToCoNet_vSleep(E_AHI_WAKE_TIMER_0, u32SleepDur_ms, bPeriodic, bDeep); // PERIODIC RAM OFF SLEEP USING WK0
}
