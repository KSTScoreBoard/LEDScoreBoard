#include <TWELITE>
#include <NWK_SIMPLE>
#include <stdlib.h>

// application ID
const uint32_t APP_ID = 0x1234abcd;
// channel
const uint8_t CHANNEL = 13;

/*** function prototype */
void vTransmit(uint16_t* _payload);

uint8_t score[3] = {0,0,0};

/*** the setup procedure (called on boot) */
void setup() {
    // the twelite main class
	the_twelite
		<< TWENET::appid(APP_ID)    // set application ID (identify network group)
		<< TWENET::channel(CHANNEL) // set channel (pysical channel)
		<< TWENET::rx_when_idle();  // open receive circuit (if not set, it can't listen packts from others)
    
    // Register Network
	auto&& nwksmpl = the_twelite.network.use<NWK_SIMPLE>();
	nwksmpl << NWK_SIMPLE::logical_id(0xFE) // set Logical ID. (0xFE means a child device with no ID)
	        << NWK_SIMPLE::repeat_max(3);   // can repeat a packet up to three times. (being kind of a router)
    
    
    the_twelite.begin(); // start twelite!
    Timer0.begin(1);
    Serial << "--- Sender ---" << crlf;
}

/*** loop procedure (called every event) */
void loop() {
    while(Serial.available()){
        int c = Serial.read();
        Serial << mwx::crlf << char(c) << ':';
        if(char(c) == 'c'){
            Timer0.begin(1);
            break;
        }
        if(isdigit(char(c))){
            Timer0.end();
            score[0] = score[1];
            score[1] = score[2];
            score[2] = c - '0';
            uint16_t payload[1] = {score[0] * 100 + score[1] * 10 + score[2]};
            vTransmit(payload);
        }
    }

    if(Timer0.available()){
        uint16_t payload[1] = {(millis()/1000)%1000};
        vTransmit(payload);
    }
}

void vTransmit(uint16_t* payload){
    if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
        // set tx packet behavior
        pkt << tx_addr(0xFF)  // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
            << tx_retry(0x3) // set retry (0x3 send four times in total)
            << tx_packet_delay(100,200,20); // send packet w/ delay (send first packet with randomized delay from 100 to 200ms, repeat every 20ms)

        // prepare packet payload
        pack_bytes(pkt.get_payload() // set payload data objects.
            , uint16_t(payload[0]) // put timestamp here.
        );
    
        // do transmit 
        pkt.transmit();
        Serial << "<TX " << format(" payload=%03ds>",payload[0]) << crlf;
    }
}