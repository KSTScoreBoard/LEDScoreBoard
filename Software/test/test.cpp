#include <TWELITE>

/*** the setup procedure (called on boot) */
void setup() {
    SerialParser.begin(PARSER::ASCII, 128); // Initialize the serial parser
    Serial << "--- test ---" << crlf;
}

/*** loop procedure (called every event) */
void loop() {
    while (Serial.available()) {
        int c = Serial.read();
        
        SerialParser.parse(c);
        
        if(SerialParser) {
            // 書式解釈完了、b に得られたデータ列(smplbuf<uint8_t>)
            auto&& b = SerialParser.get_buf();
            
        }
    }
}