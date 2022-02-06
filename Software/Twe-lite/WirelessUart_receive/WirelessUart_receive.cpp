
// use twelite mwx c++ template library
#include <TWELITE>
#include <NWK_SIMPLE>
#include <BRD_APPTWELITE>

/*** Config part */
// application ID
const uint32_t APP_ID = 0x1234abcd;
// channel
const uint8_t CHANNEL = 13;
// uid
uint8_t uid = 0;

/*** function prototype */
MWX_APIRET transmit(uint8_t addr, const uint8_t* b, const uint8_t* e);
void sendScore();

/*** application defs */
const uint8_t FOURCHARS[] = "WURT";

/*** setup procedure (run once at cold boot) */
void setup() {


	//button
	pinMode(11,INPUT_PULLUP);
	pinMode(12,INPUT_PULLUP);
	pinMode(13,INPUT_PULLUP);
	pinMode(16,INPUT_PULLUP);
	Buttons.setup(5);
	Buttons.begin(pack_bits(
						BRD_APPTWELITE::PIN_DI1,
						BRD_APPTWELITE::PIN_DI2,
						BRD_APPTWELITE::PIN_DI3,
						BRD_APPTWELITE::PIN_DI4),
					5, 		// history count
					4); 
	/*** SETUP section */
	// the twelite main class
	the_twelite
		<< TWENET::appid(APP_ID)    // set application ID (identify network group)
		<< TWENET::channel(CHANNEL) // set channel (pysical channel)
		<< TWENET::rx_when_idle();  // open receive circuit (if not set, it can't listen packts from others)

	// Register Network
	auto&& nwk = the_twelite.network.use<NWK_SIMPLE>();
	uid = 1;
	nwk	<< NWK_SIMPLE::logical_id(uid); // set Logical ID. (0xFE means a child device with no ID)

	/*** BEGIN section */
	SerialParser.begin(PARSER::ASCII, 128); // Initialize the serial parser
	the_twelite.begin(); // start twelite!

	/*** INIT message */
	Serial << "--- WirelessUart (id=" << int(uid) << ") ---" << mwx::crlf;
	Wire.begin(WIRE_CONF::WIRE_100KHZ,false);
	
}

unsigned int score[4] = {0,0,0,0};
unsigned char mode[4] = {0,0,0,0};

/*** loop procedure (called every event) */
void loop() {
	if(Buttons.available()){
		for(int i=0;i<4;i++){
			Wire.requestFrom(i,2);
			score[i] = Wire.read() + (Wire.read() << 8);
			delay(10);
		}

		Serial << int(score[0]) << mwx::crlf;

		uint32_t bm,cm;
		Buttons.read(bm,cm);
		if(!(bm & (1UL << 12))){
			Serial << "roll start" << mwx::crlf;
			mode[0] = mode[1] = mode[2] = mode[3] = 0b1110;
		}
		if(!(bm & (1UL << 13))){
			Serial << "stop one" << mwx::crlf;
			mode[0] = mode[1] = mode[2] = mode[3] = mode[0] & 0b1100;
		}
		if(!(bm & (1UL << 11))){
			Serial << "stop ten" << mwx::crlf;
			mode[0] = mode[1] = mode[2] = mode[3] = mode[0] & 0b1010;
		}
		if(!(bm & (1UL << 16))){
			Serial << "stop hun" << mwx::crlf;
			mode[0] = mode[1] = mode[2] = mode[3] = mode[0] & 0b0110;
		}
		Serial << int(mode[0]) << mwx::crlf;
		sendScore();

	}




    // read from serial
	while(Serial.available())  {
		if (SerialParser.parse(Serial.read())) {
			Serial << ".." << SerialParser;
			const uint8_t* b = SerialParser.get_buf().begin();
			uint8_t addr = *b; ++b; // the first byte is destination address.
			transmit(addr, b, SerialParser.get_buf().end());
		}
	}

	// packet
	if (the_twelite.receiver.available()) {
		auto&& rx = the_twelite.receiver.read();
		
		// check the packet header.
		const uint8_t* p = rx.get_payload().begin();
		if (rx.get_length() > 4 && !strncmp((const char*)p, (const char*)FOURCHARS, 4)) {
			Serial << format("..rx from %08x/%d", rx.get_addr_src_long(), rx.get_addr_src_lid()) << mwx::crlf;

			smplbuf_u8<128> buf;
			mwx::pack_bytes(buf			
					, uint8_t(rx.get_addr_src_lid())            // src addr (LID)
					, make_pair(p+4, rx.get_payload().end()) );	// data body

			serparser_attach pout;
			pout.begin(PARSER::ASCII, buf.begin(), buf.size(), buf.size());
			Serial << pout;
			p+=4;
			score[0] = *p++;
			score[0] += (*p++) << 8;
			mode[0] = *p++;
			score[1] = *p++;
			score[1] += (*p++) << 8;
			mode[1] = *p++;
			score[2] = *p++;
			score[2] += (*p++) << 8;
			mode[2] = *p++;
			score[3] = *p++;
			score[3] += (*p++) << 8;
			mode[3] = *p;
			sendScore();
		}
	}
}

void sendScore(){
	for(int i=0;i<4;i++){
		Wire.beginTransmission(i);
		Wire.write(score[i] & 0xFF);
		Wire.write(score[i] >> 8);
		Wire.write(mode[i]);
		Wire.endTransmission();
		delay(10);
	}
}

/** transmit a packet */
MWX_APIRET transmit(uint8_t dest, const uint8_t* b, const uint8_t* e) {
	if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
		// set tx packet behavior
		pkt << tx_addr(dest) // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
			<< tx_retry(0x1) // set retry (0x3 send four times in total)
			<< tx_packet_delay(20,100,10); // send packet w/ delay (send first packet with randomized delay from 20 to 100ms, repeat every 10ms)

		// prepare packet payload
		pack_bytes(pkt.get_payload() // set payload data objects.
			, make_pair(FOURCHARS, 4) // string should be paired with length explicitly.
			, make_pair(b, e) // put timestamp here.
		);
		
		// do transmit 
		return pkt.transmit(); 
	}

	return false;
}

/* Copyright (C) 2019 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */