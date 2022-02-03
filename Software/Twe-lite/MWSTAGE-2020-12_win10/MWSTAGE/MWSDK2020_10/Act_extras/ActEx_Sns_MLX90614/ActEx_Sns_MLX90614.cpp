// use twelite mwx c++ template library
#include <TWELITE>
#include <NWK_SIMPLE>
#include <STG_STD>

#include "Adafruit_MLX90614/Adafruit_MLX90614.h"

/*** Config part */
// application ID
const uint32_t DEF_APPID = 0x1234abcd;
uint32_t APP_ID = DEF_APPID;

// channel
uint8_t CHANNEL = 13;

// id
uint8_t u8ID = 0;

// application use
const char_t APP_NAME[] = "IR TEMP SENSOR";
const uint8_t FOURCHARS[] = "SIT1";

uint8_t u8txid = 0;
uint32_t u32tick;

// very simple state machine
enum class E_STATE {
    INIT = 0,
    CAPTURE_PRE,
    CAPTURE,
    TX,
    TX_WAIT_COMP,
	SETTING_MODE,
    SUCCESS,
    ERROR
};
E_STATE eState = E_STATE::INIT;
bool b_state_resume_on_wake_up = false;
uint16_t u16_nap_dur = 100;

/*** Local function prototypes */
// perform sleep
void sleepNow();

// fast division by 100.
struct my_div_res { int32_t quo; int32_t rem; bool b_neg; };
my_div_res my_div100(int val);

/*** sensor objects */
Adafruit_MLX90614 mlx;
struct {
	uint16_t u16temp_ambient;
	uint16_t u16temp_object;
} sns_val{};

// ADCs
uint16_t u16_volt_vcc = 0;
uint16_t u16_volt_a1 = 0;

/*** setup procedure (run once at cold boot) */
void setup() {
	/*** SETUP section */
	/// interactive mode settings
	auto&& set = the_twelite.settings.use<STG_STD>();
			// declare to use interactive setting.
			// once activated, use `set.serial' instead of `Serial'.

	set << SETTINGS::appname(APP_NAME)          // set application name appears in interactive setting menu.
		<< SETTINGS::appid_default(DEF_APPID); // set the default application ID.

	set.hide_items(
		  E_STGSTD_SETID::POWER_N_RETRY
		, E_STGSTD_SETID::OPTBITS
		, E_STGSTD_SETID::OPT_DWORD1
		, E_STGSTD_SETID::OPT_DWORD2
		, E_STGSTD_SETID::OPT_DWORD3
		, E_STGSTD_SETID::ENC_MODE
		, E_STGSTD_SETID::ENC_KEY_STRING
	);

	// read SET pin (DIO12)
	pinMode(PIN_DIGITAL::DIO12, PIN_MODE::INPUT_PULLUP);
	if (digitalRead(PIN_DIGITAL::DIO12) == LOW) {
		set << SETTINGS::open_at_start();      // start interactive mode immediately.
		eState = E_STATE::SETTING_MODE;        // set state in loop() as dedicated mode for settings.
		return;                                // skip standard initialization.
	}

	// acquired EEPROM saved data	
	set.reload(); // must call this before getting data, if configuring method is called.

	APP_ID = set.u32appid();
	CHANNEL = set.u8ch();
	u8ID = set.u8devid();

	// the twelite main object.
	the_twelite
		<< TWENET::appid(APP_ID)     // set application ID (identify network group)
		<< TWENET::channel(CHANNEL); // set channel (pysical channel)

	// Register Network
	auto&& nwk = the_twelite.network.use<NWK_SIMPLE>();
	nwk << NWK_SIMPLE::logical_id(u8ID); // set Logical ID. (0xFE means a child device with no ID)

	/*** BEGIN section */
	/*** INIT message */
	Serial << crlf << "--- " << APP_NAME << ":" << FOURCHARS << " ---";
	Serial << crlf << format("..APP_ID   = %08X", APP_ID);
	Serial << crlf << format("..CHANNEL  = %d", CHANNEL);
	Serial << crlf << format("..LID      = %d(0x%02X)", u8ID, u8ID);

	// sensors.setup() may call Wire during initialization.
	Wire.begin(WIRE_CONF::WIRE_100KHZ);

	// tsc34725
	mlx.begin();

	Serial << crlf << "..mlx begin()";

	// setup analogue
	Analogue.setup();

	/*** let the_twelite begin! */
	the_twelite.begin(); // start twelite!

}

