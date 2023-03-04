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
        Serial << format("%d",c) << crlf;
    }
}