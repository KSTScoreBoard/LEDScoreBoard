/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/*
 * event.h
 *
 *  Created on: 2013/01/10
 *      Author: seigo13
 */

#ifndef APP_EVENT_H_
#define APP_EVENT_H_

#include "ToCoNet_event.h"

typedef enum
{
	E_EVENT_APP_BASE = ToCoNet_EVENT_APP_BASE,
	E_EVENT_APP_START_NWK,
	E_EVENT_APP_GET_IC_INFO
} teEventApp;

// STATE MACHINE
typedef enum
{
	E_STATE_APP_BASE = ToCoNet_STATE_APP_BASE,
	E_STATE_APP_WAIT_NW_START,
	E_STATE_APP_WAIT_NW_START_BOOT,
	E_STATE_APP_WAIT_TX,
	E_STATE_APP_WAIT_RX,
	E_STATE_APP_IO_WAIT_RX,
	E_STATE_APP_IO_RECV_ERROR,
	E_STATE_APP_SLEEP,
	E_STATE_APP_PREUDO_SLEEP
} teStateApp;

#endif /* EVENT_H_ */
