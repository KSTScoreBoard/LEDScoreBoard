#include <TWELITE>
#include <NWK_SIMPLE>

// BOARD SELECTION (comment out either)
//#define USE_PAL PAL_MAG // PAL_MOT, PAL_AMB
#undef USE_PAL // no board defined

// application ID
const uint32_t APP_ID = 0x1234abcd;

// channel
const uint8_t CHANNEL = 13;

// application use
const uint8_t FOURCHARS[] = "SV53";

// id
uint8_t u8ID = 0xFE;

#ifdef USE_PAL
// X_STR makes string literals.
#define X_STR(s) TO_STR(s)
#define TO_STR(s) #s
#include X_STR(USE_PAL) // just use with PAL board (to handle WDT)
#endif

#include <vl53l1x/VL53L1X.h>
VL53L1X sns_tof; // note: constructor is not called here.

// shutdown pin
const uint8_t PIN_SHUTDOWN = PIN_DIGITAL::DIO8;

// measurement result
const uint8_t CT_MEASURE_MAX = 8;
const uint8_t CT_MEASURE_PRE = 2; // skipping count
int ct_now;
uint16_t u16Result[CT_MEASURE_MAX] = {};

// state control
enum class E_STATE {
    INIT = 0,
    CAPTURE_PRE,
    CAPTURE,
    TX,
    TX_WAIT_COMP,
    SUCCESS,
    ERROR
};
E_STATE eState = E_STATE::INIT;

uint8_t u8txid; // TX ID
uint32_t u32tick_tx; // for Timeout of TX operation

// start capturing
bool kick_sensor();
void sleepNow();

/*** the setup procedure (called on boot) */
void setup() {
#ifdef USE_PAL
    auto&& brd = the_twelite.board.use<USE_PAL>(); // register board (PAL)
#endif

	// the twelite main object.
	the_twelite
		<< TWENET::appid(APP_ID)       // set application ID (identify network group)
        << TWENET::channel(CHANNEL)   // set channel (pysical channel)
        << TWENET::rx_when_idle(false) // set RX channel OFF.
		;

	// Register Network
	auto&& nwk = the_twelite.network.use<NWK_SIMPLE>();
	nwk << NWK_SIMPLE::logical_id(u8ID); // set Logical ID. (0xFE means a child device with no ID)

    the_twelite.begin(); // start twelite!

    pinMode(PIN_SHUTDOWN, PIN_MODE::OUTPUT_INIT_HIGH);
    Serial << "--- VL53L1X sample code ---" << crlf;
}

/*** the second setup procedure, runs once */
void begin() {
    sleepNow();
}

/*** called when waking up */
void wakeup() {
    Serial << crlf << "..wake up.";
    digitalWrite(PIN_SHUTDOWN, HIGH);

    delay(10);
    sns_tof.setup(); // instead of calling constructor.

    Wire.begin();
    eState = E_STATE::INIT;
}

