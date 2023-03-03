// use twelite mwx c++ template library
#include <TWELITE>
#include <BRD_APPTWELITE>
#include <NWK_SIMPLE>
#include <stdio.h>
#include "ST7032.h"

#define Button_PlusOne 12
#define Button_MinusOne 13
#define Button_PlusTen 10
#define Button_MinusTen 11
#define Button_PlusHun 5
#define Button_MinusHun 9

#define _SCLR 4

void shiftOut(uint8_t, uint8_t, uint8_t, uint32_t);
void setScore(uint8_t*);

// application ID
const uint32_t APP_ID = 0x1234abcd;
// channel
const uint8_t CHANNEL = 13;

ST7032 lcd;

uint8_t segment[11] = {0b1111110,
					   0b0110000,
					   0b1111001,
					   0b0110011,
					   0b0110011,
					   0b1011011,
					   0b1011111,
					   0b1110000,
					   0b1111111,
					   0b1111011,
					   0x00};
uint8_t score[3] = {0,0,0};

/*** setup procedure (run once at cold boot) */
void setup() {
	// the twelite main class
	auto&& brd = the_twelite.board.use<BRD_APPTWELITE>();
	the_twelite
		<< TWENET::appid(APP_ID)    // set application ID (identify network group)
		<< TWENET::channel(CHANNEL) // set channel (pysical channel)
		<< TWENET::rx_when_idle();  // open receive circuit (if not set, it can't listen packts from others)
	
	// Register Network
	auto&& nwksmpl = the_twelite.network.use<NWK_SIMPLE>();
	nwksmpl << NWK_SIMPLE::logical_id(0xFE) // set Logical ID. (0xFE means a child device with no ID)
	        << NWK_SIMPLE::repeat_max(3);   // can repeat a packet up to three times. (being kind of a router)
	
	the_twelite.begin(); // start twelite!
	Timer1.end();
	Timer2.end();
	SPI.begin(0, SPISettings(100000, SPI_CONF::LSBFIRST, SPI_CONF::SPI_MODE0));

	Buttons.setup(5);
    Buttons.begin(1UL << 5, 5, 10);
	
	Timer4.change_hz(10000);
	Timer4.change_duty(1024*0.0);

	lcd.begin(8,2);
    lcd.setContrast(25);

	pinMode(Button_PlusOne,INPUT_PULLUP);
	pinMode(Button_MinusOne,INPUT_PULLUP);
	pinMode(Button_PlusTen,INPUT_PULLUP);
	pinMode(Button_MinusTen,INPUT_PULLUP);
	pinMode(Button_PlusHun,INPUT_PULLUP);
	pinMode(Button_MinusHun,INPUT_PULLUP);
	pinMode(_SCLR,OUTPUT);

	Buttons.setup(5);
	Buttons.begin(1UL << Button_PlusOne | 1UL << Button_MinusOne | 
				  1UL << Button_PlusTen | 1UL << Button_MinusTen |
				  1UL << Button_PlusHun | 1UL << Button_MinusHun,5,10);
	
	Serial << "--- This Is Receiver ---" << crlf;				
}

/*** loop procedure (called every event) */
void loop() {
	if (Buttons.available()) {
        uint32_t bm, cm;
        Buttons.read(bm, cm);

        if (!(bm & (1UL << Button_PlusOne))) {
			score[2]++;
        }else if(!(bm & (1UL << Button_MinusOne))){
            score[2]--;
        }else if(!(bm & (1UL << Button_PlusTen))){
            score[1]++;
        }else if(!(bm & (1UL << Button_MinusTen))){
            score[1]--;
        }else if(!(bm & (1UL << Button_PlusHun))){
            score[0]++;
        }else if(!(bm & (1UL << Button_MinusHun))){
            score[0]--;
        }

		
		if(score[0] >= 255) score[0] = 9;
		if(score[1] >= 255) score[1] = 9;	
		if(score[2] >= 255) score[2] = 9;
		if(score[0] > 9) score[0] = 0;
		if(score[1] > 9) score[1] = 0;
		if(score[2] > 9) score[2] = 0;

		setScore(score);
    }
}

void on_rx_packet(packet_rx& rx, bool_t &handled) {
	// rx >> Serial; // debugging (display longer packet information)
	uint16_t timestamp;

	// expand packet payload (shall match with sent packet data structure, see pack_bytes())
	expand_bytes(rx.get_payload().begin(), rx.get_payload().end()
				, timestamp // 4bytes of timestamp
	);

	// display the packet
	Serial << format("<RX ad=%x/lq=%d/ln=%d/sq=%d:" // note: up to 4 args!
				, rx.get_psRxDataApp()->u32SrcAddr
				, rx.get_lqi()
				, rx.get_length()
				, rx.get_psRxDataApp()->u8Seq
				)
			<< format(" TS=%dms>" // note: up to 4 args!
				, timestamp
				)
			<< mwx::crlf
			<< mwx::flush;

	uint8_t _score[3] = {timestamp / 100, (timestamp % 100)/10, timestamp % 10};
	setScore(_score);
}

void setScore(uint8_t* _score){
	memcpy(score,_score,3);
	if (auto&& trs = SPI.get_rwer()) { // オブジェクトの生成とデバイスの通信判定
			// このスコープ(波かっこ)内が trs の有効期間。
			trs << (uint8_t)(segment[_score[0]]);
			trs << (uint8_t)(segment[_score[1]]);
			trs << (uint8_t)(segment[_score[2]]);
		}	
		char str[64];
		sprintf(str,"%d%d%d",_score[0],_score[1],_score[2]);
		lcd.print(str);
}