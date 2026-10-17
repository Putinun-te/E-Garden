// E-Garden air node. Ported from the v1-original sketch; logic unchanged.
#include <Arduino.h>
#include <EGarden.h>
#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_AHTX0.h>

String message;
String localAddress = String(eg::ADDR_AIR);  // v1 sent "2": changed (approved A)

BH1750 lightMeter;

Adafruit_AHTX0 aht;

RTC_DATA_ATTR int readingID = 0;

uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds
uint64_t TIME_TO_SLEEP = eg::air::SLEEP_SECONDS;

float light;
float Temp;
float Humid;

float batteryLevel;
const int batteryPin = eg::air::PIN_BATTERY;
const int RELAY_PIN = 27;

// Forward declarations (the Arduino IDE generates these for .ino files, .cpp needs them)
void getReadings();
void sendReadings();

void setup() {
  Serial.begin(115200);
  Wire.begin();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  pinMode(RELAY_PIN, OUTPUT);

  LoRa.setPins(eg::air::PIN_LORA_SS, eg::air::PIN_LORA_RST, eg::air::PIN_LORA_DIO0);
  Serial.println("initializing LoRa");

  int counter = 0;
  while (!LoRa.begin(eg::LORA_FREQUENCY_HZ) && counter < eg::LORA_INIT_ATTEMPTS) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == eg::LORA_INIT_ATTEMPTS) {
    // Increment readingID on every new reading
    readingID++;
    // Start deep sleep
    Serial.println("Failed to initialize LoRa. Going to sleep now");
    esp_deep_sleep_start();
  }
  LoRa.setSyncWord(eg::LORA_SYNC_WORD);  // v1 used 0xAA: changed (approved A)
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
  for (int i = 0; i < eg::air::BATTERY_SAMPLES; i++)
  {
    analog_volt += analogRead(batteryPin);
    delay(eg::air::BATTERY_SAMPLE_GAP_MS);
  }
  float batteryVolt = ((analog_volt / eg::air::BATTERY_SAMPLES) / 4095) * eg::air::BATTERY_FULL_VOLTS;
  analog_volt = 0;
  batteryLevel = (batteryVolt / eg::air::BATTERY_FULL_VOLTS) * 100;
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
