/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */


#ifndef COMMON_H_
#define COMMON_H_

#include <serial.h>
#include <fprintf.h>

#include "ToCoNet.h"

void vDispInfo(tsFILE *psSerStream, tsToCoNet_NwkLyTr_Context *pc);

uint32 u32DioPortWakeUp;
void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep);
void vResetWithMsg(tsFILE *psSerStream, string str);

#ifdef ENDDEVICE_INPUT
bool_t bTransmitToParent(tsToCoNet_Nwk_Context *pNwk, uint8 *pu8Data, uint8 u8Len);
bool_t bTransmitToAppTwelite( uint8 *pu8Data, uint8 u8Len );
#endif

bool_t bRegAesKey(uint32 u32seed);

extern const uint8 au8EncKey[];

/*
 * パケット識別子
 */
#define PKT_ID_STANDARD 0x10
#define PKT_ID_LM61 0x11
#define PKT_ID_SHT21 0x31
#define PKT_ID_ADT7410 0x32
#define PKT_ID_MPL115A2 0x33
#define PKT_ID_LIS3DH 0x34
#define PKT_ID_ADXL345 0x35
#define PKT_ID_TSL2561 0x36
#define PKT_ID_L3GD20 0x37
#define PKT_ID_S1105902 0x38
#define PKT_ID_BME280 0x39
#define PKT_ID_SHT31 0x3A
#define PKT_ID_SHTC3 0x3B
#define PKT_ID_IO_TIMER 0x51
#define PKT_ID_MAX31855 0x61
#define PKT_ID_UART 0x81
#define PKT_ID_ADXL345_LOWENERGY 0xA1
#define PKT_ID_MULTISENSOR 0xD1
#define PKT_ID_BUTTON 0xFE

/*
 * 標準ポート定義 (TWE-Lite DIP)
 */
#if defined (USE_MONOSTICK)
// MONOSTICK 用
#warning "IO CONF IS FOR MONOSTICK!"
#define PORT_OUT1 16 // DIO16/18 をスワップ
#define PORT_OUT2 19
#define PORT_OUT3 4
#define PORT_OUT4 9

#define PORT_INPUT1 12
#define PORT_INPUT2 13
#define PORT_INPUT3 11
#define PORT_INPUT4 18 // DIO16/18 をスワップ
#else	//	USE_MONOSTICK
#define PORT_OUT1 18
#define PORT_OUT2 19
#define PORT_OUT3 4
#define PORT_OUT4 9

#define PORT_INPUT1 12
#define PORT_INPUT2 13
#define PORT_INPUT3 11
#define PORT_INPUT4 16
#endif	//	USE_MONOSTICK

#define PORT_SDA 15
#define PORT_SCL 14

#define PORT_CONF1 10
#define PORT_CONF2 2
#define PORT_CONF3 3

#define PORT_BAUD 17
#define PORT_UART0_RX 7


#endif /* COMMON_H_ */