/*** loop procedure (called every event) */
void loop() {
	bool new_state;

	if (Analogue.available()) {
		if (!u16_volt_vcc) {
			u16_volt_vcc = Analogue.read(PIN_ANALOGUE::VCC);
			u16_volt_a1 =  Analogue.read(PIN_ANALOGUE::A1);
		}
	}

	do {
		new_state = false;

		switch(eState) {			
		case E_STATE::SETTING_MODE: // while in setting (interactive mode)
			break;

		case E_STATE::INIT:
			Serial << crlf << format("..%04d/start sensor capture.", millis() & 8191);
			u32tick = millis(); // wait timer

			// start sensor capture
			Analogue.begin(pack_bits(PIN_ANALOGUE::A1, PIN_ANALOGUE::VCC)); // _start continuous adc capture.

			eState =  E_STATE::CAPTURE_PRE;
			new_state = true;
			break;

		case E_STATE::CAPTURE_PRE: // wait for sensor capture completion
#ifdef USE_NAP
#if 1
			// wait completion by sleep.
			Serial << crlf << "..nap " << int(u16_nap_dur) << "ms.";
			Serial.flush();

			eState = E_STATE::CAPTURE; // next state
			b_state_resume_on_wake_up = true; // flag to resume state on waking up.
			the_twelite.sleep(u16_nap_dur, false);
#else
			if (TickTimer.available()) {
				// both sensors are finished.
				if (millis() - u32tick > u16_nap_dur) {
					new_state = true; // do next state immediately.
					eState =  E_STATE::CAPTURE;
					break;
				}
			}
#endif
#else
			// do nothing
			eState =  E_STATE::CAPTURE;
			new_state = true;
#endif
			break;

		case E_STATE::CAPTURE: // display sensor results
			sns_val.u16temp_object = mlx.readObjectTempC100();
			sns_val.u16temp_ambient = mlx.readAmbientTempC100();

			Serial << crlf 
					<< format("..SENSOR DATA %d, %d", sns_val.u16temp_object, sns_val.u16temp_ambient);

			if (1) {
				// separate 
				auto d1 = my_div100(sns_val.u16temp_object);
				auto d2 = my_div100(sns_val.u16temp_ambient);

				Serial << crlf
					<< format("  Object  = %c%2d.%02d", d1.b_neg ? '-' : '+', d1.quo, d1.rem)
					<< format("  Ambient = %c%2d.%02d", d2.b_neg ? '-' : '+', d2.quo, d2.rem);
			}

			if (1) {
				Serial << crlf 
					<< format("  ADC      : Vcc=%dmV A1=%04dmV", u16_volt_vcc, u16_volt_a1);
			}
			
			new_state = true; // do next state immediately.
			eState =  E_STATE::TX;
			break;

		case E_STATE::TX: // place TX packet requiest.
			eState = E_STATE::ERROR; // change this when success TX request...

			if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
				// set tx packet behavior
				pkt << tx_addr(0x00)  // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
					<< tx_retry(0x1) // set retry (0x1 send two times in total)
					<< tx_packet_delay(0, 0, 2); // send packet w/ delay

				// prepare packet payload
				pack_bytes(pkt.get_payload() // set payload data objects.
					, make_pair(FOURCHARS, 4)  // just to see packet identification, you can design in any.
					, uint16_t(sns_val.u16temp_object)  // object temp [degC x 100]
					, uint16_t(sns_val.u16temp_ambient) // ambient temp [degC x 100]
					, uint16_t(u16_volt_vcc)            // adc val (VCC) [mV]
					, uint16_t(u16_volt_a1)             // adc val (ADC1) [mV]
				);

				// do transmit
				MWX_APIRET ret = pkt.transmit();
				Serial << crlf << format("..%04d/transmit request by id = %d.", millis() & 8191, ret.get_value());
				
				if (ret) {
					u8txid = ret.get_value() & 0xFF;
					u32tick = millis();
					eState = E_STATE::TX_WAIT_COMP;
				} else {
					Serial << crlf << "!FATAL: TX REQUEST FAILS. reset the system." << crlf;
				}
			} else {
				Serial << crlf << "!FATAL: MWX TX OBJECT FAILS. reset the system." << crlf;
			}
			break;

		case E_STATE::TX_WAIT_COMP: // wait TX packet completion.
			if (the_twelite.tx_status.is_complete(u8txid)) {
				Serial << crlf << format("..%04d/transmit complete.", millis() & 8191);
		
				// success on TX
				eState = E_STATE::SUCCESS;
				new_state = true;
			} else if (millis() - u32tick > 3000) {
				Serial << crlf << "!FATAL: MWX TX OBJECT FAILS. reset the system." << crlf;
				eState = E_STATE::ERROR;
				new_state = true;
			} 
			break;

		case E_STATE::ERROR: // FATAL ERROR
			Serial.flush();
			delay(100);
			the_twelite.reset_system();
			break;

		case E_STATE::SUCCESS: // NORMAL EXIT (go into sleeping...)
			sleepNow();
			break;
		}
	} while(new_state);
}

// perform sleeping
void sleepNow() {
	uint32_t u32ct = 1750 + random(0,500);
	Serial << crlf << format("..%04d/sleeping %dms.", millis() % 8191, u32ct);
	Serial.flush();

	the_twelite.sleep(u32ct, false);
}

// wakeup procedure
void wakeup() {
	if (b_state_resume_on_wake_up) {
		Serial  << "..wake up.";
	} else {
		Serial	<< crlf << "--- " << APP_NAME << ":" << FOURCHARS << " wake up ---";
		eState = E_STATE::INIT; // go into INIT state in the loop()
	}
	
	b_state_resume_on_wake_up = false;
}

// calculate val/100, val%100 faster. (only works from -99999 to 99999)
my_div_res my_div100(int val) {
	int32_t neg = val < 0;
	if (neg) val = -val;
	int dv = val * 10486 >> 20; // compute as slightly bigger.
	int32_t rem = val - dv * 100;
	if (rem < 0) { // has bigger error in `dv', adjust it. (only handle when `dv' gets smaller)
		dv--;
		rem += 100;
	}
	return { dv, rem, neg };
}

/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */