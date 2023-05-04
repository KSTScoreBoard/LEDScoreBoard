#include <ST7032_asukiaaa.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WebSockets2_Generic.h>
#include <WiFi.h>
#include "defines.h"
using namespace websockets2_generic;

#define PIN_SI 10
#define PIN_LATCH 20
#define PIN_CLK 8
#define PIN_G 21

#define SDA 6
#define SCL 7

#define PIN_PO 9
#define PIN_MO 1
#define PIN_PT 4
#define PIN_MT 5
#define PIN_PH 3
#define PIN_MH 2

uint8_t segment[10] = {0b1111110,
					   0b0110000,
					   0b1101101,
					   0b1111001,
					   0b0110011,
					   0b1011011,
					   0b1011111,
					   0b1110000,
					   0b1111111,
					   0b1111011,};

ST7032_asukiaaa lcd;
WebsocketsClient client;

int score = 0;
int level = 4;
float tmp = 0;
bool hidden[3] = {false,false,false};
bool refresh = false;
bool reConnect = false;
bool doTemp = false;

void onMessageCallback(WebsocketsMessage message)
{
  Serial.print("Got Message: ");
  Serial.println(message.data());


  StaticJsonDocument<256> doc;
  deserializeJson(doc, message.data());
  if(doc["auth"] == "OK"){
    lcd.setCursor(0, 1);
    lcd.print("auth OK");
    delay(1000);
    lcd.clear();
    refresh = true;
    doTemp = true;
  }
  if(doc["to"] == identification){
    score = doc["score"];
    level = doc["level"];
    hidden[0] = doc["hidden0"];
    hidden[1] = doc["hidden1"];
    hidden[2] = doc["hidden2"];
    refresh = true;
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
    lcd.clear();
    lcd.print("ConnLost");
    reConnect = true;
    doTemp = false;
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


void IRAM_ATTR btnPushed() {
  static int old_time;
  if(millis() - old_time < 500) return;
  if(hidden[0] || hidden[1] || hidden[2]){
    hidden[0] = hidden[1] = hidden[2] = 0;
    refresh = true;
    return;
  }

  if(!digitalRead(PIN_PO)) score += 1;
  if(!digitalRead(PIN_MO)) score -= 1;
  if(!digitalRead(PIN_PT)) score += 10;
  if(!digitalRead(PIN_MT)) score -= 10;
  if(!digitalRead(PIN_PH)) score += 100;
  if(!digitalRead(PIN_MH)) score -= 100;

  if(score < 0) score = 0;
  if(score > 999) score = 999;
  old_time = millis();
  refresh = true;
}

bool onlineSetup(){
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
    lcd.setCursor(5,0);
    lcd.print(i);
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("No Wifi!");
    lcd.setCursor(0, 1);
    lcd.print("No WiFi");
    return false;
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
    Serial.println(client.available());
    lcd.print("conn OK");
    String login = "{\"auth\":\"" + identification + "\"}";	//create json text
    client.send(login);
    return true;
  }
  else
  {
    Serial.println("Not Connected!");
    lcd.print("conn NG");
  }
  return false;
}

void offlineSetup(){
  pinMode(PIN_SI,OUTPUT);
  pinMode(PIN_CLK,OUTPUT);
  pinMode(PIN_LATCH,OUTPUT);
  pinMode(PIN_PO,INPUT_PULLUP);
  pinMode(PIN_MO,INPUT_PULLUP);
  pinMode(PIN_PT,INPUT_PULLUP);
  pinMode(PIN_MT,INPUT_PULLUP);
  pinMode(PIN_PH,INPUT_PULLUP);
  pinMode(PIN_MH,INPUT_PULLUP);

  attachInterrupt(PIN_PO,btnPushed,FALLING);
  attachInterrupt(PIN_MO,btnPushed,FALLING);
  attachInterrupt(PIN_PT,btnPushed,FALLING);
  attachInterrupt(PIN_MT,btnPushed,FALLING);
  attachInterrupt(PIN_PH,btnPushed,FALLING);
  attachInterrupt(PIN_MH,btnPushed,FALLING);

  digitalWrite( PIN_LATCH, LOW );
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  shiftOut( PIN_SI, PIN_CLK, LSBFIRST, 0xFF);
  digitalWrite( PIN_LATCH, HIGH );

  ledcSetup(0,10000,3);
  ledcAttachPin(PIN_G,0);
  ledcWrite(0,7-level);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA,SCL);
  lcd.setWire(&Wire);
  lcd.begin(8,2);
  lcd.setContrast(30);
  lcd.print(identification);
  delay(500);

  onlineSetup();
  offlineSetup();
}

void display(){
  lcd.clear();
  char str[50];
  sprintf(str,"%03d  %d%d%d",score,hidden[0],hidden[1],hidden[2]);
  lcd.print(str);
  lcd.setCursor(0, 1);
  sprintf(str,"%d  %f",level,tmp);
  lcd.print(str);
}

void loop() {
  if(refresh){
    if(!(hidden[0] || hidden[1] || hidden[2])){
      digitalWrite( PIN_LATCH, LOW );
      shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score/100]);
      shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score%100/10]);
      shiftOut( PIN_SI, PIN_CLK, LSBFIRST, segment[score%10]);
      digitalWrite( PIN_LATCH, HIGH );
      ledcWrite(0,7-level);
    }
    display();
    doTemp = true;
    refresh = false;
  }

  if(hidden[0] || hidden[1] || hidden[2]){
    static int old_time;
    static int index = 0;
    if(millis() - old_time < 100) return;

    index = (++index % 7);
    digitalWrite( PIN_LATCH, LOW );
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST,hidden[0] ? 0x80 >> index : segment[score/100]);
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST,hidden[1] ? 0x80 >> index : segment[score%100/10]);
    shiftOut( PIN_SI, PIN_CLK, LSBFIRST,hidden[2] ? 0x80 >> index : segment[score%10]);
    digitalWrite( PIN_LATCH, HIGH );
    old_time = millis();
  }

  if(doTemp){
    static int old_time = millis();
    if(millis() - old_time < 1000) return;
    tmp = (analogReadMilliVolts(0)-600)/10.0;
    display();
    old_time = millis();
  }

  if(reConnect){
    reConnect = false;
    for(int i=0;i<3;i++){
      bool connected = onlineSetup();
      if(connected) break;
    } 
  }

  client.poll();
}
