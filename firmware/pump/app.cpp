// E-Garden pump node. Ported from the v1-original sketch; logic unchanged.
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define ss 5
#define rst 14
#define dio0 2

#define IED 26

unsigned long lasttime ;
unsigned long lasttime2 ;

int counter = 0; int readingID = 0;
int rssi;
String message;

String loRaMessage;
String Local_ID;

String temperature_air;
String humidity;
String light;
String batteryLevel_air;

String temperature_water;
String batteryLevel_water;
String ph;
String ec;

String localAddress = "123";

bool CoC = false;
bool Relay = false;
bool start_timer = false;

String Data1;

bool state_pump = false;
int Time_State = 90000;

unsigned long previousMillis = 0;
const long interval = 1000;

// Forward declarations (the Arduino IDE generates these for .ino files, .cpp needs them)
void onReceive(int packetSize);
void start_timer_pump();

void setup() {
  pinMode(IED, OUTPUT);

  Serial.begin(115200);//initialize Serial Monito
  while (!Serial);
  Serial.println("LoRa Receiver");


  LoRa.setPins(ss, rst, dio0);//setup LoRa transceiver module

  int counter = 0;
  while (!LoRa.begin(433E6) && counter < 30) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  LoRa.setSyncWord(0xAB);
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa Initializing OK!");
}

void loop() {
//  unsigned long currentMillis = millis();
  
  if (state_pump == true) {
    unsigned long currentMillis = millis();
    start_timer_pump();
    if (currentMillis - previousMillis >= Time_State) {
      Data1 = "oFF";
      digitalWrite(IED, LOW);
      state_pump = false;
      start_timer = false;
      Serial.println("___PUMP OFF___");
    }
  }
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return
  while (LoRa.available()) {
    Serial.print("Received packet '");
    String LoRaData = LoRa.readString();
    Serial.print(LoRaData);
    Serial.print("' with RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", Snr: ");
    Serial.println(LoRa.packetSnr());

    int pos1 = LoRaData.indexOf('@');
    int pos2 = LoRaData.indexOf('/');

    Local_ID = LoRaData.substring(pos1 + 1, pos2);
    Serial.println(Local_ID);

    if (Local_ID == "1") {
      int pos3 = LoRaData.indexOf('&');
      Data1 = LoRaData.substring(pos2 + 1, pos3);
      Serial.println(Data1);
      if (Data1 == "On") {
        Serial.println("Hello1");
        digitalWrite(IED, HIGH);  LoRa.receive();
        state_pump = true;
      }
      if (Data1 == "oFF") {
        Serial.println("Hello2");
        digitalWrite(IED, LOW);  LoRa.receive();
        state_pump = false;
      }
    }
    Data1 = "";
  }
}

void start_timer_pump() {
  if (start_timer == false){
    previousMillis = millis();
    start_timer = true;
    Serial.println("___PUMP ON___");
  }
}
