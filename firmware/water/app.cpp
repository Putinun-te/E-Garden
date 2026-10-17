// E-Garden water node. Ported from the v1-original sketch; logic unchanged.
#include <Arduino.h>
#include <EGarden.h>
// Libraries for LoRa Module
#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>

#define RE eg::water::PIN_RS485_RE
#define DE eg::water::PIN_RS485_DE


#define Pump_A eg::water::PIN_DOSE_STEP_ACID
#define Pump_B eg::water::PIN_DOSE_STEP_B
#define Pump_C eg::water::PIN_DOSE_STEP_A

#define X_DIR eg::water::PIN_DOSE_DIR
#define X_EN eg::water::PIN_DOSE_EN

String message;
String localAddress = String(eg::ADDR_WATER);

String ADD; String Value;

String Local_ID;
int counter = 0;
int Var; int Var2; int Var3; int Var1;
int rssi;

RTC_DATA_ATTR int readingID = 0;

uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds
// Sleep for 30 minutes = 0.5 hours  = 1800 seconds
uint64_t TIME_TO_SLEEP = 10;


const byte ph_temp [8] = {0x05, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0x8F};
const byte ec[] = {0x0A, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0xB1};

byte values_ph[11];
byte values_ec[11];

unsigned long lasttime ;

SoftwareSerial mod(eg::water::PIN_RS485_RX, eg::water::PIN_RS485_TX);

float PH, TEMP;
int EC;

//Variable to hold battery level;
int batteryPin;
int batteryLevel;
const int DIR = 12;
const int STEP = 14;
const int  steps_per_rev = 200;

bool Open1 = false; bool Open2 = false; bool Open3 = false;

long now = millis();
long lastMeasure = 0;
bool start_timer = false;

// Forward declarations (the Arduino IDE generates these for .ino files, .cpp needs them)
void step(boolean dir, byte dirPin, byte stepperPin, int steps);
void start_timer_do();
void getReadings();
void sendReadings();
void Read_all_sensor();
void Read_PH_TEMP();
void Read_EC();
void clear_sensor485();
void Do();
void onReceive(int packetSize);

void step (boolean dir, byte dirPin, byte stepperPin, int steps) {
  digitalWrite (dirPin, dir);
  for (int i = 0; i < steps; i++)
  {
    digitalWrite (stepperPin, HIGH);
    delayMicroseconds (eg::water::STEP_HALF_PERIOD_US);
    digitalWrite (stepperPin, LOW);
    delayMicroseconds (eg::water::STEP_HALF_PERIOD_US);
  }
}

void start_timer_do() {
  if (start_timer == false) {
    lastMeasure = millis();
    start_timer = true;
    Serial.println("___Timer ON___");
  }
}

void setup() {

  Serial.begin(115200);
  mod.begin(eg::water::RS485_BAUD);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);

  pinMode (Pump_A, OUTPUT);
  pinMode (Pump_B, OUTPUT);
  pinMode (Pump_C, OUTPUT);
  pinMode (X_EN, OUTPUT);
  pinMode (X_DIR, OUTPUT);
  digitalWrite (X_EN, LOW);

  LoRa.setPins(eg::water::PIN_LORA_SS, eg::water::PIN_LORA_RST, eg::water::PIN_LORA_DIO0);
  Serial.println("initializing LoRa");
  int counter = 0;
  while (!LoRa.begin(eg::LORA_FREQUENCY_HZ) && counter < eg::LORA_INIT_ATTEMPTS) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == eg::LORA_INIT_ATTEMPTS) {
    readingID++;
    Serial.println("Failed to initialize LoRa. Going to sleep now");
  }
  LoRa.setSyncWord(eg::LORA_SYNC_WORD);
  Serial.println("LoRa initializing OK!");

  LoRa.onReceive(onReceive);
  LoRa.receive();
}