/*** the loop procedure (called every event) */
void loop() {
    bool new_state;

    do {
        new_state = false;

        switch(eState) {
            case E_STATE::INIT:
                Serial << crlf << format("[INIT(%05d)]", millis() & 0xFFFF) << crlf;
                ct_now = 0;

                if (!kick_sensor()) {
                    Serial << "!FATAL: Failed to detect and initialize sensor!";
                    eState = E_STATE::ERROR;
                } else {
                    eState = E_STATE::CAPTURE_PRE;
                    Serial << '[';
                }
                new_state = true;
            break;
            
            case E_STATE::CAPTURE_PRE:
                sns_tof.read();
                Serial << 's';
                ct_now++;
                
                if (ct_now >= CT_MEASURE_PRE) {
                    eState = E_STATE::CAPTURE;
                }
            break;
            
            case E_STATE::CAPTURE:
            {   
                uint16_t u16val = sns_tof.read();
                bool b_timeout = sns_tof.timeoutOccurred();
                if (ct_now >= CT_MEASURE_PRE) { // store 8 samples (skip first 2 samples)
                    u16Result[ct_now - 2] = u16val;
                    Serial << (b_timeout ? 't' : '.');
                    ct_now++;
                }

                if (ct_now >= CT_MEASURE_PRE + CT_MEASURE_MAX) {
                    Serial << "] " << crlf << "L[mm]= ";
                    int ave = 0;
                    for(auto x : u16Result) {
                        ave += x;
                        Serial << format("%4d ", x); 
                    }
                    Serial << format("ave=%4d", ave / CT_MEASURE_MAX);
                    Serial << crlf;

                    eState = E_STATE::TX;
                    new_state = true; // one more loop
                }
            }
            break;

            case E_STATE::TX:
                eState = E_STATE::ERROR; // change this when success TX request...

                // stop capture
                sns_tof.stopContinuous();
                // set shutdown pin
                digitalWrite(PIN_SHUTDOWN, LOW);

                if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
                    // set tx packet behavior
                    pkt << tx_addr(0x00)  // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
                        << tx_retry(0x1) // set retry (0x1 send two times in total)
                        << tx_packet_delay(0, 0, 2); // send packet w/ delay

                    // prepare packet payload
                    auto &&ctn = pack_bytes(pkt.get_payload() // set payload data objects.
                        , make_pair(FOURCHARS, 4)  // just to see packet identification, you can design in any.
                        , uint8_t(CT_MEASURE_MAX)  // count of data
                    );

                    for (uint16_t x : u16Result) {
                        pack_bytes(ctn, x); // push each data on the tail of payload.
                    }

                    // do transmit
                    MWX_APIRET ret = pkt.transmit();
                
                    if (ret) {
                        Serial << format("[TX REQUEST(%05d)] id = %d.", millis(), ret.get_value()) << mwx::crlf << mwx::flush;

                        u8txid = ret.get_value() & 0xFF;
                        u32tick_tx = millis();
                        eState = E_STATE::TX_WAIT_COMP;
                        break;
                    }
                    else {
                        Serial << crlf << "!FATAL: TX REQUEST FAILS. reset the system." << crlf;
                        eState = E_STATE::ERROR;
                        new_state = true;
                        break;
                    }
                } else {
                    Serial << crlf << "!FATAL: MWX TX OBJECT FAILS. reset the system." << crlf;
                    eState = E_STATE::ERROR;
                    new_state = true;
                    break;
                }
            break;

            case E_STATE::TX_WAIT_COMP:
                if (the_twelite.tx_status.is_complete(u8txid)) {
                    // success on TX
                    eState = E_STATE::SUCCESS;
                    new_state = true;
                } else 
                if (millis() - u32tick_tx > 3000) {
                    // maybe FATAL, reset the system
                    Serial << crlf << '(' << int(millis()) << ')'<< "!FATAL: TX TIMEOUT. reset the system." << crlf;

                    eState = E_STATE::ERROR;
                    new_state = true;
                }
            break;

            case E_STATE::ERROR: // FATAL ERROR
                // stop the sensor
                Serial << mwx::flush;
                delay(100);
                //the_twelite.reset_system();
                sleepNow();
            break;

            case E_STATE::SUCCESS: // NORMAL EXIT (go into sleeping...)
                Serial << format("[SUCCESS(%05d)]", millis() & 0xFFFF) << crlf;

                // stop the sensor
                sleepNow();
            break;
        }
    } while (new_state);
}

// start sensor capturing
bool kick_sensor() {
    sns_tof.setTimeout(500);
    if (!sns_tof.init()) return false;

    sns_tof.setDistanceMode(VL53L1X::Long);
    sns_tof.setMeasurementTimingBudget(50000);
    sns_tof.startContinuous(50);

    return true;
}

// perform sleeping
void sleepNow() {
	uint32_t u32ct = 2000;
    u32ct = random(u32ct - u32ct / 8, u32ct + u32ct / 8); // add random to sleeping ms.
	Serial << "..sleeping " << int(u32ct) << "ms." << mwx::crlf;

	the_twelite.sleep(u32ct, false);
}

/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */