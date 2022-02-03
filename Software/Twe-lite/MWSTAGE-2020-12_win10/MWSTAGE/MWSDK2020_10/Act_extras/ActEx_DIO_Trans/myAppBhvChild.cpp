/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// mwx header
#include <TWELITE>
#include <NWK_SIMPLE>
#include "myAppBhvChild.hpp"

/*****************************************************************/
// MUST DEFINE CLASS NAME HERE
#define __MWX_APP_CLASS_NAME MY_APP_CHILD
#include "_mwx_cbs_cpphead.hpp"
/*****************************************************************/

// called when this object is created (in ::setup())
void MY_APP_CHILD::setup() {
}

// called when this object is stareted (after setup)
void MY_APP_CHILD::begin() {
}

// loop procedure
void MY_APP_CHILD::loop() {
}

// kick DIO capturing
void MY_APP_CHILD::start_capture(int hz, uint8_t dio_count, uint8_t *dio_list) {
	// LIST BITMAPS
	_dio_count = dio_count;
	for (int i = 0; i < _dio_count; i++) {
		_dio_list[i] = dio_list[i];
		
		pinMode(_dio_list[i], INPUT_PULLUP); // set peripheal (input w/ pullup)
	}

	// calculate padding number, if _dio_count is NOT 2^n.
	_dio_count_pad = calc_dio_count_pad(_dio_count);

	// WARNING
	if (hz * _dio_count > 8192) {
		Serial << crlf << format("!warning, %dhz is very high.", hz, hz);
	}

	// init vars
	init_vars();

	//start Timer0
	Timer0.begin(hz, true); // start timer 0
}

void MY_APP_CHILD::stop_capture() {
	Timer0.end();
	init_vars();
}

// init important vars.
void MY_APP_CHILD::init_vars() {
	// init vars
	_buff_busy = 0;		 // start with _buff[0]
	_buff[0][0] = 0;     // clear the first element
	_buff_available = 0;
    _buff_bit_index = 0; // 0..511
}

// TIMER_0 INT HANDLER (runs at khz)
MWX_TIMER_INT(0, uint32_t arg, uint8_t& handled) {
	// no further event
	handled = true;

	// firstly get DIO status
	uint32_t bm_dio = digitalReadBitmap(); 

	// save DIO information into internal buffer
	int off = _buff_bit_index & (sizeof(buff_type) * 8 - 1);
	buff_type& c = _buff[_buff_busy][_buff_bit_index / (sizeof(buff_type) * 8)];
	if(off == 0) c = 0;

	for(int i = 0; i < _dio_count; i++, off++) {
		bool b = (bm_dio & (1UL << _dio_list[i]));
		c |= b ? (1 << off) : 0;
	}

	// increment buff index
	_buff_bit_index += _dio_count + _dio_count_pad;

	// check if it reaches the buffer end.
	if (_buff_bit_index >= N_BUFFER * (sizeof(buff_type) * 8)) {
		/// switch to next buffer

		// set available flag (to transmit operation)
		_buff_available = BUFF_AVAIL_0 + _buff_busy;
		
		// set buffer vars.
		_buff_busy = !_buff_busy;
		_buff[_buff_busy][0] = 0;
		_buff_bit_index = 0;
	}

}

MWX_STATE(MY_APP_CHILD::STATE_IDLE, uint32_t ev, uint32_t evarg) {
	if (PEV_is_coldboot(ev,evarg)) {
		// Serial << "[STATE_IDLE:START_UP(" << int(evarg) << ")]" << mwx::crlf;
	} else
	if (PEV_is_warmboot(ev,evarg)) {
		// Serial << "[STATE_IDLE:START_UP(" << int(evarg) << ")]" << mwx::crlf;
	}

	if (_buff_available) {
		PEV_SetState(STATE_TX);
	}
}

MWX_STATE(MY_APP_CHILD::STATE_TX, uint32_t ev, uint32_t evarg) {
	static int u8txid;

	if (ev == E_EVENT_NEW_STATE) {
		// Serial << "[STATE_TX:NEW]" << mwx::crlf;
		u8txid = -1;
		
		if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
			// set tx packet behavior
			pkt << tx_addr(0x00)  // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
				<< tx_retry(0x2)  // set retry (0x2 send three times in total)
				<< tx_packet_delay(0, 0, 4)  // send packet w/ delay
				<< tx_process_immediate()    // transmit immediately
				;

			// prepare packet payload
			uint8_t *p, *e;
			p = (uint8_t*)&_buff[_buff_get_avail_idx(_buff_available)][0];
			e = p + N_BUFFER * sizeof(buff_type);

			pack_bytes(pkt.get_payload() // set payload data objects.
				, make_pair(FOURCHARS, 4)  // just to see packet identification, you can design in any.
				, make_pair(p, e)
			);

			// do transmit
			MWX_APIRET ret = pkt.transmit();
			
			if (ret) {
				u8txid = ret.get_value() & 0xFF;
			}
		}

		_buff_available = BUFF_AVAIL_NA;
	} else if (ev == E_ORDER_KICK && evarg == uint32_t(u8txid)) {
		// wait completion of transmit
		Serial << 't';
		PEV_SetState(STATE_IDLE);
	}

	// timeout (fatal)
	if (PEV_u32Elaspsed_ms() > 100) {
		// does not finish TX!
		Serial << "[STATE_TX] FATAL, TX does not finish!" << mwx::crlf << mwx::flush;
		the_twelite.reset_system();
	}
}

/*****************************************************************/
// common procedure (DO NOT REMOVE)
#include "_mwx_cbs_cpptail.cpp"
// MUST UNDEF CLASS NAME HERE
#undef __MWX_APP_CLASS_NAME
/*****************************************************************/
