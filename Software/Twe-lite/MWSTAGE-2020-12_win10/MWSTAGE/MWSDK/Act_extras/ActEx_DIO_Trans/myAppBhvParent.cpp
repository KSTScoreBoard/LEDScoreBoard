/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// mwx header
#include <TWELITE>
#include "myAppBhvParent.hpp"

/*****************************************************************/
// MUST DEFINE CLASS NAME HERE
#define __MWX_APP_CLASS_NAME MY_APP_PARENT
#include "_mwx_cbs_cpphead.hpp"
/*****************************************************************/

// called when this object is created (in ::setup())
void MY_APP_PARENT::setup() {
}

// called when this object is stareted (after setup)
void MY_APP_PARENT::begin() {
}

// loop procedure (do nothing here)
void MY_APP_PARENT::loop() {
	while (Serial.available()) {
		int c = Serial.read();

		Serial << uint8_t(c) << "->";

		switch(c) {
			// case 'a': test_a();	break;
		default:
			Serial.println();
		}
	}
}

// when receiving a packet from child device.
void MY_APP_PARENT::receive(mwx::packet_rx& rx) {
	uint8_t msg[4];

	// expand packet payload (shall match with sent packet data structure, see pack_bytes())
	auto&& np = expand_bytes(rx.get_payload().begin(), rx.get_payload().end(), msg);
	
	// if PING packet, respond pong!
	if (!strncmp((const char*)msg, (const char*)FOURCHARS, 4)) {
		Serial << '.';
		// Serial << format("[R:%d]", rx.get_psRxDataApp()->u8Seq);
		if (_monitor_active) {
			if(_seq_last == -1 || (((_seq_last + 1) & 0xFF) == rx.get_psRxDataApp()->u8Seq)) {
				;
			} else {
				// skipped?
				Serial << "\033[7mS\033[0m";
			}

			// get rest of data
			if (_buff_tail != _buff_head) {
				memcpy(_buff[_buff_tail], np, N_BUFFER * sizeof(buff_type));

				_buff_tail++;
				if (_buff_tail >= N_BUFFER_BANK) _buff_tail = 0;

				// Serial << format("(%d,%d)", _buff_head, _buff_tail);
			} else {
				// buffer overrun
				Serial << "\033[7mO\033[0m";
			}

			_seq_last = rx.get_psRxDataApp()->u8Seq;
		}
    }
}

// kick DIO monitoring
void MY_APP_PARENT::start_monitor(int hz, uint8_t dio_count, const uint8_t *dio_list) {
	_dio_count = dio_count;
	for (int i = 0; i < _dio_count; i++) {
		_dio_list[i] = dio_list[i];
		pinMode(_dio_list[i], OUTPUT_INIT_HIGH);
	}

	// calculate padding number, if _dio_count is NOT 2^n.
	_dio_count_pad = calc_dio_count_pad(_dio_count);

	// init vars
	init_vars();
	
	//start Timer0
	Timer0.begin(hz, true); // start timer 0
	_monitor_active = true;
}

// stop DIO monitoring
void MY_APP_PARENT::stop_monitor() {
	_monitor_active = false;
	Timer0.end();
	init_vars();
}

// init important vars.
void MY_APP_PARENT::init_vars() {
	_buff_head = -1;
	_buff_tail = 0;
	_buff_bit_index = 0;
	_seq_last = -1;
}

// TIMER_0 INT HANDLER (runs at khz)
MWX_TIMER_INT(0, uint32_t arg, uint8_t& handled) {
	// no further event
	handled = true;

	if (_buff_head == -1) {
		/// wait for buffers accumulation

		// start monitoring
		if (_buff_tail == 2) {
			_buff_head = 0;
		}
	} else {
		/// Set DIO from buffer
		int c = _buff[_buff_head][_buff_bit_index / (sizeof(buff_type) * 8)];
		int off = _buff_bit_index & (sizeof(buff_type) * 8 - 1);

		for (int i = 0; i < _dio_count; i++, off++) {
			bool b = (c & (1 << off));

			digitalWrite(_dio_list[i], b ? HIGH : LOW);
		}

		_buff_bit_index += (_dio_count + _dio_count_pad);

		if (_buff_bit_index >= N_BUFFER * sizeof(buff_type) * 8) {
			// go with the next bank
			_buff_bit_index = 0;
			_buff_head++;
			//Serial << '<' << char_t('0' + _buff_head) << '>';
			if (_buff_head >= N_BUFFER_BANK) _buff_head = 0;

			if (_buff_head == _buff_tail) {
				// buffer underrun
				_buff_head = -1;
				_buff_tail = 0;
				_seq_last = -1;

				Serial << "\033[7mU\033[0m";
			}
		}
	}
}

/*****************************************************************/
// common procedure (DO NOT REMOVE)
#include "_mwx_cbs_cpptail.cpp"
// MUST UNDEF CLASS NAME HERE
#undef __MWX_APP_CLASS_NAME
/*****************************************************************/