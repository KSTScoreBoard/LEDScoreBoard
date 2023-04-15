const char* id = "B1";


#include <ST7032_asukiaaa.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WebSockets2_Generic.h>
#include <WiFi.h>
#include "defines.h"

using namespace websockets2_generic;

#define PIN_SI    10
#define PIN_LATCH 20
#define PIN_CLK    8
#define PIN_G     21

#define SDA       6
#define SCL       7

uint8_t segment[11] = {0b1111110,
					   0b0110000,
					   0b1101101,
					   0b1111001,
					   0b0110011,
					   0b1011011,
					   0b1011111,
					   0b1110000,
					   0b1111111,
					   0b1111011,
					   0x00};

ST7032_asukiaaa lcd;
WebsocketsClient client;

void onMessageCallback(WebsocketsMessage message)
{
  Serial.print("Got Message: ");
  Serial.println(message.data());

  StaticJsonDocument<256> doc;
  deserializeJson(doc, message.data());
  if(doc["auth"] == "OK"){
    lcd.setCursor(0, 1);
    lcd.print("auth OK");
  }else{
  }
  if(doc["to"] == id){
    int score = doc["score"];
    int level = doc["level"];

    digitalWrite( PIN_LATCH, LOW );
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score/100]);
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score%100/10]);
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score%10]);
    digitalWrite( PIN_LATCH, HIGH );

    ledcWrite(0,7-level);
    lcd.clear();
    char str[50];
    sprintf(str,"%03d",score);
    lcd.print(str);
    lcd.setCursor(0, 1);
    lcd.print(level);
  }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  (void) data;

  if (event == WebsocketsEvent::ConnectionOpened)
  {
    Serial.println("Connnection Opened");
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    Serial.println("Connnection Closed");
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    Serial.println("Got a Pong!");
  }
}

void setup() {
  pinMode(PIN_SI,OUTPUT);
  pinMode(PIN_CLK,OUTPUT);
  pinMode(PIN_LATCH,OUTPUT);

  digitalWrite( PIN_LATCH, LOW );
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  digitalWrite( PIN_LATCH, HIGH );

  Serial.begin(115200);

  ledcSetup(0,10000,3);
  ledcAttachPin(PIN_G,0);
  ledcWrite(0,4);

  Wire.begin(SDA,SCL);
  lcd.setWire(&Wire);
  lcd.begin(8,2);
  lcd.setContrast(30);
  lcd.print(id);
  delay(500);


  while (!Serial && millis() < 5000);

  Serial.print("\nStart Secured-ESP32-Client on ");
  Serial.println(ARDUINO_BOARD);
  Serial.println(WEBSOCKETS2_GENERIC_VERSION);

  // Connect to wifi
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.print("WiFi");

  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++)
  {
    Serial.print(".");
    lcd.print(".");
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("No Wifi!");
    lcd.setCursor(0, 1);
    lcd.print("No WiFi");
    return;
  }

  Serial.print("\nConnected to Wifi");
  // run callback when messages are received
  client.onMessage(onMessageCallback);
  // run callback when events are occuring
  client.onEvent(onEventsCallback);
  // Before connecting, set the ssl fingerprint of the server
  client.setCACert(echo_org_ssl_ca_cert);

  // Connect to server
  bool connected = client.connect("wss://cloud.achex.ca");
  lcd.clear();
  if (connected)
  {
    Serial.println("Connected!");
    lcd.print("conn OK");
    String login = "";
    String user = id;		
    String passwd = "1234";
    login = "{\"auth\":\"" + user + "\"}";	//create json text
    client.send(login);
  }
  else
  {
    Serial.println("Not Connected!");
    lcd.print("conn NG");
  }
}

void loop() {
  client.poll();
}
