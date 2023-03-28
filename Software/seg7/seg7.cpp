// use twelite mwx c++ template library
#include <TWELITE>
#include <BRD_APPTWELITE>
#include <NWK_SIMPLE>
#include <stdio.h>
#include "ST7032.h"

#define DUTY_MAX 8

#define addr_01 2
#define addr_02 3

#define Button_PlusOne 5
#define Button_MinusOne 9
#define Button_PlusTen 10
#define Button_MinusTen 11
#define Button_PlusHun 12
#define Button_MinusHun 13

#define _SCLR 4

void display();
ST7032 lcd;

uint8_t segment[11] = {0b1111110,
					   0b0110000,
					   0b1111001,
					   0b1111001,
					   0b0110011,
					   0b1011011,
					   0b1011111,
					   0b1110000,
					   0b1111111,
					   0b1111011,
					   0x00};
int16_t score = 888;
int16_t brightness = 4;
uint8_t lq = 0;
int16_t temp;

/*** setup procedure (run once at cold boot) */
void setup() {
	//setup pinMode
	{
		pinMode(addr_01,INPUT_PULLUP);
		pinMode(addr_02,INPUT_PULLUP);
		pinMode(Button_PlusOne,INPUT_PULLUP);
		pinMode(Button_MinusOne,INPUT_PULLUP);
		pinMode(Button_PlusTen,INPUT_PULLUP);
		pinMode(Button_MinusTen,INPUT_PULLUP);
		pinMode(Button_PlusHun,INPUT_PULLUP);
		pinMode(Button_MinusHun,INPUT_PULLUP);
		pinMode(_SCLR,OUTPUT);

		Buttons.setup(5);
		Buttons.begin(1UL << 5, 5, 10);
		Buttons.setup(5);
		Buttons.begin(1UL << Button_PlusOne | 1UL << Button_MinusOne | 
					1UL << Button_PlusTen | 1UL << Button_MinusTen |
					1UL << Button_PlusHun | 1UL << Button_MinusHun,5,10);

		Analogue.setup(true, ANALOGUE::KICK_BY_TIMER0);
		Analogue.begin(BRD_APPTWELITE::PIN_AI1);
	}

	// the twelite main class
	auto&& brd = the_twelite.board.use<BRD_APPTWELITE>();
	the_twelite
		<< TWENET::appid(0x1234abcd)    // set application ID (identify network group)
		<< TWENET::channel(13) // set channel (pysical channel)
		<< TWENET::rx_when_idle();  // open receive circuit (if not set, it can't listen packts from others)
	
	// Register Network
	uint8_t addr = 1 + (digitalRead(addr_01)  ==  HIGH ? 0 : 1) | (digitalRead(addr_02) == HIGH ? 0 : 1) << 1;
	auto&& nwksmpl = the_twelite.network.use<NWK_SIMPLE>();
	nwksmpl << NWK_SIMPLE::logical_id(addr) // set Logical ID. (0xFE means a child device with no ID)
	        << NWK_SIMPLE::repeat_max(3);   // can repeat a packet up to three times. (being kind of a router)
	
	the_twelite.begin(); // start twelite!
	Timer0.begin(10,true);
	Timer1.end();
	Timer2.end();
	Timer4.change_hz(5000);
	Timer4.change_duty(DUTY_MAX - brightness,DUTY_MAX);
	SPI.begin(0, SPISettings(100000, SPI_CONF::LSBFIRST, SPI_CONF::SPI_MODE0));
	
	lcd.begin(8,2);
    lcd.setContrast(25);

	char str[64];
	sprintf(str,"id=%d\n",addr);
	lcd.print(str);
	Serial << "--- This Is Receiver id=" << format("%d",addr) << "---" << crlf;
	delay(1000);			
}

/*** loop procedure (called every event) */
void loop() {
	if (Buttons.available()) {
        uint32_t bm, cm;
        Buttons.read(bm, cm);

		if(!(bm & (1UL << Button_MinusHun | 1UL << Button_PlusOne))){
			brightness += 1;
		}else if(!(bm & (1UL << Button_MinusHun | 1UL << Button_MinusOne))){
			brightness -= 1;
		}else if (!(bm & (1UL << Button_PlusOne))) {
			score+=1;
        }else if(!(bm & (1UL << Button_MinusOne))){
            score-=1;
        }else if(!(bm & (1UL << Button_PlusTen))){
            score+=10;
        }else if(!(bm & (1UL << Button_MinusTen))){
            score-=10;
        }else if(!(bm & (1UL << Button_PlusHun))){
            score+=100;
        }else if(!(bm & (1UL << Button_MinusHun))){
            score-=100;
        }

		if(brightness >= 8) brightness = 8;
		if(brightness < 0) brightness = 0;
		if(score > 999)score = 999;
		if(score < 0)score = 0;
		
		display();
    }

	if(Analogue.available()){
		temp = (Analogue.read(BRD_APPTWELITE::PIN_AI1)-600)/10;
		display();
	}
}

void on_rx_packet(packet_rx& rx, bool_t &handled) {
	// rx >> Serial; // debugging (display longer packet information)
	uint16_t _score = 0;
	uint16_t _brightness = 0;

	// expand packet payload (shall match with sent packet data structure, see pack_bytes())
	expand_bytes(rx.get_payload().begin(), rx.get_payload().end()
				, _score
				, _brightness
	);

	// display the packet
	Serial << format("<RX ad=%x/lq=%d/ln=%d/sq=%d:" // note: up to 4 args!
				, rx.get_psRxDataApp()->u32SrcAddr
				, rx.get_lqi()
				, rx.get_length()
				, rx.get_psRxDataApp()->u8Seq
				)
			<< format(" score=%d, brightness=%d>" // note: up to 4 args!
				, _score
				, _brightness
				)
			<< mwx::crlf
			<< mwx::flush;

	score = int16_t(_score);
	brightness = int16_t(_brightness);
	lq = rx.get_lqi();
	display();
}

void display(){
	int _score[3] = {score / 100, (score%100)/10, score % 10};
	if (auto&& trs = SPI.get_rwer()) { // オブジェクトの生成とデバイスの通信判定
		// このスコープ(波かっこ)内が trs の有効期間。
		trs << (uint8_t)(segment[_score[0]]);
		trs << (uint8_t)(segment[_score[1]]);
		trs << (uint8_t)(segment[_score[2]]);
	}
	if(brightness >=  8) brightness = 8;  
	Timer4.change_duty(DUTY_MAX - brightness ,DUTY_MAX);

	char str[64];
	sprintf(str,"%03d %d\nt=%d",score,brightness,temp);
	lcd.print(str);
}