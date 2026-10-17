// E-Garden air node. Ported from the v1-original sketch; logic unchanged.
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_AHTX0.h>
#define ss 5
#define rst 15
#define dio0 2

String message;
String localAddress = "2";

BH1750 lightMeter;

Adafruit_AHTX0 aht;

RTC_DATA_ATTR int readingID = 0;

uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds
uint64_t TIME_TO_SLEEP = 10;

float light;
float Temp;
float Humid;

float batteryLevel;
const int batteryPin = 36;
const int RELAY_PIN = 27;

// Forward declarations (the Arduino IDE generates these for .ino files, .cpp needs them)
void getReadings();
void sendReadings();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  pinMode(RELAY_PIN, OUTPUT);

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
  LoRa.setSyncWord(0xAA);
  Serial.println("LoRa initializing OK!");


  getReadings();
  Serial.print("Battery level = ");
  Serial.println(batteryLevel, 2);
  Serial.print("Light LUX = ");
  Serial.println(light);
  Serial.print("Temperature Celsius = ");
  Serial.println(Temp);
  Serial.print("Humidity = ");
  Serial.println(Humid);
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
  digitalWrite(RELAY_PIN, HIGH); // turn on pump 5 seconds
  delay(5000);
  digitalWrite(RELAY_PIN, LOW);  // turn off pump 5 seconds
  delay(1000);


}

void getReadings() {
  lightMeter.begin();
  float lux = lightMeter.readLightLevel();
  light = lux;

  aht.begin();
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Temp = temp.temperature;
  Humid = humidity.relative_humidity;

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
  message = String(readingID) + "@" + String(localAddress) + "/" + String(Temp) + "&" +
            String(Humid) + "$" + String(light) + "#" + String(batteryLevel) + "^";

  delay(1000);
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();
}