void loop() {
  start_timer_do();
  now = millis();
  if (start_timer && (now - lastMeasure > eg::water::MEASURE_INTERVAL_MS)) {
    Do();
    start_timer = false;
  }


  if (Open1 == true) {
    Serial.println(Var1);
    LoRa.beginPacket();
    LoRa.print("");
    LoRa.endPacket();
    for (int a = 0; a < Var1; a++) {
      step (eg::water::DOSE_DIRECTION, X_DIR, Pump_C, eg::water::STEPS_PER_UNIT_A);
    }
    digitalWrite (X_EN, LOW);
    Open1 = false;
  }
  if (Open2 == true) {
    Serial.println(Var2);
    LoRa.beginPacket();
    LoRa.print("");
    LoRa.endPacket();
    for (int a = 0; a < Var2; a++) {
      step (eg::water::DOSE_DIRECTION, X_DIR, Pump_B, eg::water::STEPS_PER_UNIT_B);
    }
    digitalWrite (X_EN, LOW);
    Open2 = false;
  }  if (Open3 == true) {
    Serial.println(Var3);
    LoRa.beginPacket();
    LoRa.print("");
    LoRa.endPacket();
    for (int a = 0; a < Var3; a++) {
      step (eg::water::DOSE_DIRECTION, X_DIR, Pump_A, eg::water::STEPS_PER_UNIT_ACID);
    }
    LoRa.receive();
    digitalWrite (X_EN, LOW);
    Open3 = false;
  }
}


void getReadings() {

  float analog_volt = 0;
  for (int i = 0; i < 20; i++)
  {
    analog_volt += analogRead(batteryPin);
    delay(50);
  }
  float batteryVolt = ((analog_volt / 20) / 4095) * 4.2;
  analog_volt = 0;
  batteryLevel = (batteryVolt / 4.2) * 100;
}

void sendReadings() {
  message = String(readingID) + "@" + String(localAddress) + "/" + String(PH) + "&" +
            String(EC) + "$" + String(TEMP) + "#";

  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}

void Read_all_sensor() {
  Read_PH_TEMP();  delay(500);  Read_PH_TEMP();  delay(500);
  clear_sensor485();

  Read_EC();  delay(500);  Read_EC();  delay(500);
  clear_sensor485();
  
//  if (Read_EC() >= 1600 && Read_EC() <= 1700){
//    Read_all_sensor();
//  }
//  else(){
//    Do();
//  }

}

void Read_PH_TEMP() {
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(100);
  if (mod.write(ph_temp, sizeof(ph_temp)) == 8) {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
    for (byte i = 0; i < 9; i++) {
      values_ph[i] = mod.read();
    }
  }
  PH = float((uint16_t)(values_ph[5] << 8) | (values_ph[6])) / 10 ;
  TEMP = float((uint16_t)(values_ph[3] << 8) | (values_ph[4])) / 10;
}

void Read_EC() {
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(100);
  if (mod.write(ec, sizeof(ec)) == 8) {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
    for (byte i = 0; i < 9; i++) {
      values_ec[i] = mod.read();
    }
  }
  EC = ((uint16_t)(values_ec[3] << 8) | (values_ec[4])) / 10 ;
}

void clear_sensor485() {
  for (byte i = 0; i < 11; i++) {
    values_ph[i] = 0;
  }
  for (byte i = 0; i < 9; i++) {
    values_ec[i] = 0;
  }
}

void Do() {

  Read_all_sensor();
  Serial.print("Temperature Celsius = ");
  Serial.println(TEMP);
  Serial.print("PH: ");
  Serial.println(PH);
  Serial.print("EC: ");
  Serial.println(EC);
  Serial.print("Reading ID = ");
  Serial.println(readingID);


  sendReadings();
  Serial.print("Message sent = ");
  Serial.println(message);
  message = "";

  readingID++;
  LoRa.receive();
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  while (LoRa.available()) {
    Serial.print("Received packet '");
    String LoRaData = LoRa.readString();
    Serial.print(LoRaData);
    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", Snr: ");
    Serial.println(LoRa.packetSnr());

    int pos1 = LoRaData.indexOf('@');
    int pos2 = LoRaData.indexOf('/');
    int pos3 = LoRaData.indexOf('&');
    int pos4 = LoRaData.indexOf('$');
    Local_ID = LoRaData.substring(pos1 + 1, pos2);
    if (Local_ID == String(eg::ADDR_HUB)) {
      ADD = LoRaData.substring(pos2 + 1, pos3);
      Value = LoRaData.substring(pos3 + 1, pos4);
      Serial.println(ADD); Serial.println(Value);
      Var = Value.toInt(); Serial.println(Var);
      if (Var < 0) {
        Var = 0;
      }

      digitalWrite (X_EN, LOW);
      if (ADD == "Add(A)") {
        Var1 = Var; Open1 = true;
      }
      if (ADD == "Add(B)") {
        Var2 = Var; Open2 = true;
      }
      if (ADD == "Add(Acid)") {
        Var3 = Var; Open3 = true;
      }
    }
  }
}
