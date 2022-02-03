// use twelite mwx c++ template library
#include <TWELITE>
#include <NWK_SIMPLE>
#include <BRD_APPTWELITE> // use standard ports setup.

#include "common.hpp"

#include "myAppBhvChild.hpp"
#include "myAppBhvParent.hpp"

/*** Local objects */
uint8_t u8ID = 0;

// Hz to capture/monitor DIO
uint16_t u16Hz = 4000;

// 1,2,4,8 ports are recommended.
uint8_t u8PortCount = 1;
uint8_t au8portInput[] = {
	BRD_APPTWELITE::PIN_DI1,
	BRD_APPTWELITE::PIN_DI2,
	BRD_APPTWELITE::PIN_DI3,
	BRD_APPTWELITE::PIN_DI4,
	};

uint8_t au8portOutput[] = {
	BRD_APPTWELITE::PIN_DO1,
	BRD_APPTWELITE::PIN_DO2,
	BRD_APPTWELITE::PIN_DO3,
	BRD_APPTWELITE::PIN_DO4,
	};

/*** Local function prototypes */

/*** setup procedure (run once at cold boot) */
void setup() {
	/*** SETUP section */
	// use PAL_AMB board behavior.
	auto&& brd = the_twelite.board.use<BRD_APPTWELITE>();

	// now read DIP sw status can be read.
	if (brd.get_M1()) {
		u8ID = 0;
	} else {
		u8ID = 1;
	}

	// M2/M3 profile
	int prof = (brd.get_M2() ? 1 : 0) + (brd.get_M3() ? 2 : 0);	
	switch(prof) {
	case 0: // M2=O M3=O --> 4ch, 1Khz
		u8PortCount = 4;
		u16Hz = 1000;
	break;
	case 1: // M2=G M3=O --> 2ch, 2Khz
		u8PortCount = 2;
		u16Hz = 2000;
	break;
	case 2: // M2=O M3=G --> 1ch, 4Khz
		u8PortCount = 1;
		u16Hz = 4000;
	break;
	case 3: // M2=G M3=G --> 1ch, 6.4Khz
		u8PortCount = 1;
		u16Hz = 6400;
	break;
	}

	// BPS=G --> double sample rate. not sure if it works...
	if (brd.get_BPS()) {
		u16Hz *= 2;
	}

	// Register App Behavior (set differnt Application by DIP SW settings)
	if (u8ID == 0) {
		// put settings to the twelite main object.
		the_twelite
			<< TWENET::appid(APP_ID)     // set application ID (identify network group)
			<< TWENET::channel(CHANNEL)  // set channel (pysical channel)
			<< TWENET::rx_when_idle()    // open RX channel
			;

		auto&& app = the_twelite.app.use<MY_APP_PARENT>(); // register parent app

		app.start_monitor(u16Hz, u8PortCount, au8portOutput); // start the app (wait for child's packet.)
	} else {		
		// put settings to the twelite main object.
		the_twelite
			<< TWENET::appid(APP_ID)        // set application ID (identify network group)
			<< TWENET::channel(CHANNEL)     // set channel (pysical channel)
			<< TWENET::rx_when_idle(false)  // no rx channel
			<< TWENET::cca_level(0)			// no CCA
			<< TWENET::cca_retry(0)
			;

		auto&& app = the_twelite.app.use<MY_APP_CHILD>(); // register child app

		app.start_capture(u16Hz, u8PortCount, au8portInput); // start the apps (monitor DIO and transmit.)
	}

	// Register Network Behavior
	auto&& nwk = the_twelite.network.use<NWK_SIMPLE>();
	nwk << NWK_SIMPLE::logical_id(u8ID) // set Logical ID.
		<< NWK_SIMPLE::repeat_max(0)    // no repeat
		<< NWK_SIMPLE::dup_check(8, 1000, 4) // short timeout (Note: this setting is important for very frequent tx/rx apps.)
		;

	/*** BEGIN section */
	the_twelite.begin(); // let the twelite begin!
	Serial << "---DIO Trans id=" << int(u8ID) << "---" << mwx::crlf;
}

// calclulating padding bits.
//  - if dio_count is not 2^n, the algorithm put dummy data to fit
//    byte(s) boundary.
int calc_dio_count_pad(int _dio_count) {
	// calculate padding number, if _dio_count is NOT 2^n.
	int _dio_count_pad = 0;
	if(_dio_count == 0) {
        ;
	} else if (_dio_count <= 2) {
		;
	} else if (_dio_count <= 4) {
		_dio_count_pad = 4 - _dio_count;
	} else if (_dio_count <= 8) {
		_dio_count_pad = 8 - _dio_count;
	} else if (_dio_count <= 16) {
		_dio_count_pad = 16 - _dio_count;
	}

    return _dio_count_pad;
}

/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */