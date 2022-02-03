#pragma once
#include <TWELITE>

/*** Config part */
// application ID
const uint32_t APP_ID = 0x1234abcd;

// channel
const uint8_t CHANNEL = 13;

// id
extern uint8_t u8ID;

// application use
const uint8_t FOURCHARS[] = "DTR1";

// buffer size
typedef uint16_t buff_type;
const int N_BUFFER = 64 / sizeof(buff_type);

// count padding count if _dio_count is not 2^N.
int calc_dio_count_pad(int _dio_count);

/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */