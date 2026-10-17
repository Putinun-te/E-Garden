// Libraries for LoRa Module
#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
//DS18B20 libraries
//#include <OneWire.h>
//#include <DallasTemperature.h>

//#include <Wire.h>
//#include <BH1750.h>

//#include <Adafruit_AHTX0.h>

// LoRa Module pin definition
// define the pins used by the transceiver module
#define ss 5
#define rst 15
#define dio0 2
#define RE 32
#define DE 33
// LoRa message variable
String message;
String localAddress = "2";

//BH1750 lightMeter;

//Adafruit_AHTX0 aht;

// Save reading number on RTC memory
RTC_DATA_ATTR int readingID = 0;

// Define deep sleep options
uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds
// Sleep for 30 minutes = 0.5 hours  = 1800 seconds
uint64_t TIME_TO_SLEEP = 10;

// Data wire is connected to ESP32 GPIO15
//#define ONE_WIRE_BUS 15
// Setup a oneWire instance to communicate with a OneWire device
//OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
//DallasTemperature sensors(&oneWire);

// Moisture Sensor variables
//const int moisturePin = 26;
//const int moisturePowerPin = 12;
//int soilMoisture;
const byte ph_temp [8] = {0x05, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0x8F};
const byte ec[] = {0x0A, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0xB1};

byte values_ph[11];
byte values_ec[11];


SoftwareSerial mod(26, 27);

float PH, TEMP;
int EC;
// Temperature Sensor variables
//float tempC;
//float tempF;
//float light;
//float Temp;
//float Humid;

//Variable to hold battery level;
float batteryLevel;
const int batteryPin = 39;
const int DIR = 12;
const int STEP = 14;
const int  steps_per_rev = 200;

void setup() {   
  //  pinMode(moisturePowerPin, OUTPUT);

  // Start serial communication for debugging purposes
  Serial.begin(115200);
//  Wire.begin();
  mod.begin(9600);
  // Enable Timer wake_up
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  // Start the DallasTemperature library
  //  sensors.begin();

  // Initialize LoRa
  //replace the LoRa.begin(---E-) argument with your location's frequency
  //note: the frequency should match the sender's frequency
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  LoRa.setPins(ss, rst, dio0);
  Serial.println("initializing LoRa");

  int counter = 0;
  while (!LoRa.begin(433E6) && counter < 30) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 30) {
    // Increment readingID on every new reading
    readingID++;
    // Start deep sleep
    Serial.println("Failed to initialize LoRa. Going to sleep now");
    esp_deep_sleep_start();
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSyncWord(0xAB);
  Serial.println("LoRa initializing OK!");
  
  Read_all_sensor();
  getReadings();
  Serial.print("Battery level = ");
  Serial.println(batteryLevel, 2);
//  Serial.print("Light LUX = ");
//  Serial.println(light);
  Serial.print("Temperature Celsius = ");
  Serial.println(TEMP);
//  Serial.print("Humidity = ");
//  Serial.println(Humid);
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

  // Increment readingID on every new reading
  readingID++;

  // Start deep sleep
  Serial.println("DONE! Going to sleep now.");
  esp_deep_sleep_start();
}

void loop() {
//  digitalWrite(DIR, HIGH);
//  Serial.println("Spinning Clockwise...");
//  
//  for(int i = 0; i<steps_per_rev; i++)
//  {
//    digitalWrite(STEP, HIGH);
//    delayMicroseconds(2000);
//    digitalWrite(STEP, LOW);
//    delayMicroseconds(2000);
//  }
//  delay(1000); 
//  
//  digitalWrite(DIR, LOW);
//  Serial.println("Spinning Anti-Clockwise...");
//
//  for(int i = 0; i<steps_per_rev; i++)
//  {
//    digitalWrite(STEP, HIGH);
//    delayMicroseconds(1000);
//    digitalWrite(STEP, LOW);
//    delayMicroseconds(1000);
//  }
//  delay(1000);

}

void getReadings() {
  // Measure Light
//  lightMeter.begin();
//  float lux = lightMeter.readLightLevel();
//  light = lux;
//
//  // Measure temperature, humid
//  aht.begin();
//  sensors_event_t humidity, temp;
//  aht.getEvent(&humidity, &temp);
//  Temp = temp.temperature;
//  Humid = humidity.relative_humidity;

  //Measure battery level
  //  batteryLevel = map(analogRead(batteryPin), 0.0f, 4095.0f, 0, 100);
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
  // Send packet data
  // Send temperature in Celsius
  message = String(readingID) + "@" + String(localAddress) + "/" + String(PH) + "&" +
  String(EC) + "$" + String(TEMP) + "#" + String(batteryLevel) + "^" ;

  delay(1000);
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}

void Read_all_sensor() {
  Read_PH_TEMP();  delay(1000);  Read_PH_TEMP();  delay(1000);
  clear_sensor485();

  //  EC_old = EC;
  Read_EC();  delay(1000);  Read_EC();  delay(1000);
  clear_sensor485();
  delay(1000);

  while (PH > 14) {
    Serial.print("PH = "); Serial.print(PH);
    Serial.println(" PH Sensor ERROR");
    Read_PH_TEMP();  delay(1000);  Read_PH_TEMP();  delay(1000);
    clear_sensor485();
    //    Read_EC();  delay(1000);  Read_EC();  delay(1000);
    //    clear_sensor485();
    delay(1000);
  }
  while (TEMP > 100) {
    Serial.print("Temp = "); Serial.print(TEMP);
    Serial.println(" TEMP Sensor ERROR");
    Read_PH_TEMP();  delay(1000);  Read_PH_TEMP();  delay(1000);
    clear_sensor485();
    //    Read_EC();  delay(1000);  Read_EC();  delay(1000);
    //    clear_sensor485();
    delay(1000);
  }
  while (EC > (3000)) {
    Serial.print("EC = "); Serial.print(EC);
    Serial.println(" EC Sensor ERROR");
    //    Read_PH_TEMP();  delay(1000);  Read_PH_TEMP();  delay(1000);
    //    clear_sensor485();
    Read_EC();  delay(1000);  Read_EC();  delay(1000);
    clear_sensor485();
    delay(1000);
  }
}

void Read_PH_TEMP() {  
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(100);
  if (mod.write(ph_temp, sizeof(ph_temp)) == 8)
  {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
    for (byte i = 0; i < 9; i++)
    {
      values_ph[i] = mod.read();
    }
  }
  PH = float((uint16_t)(values_ph[5] << 8) | (values_ph[6])) / 11.5;
  TEMP = float((uint16_t)(values_ph[3] << 8) | (values_ph[4])) / 10;
}

void Read_EC() {
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(100);
  if (mod.write(ec, sizeof(ec)) == 8)
  {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
    for (byte i = 0; i < 9; i++)
    {
      values_ec[i] = mod.read();
    }
  }
 EC = ((uint16_t)(values_ec[3]<<8)|(values_ec[4]))/14;
}

void clear_sensor485() {
  for (byte i = 0; i < 11; i++)
  {
    values_ph[i] = 0;
//        Serial.print(values_ph[i], HEX);
//        Serial.print(" ");
  }
  //  Serial.println();
  for (byte i = 0; i < 9; i++)
  {
    values_ec[i] = 0;
    //    Serial.print(values_ec[i], HEX);
    //    Serial.print(" ");
  }
//  Serial.println();
}
