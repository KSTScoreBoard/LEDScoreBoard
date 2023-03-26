#include <TWELITE>
#include <NWK_SIMPLE>
#include <stdlib.h>

/*** function prototype */
void vTransmit(uint8_t addr, int score, int brightness);

int brightness = 4;

/*** the setup procedure (called on boot) */
void setup() {
    // the twelite main class
	the_twelite
		<< TWENET::appid(0x1234abcd)    // set application ID (identify network group)
		<< TWENET::channel(13) // set channel (pysical channel)
		<< TWENET::rx_when_idle();  // open receive circuit (if not set, it can't listen packts from others)
    
    // Register Network
	auto&& nwksmpl = the_twelite.network.use<NWK_SIMPLE>();
	nwksmpl << NWK_SIMPLE::logical_id(0x00) // set Logical ID. (0xFE means a child device with no ID)
	        << NWK_SIMPLE::repeat_max(3);   // can repeat a packet up to three times. (being kind of a router)
    
    SerialParser.begin(PARSER::BINARY, 128); // Initialize the serial parser
    the_twelite.begin(); // start twelite!
    //Timer0.begin(1);
    Serial << "--- Sender ---" << crlf;
}

/*** loop procedure (called every event) */
void loop() {
    while(Serial.available()){
        int c = Serial.read();
        SerialParser.parse(c);
        if(SerialParser) {
            // 書式解釈完了、b に得られたデータ列(smplbuf<uint8_t>)
            auto&& b = SerialParser.get_buf();
            vTransmit(b[0],(b[1] << 8) + b[2],b[3]);
        }
    }

    if(Timer0.available()){
        vTransmit(0xFF,(millis()/1000)%1000,brightness);
    }
}

void vTransmit(uint8_t addr,int score,int brightness){
    if (auto&& pkt = the_twelite.network.use<NWK_SIMPLE>().prepare_tx_packet()) {
        // set tx packet behavior
        pkt << tx_addr(addr)  // 0..0xFF (LID 0:parent, FE:child w/ no id, FF:LID broad cast), 0x8XXXXXXX (long address)
            << tx_retry(0x5) // set retry (0x3 send four times in total)
            << tx_packet_delay(100,200,20); // send packet w/ delay (send first packet with randomized delay from 100 to 200ms, repeat every 20ms)

        // prepare packet payload
        pack_bytes(pkt.get_payload() // set payload data objects.
            , (uint16_t)score
            , (uint16_t)brightness
        );
    
        // do transmit 
        pkt.transmit();
        Serial << "<TX " << format("score=%d brightness=%d >",score,brightness) << crlf;
    }
}