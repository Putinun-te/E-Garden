// E-Garden air sensor node: wakes up, measures light, air temperature,
// humidity and battery, sends one reading to the hub, and deep-sleeps.
//
// v1 behaviour kept: pins, 10 s deep sleep, battery averaging (20 samples,
// 50 ms apart, 4.2 V = 100 %), up to 30 LoRa init attempts before giving up.
// Changed (approved): sync word 0xAB and address 3 so the hub receives it,
// protocol v2, failed sensors are reported instead of sending NaN, and the
// unreachable relay test in loop() and unused DS18B20 includes are removed.
#include <Adafruit_AHTX0.h>
#include <Arduino.h>
#include <BH1750.h>
#include <EGarden.h>
#include <Wire.h>

using namespace eg;

namespace {

BH1750 lightMeter;
Adafruit_AHTX0 aht;
LoRaLink radio;

float readBatteryPercent() {
  // Sensor averaging, not a wait for something else: kept as in v1.
  float sum = 0;
  for (int i = 0; i < air::BATTERY_SAMPLES; i++) {
    sum += analogRead(air::PIN_BATTERY);
    delay(air::BATTERY_SAMPLE_GAP_MS);
  }
  float volts = ((sum / air::BATTERY_SAMPLES) / 4095) * air::BATTERY_FULL_VOLTS;
  return (volts / air::BATTERY_FULL_VOLTS) * 100;
}

void sleepNow() {
  esp_sleep_enable_timer_wakeup(air::SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

}  // namespace

void setup() {
  eg::log::begin(115200);
  Wire.begin();

  // The air node only lives for one reading, so it waits for the radio here
  // (v1: up to 30 attempts, 500 ms apart) and sleeps if it never comes up.
  if (!radio.begin({air::PIN_LORA_SS, air::PIN_LORA_RST, air::PIN_LORA_DIO0}, ADDR_AIR, LORA_INIT_ATTEMPTS)) {
    EG_LOG("Air: LoRa failed, sleeping");
    sleepNow();
  }

  proto::Message m;
  m.type = proto::MsgType::Air;
  m.dst = ADDR_HUB;
  bool ahtOk = false, luxOk = false;

  if (aht.begin()) {
    sensors_event_t humidity, temp;
    if (aht.getEvent(&humidity, &temp) && !isnan(temp.temperature) && !isnan(humidity.relative_humidity)) {
      m.setFloat("t", temp.temperature, 2);
      m.setFloat("h", humidity.relative_humidity, 1);
      ahtOk = true;
    }
  }
  if (lightMeter.begin()) {
    float lux = lightMeter.readLightLevel();  // negative on error
    if (lux >= 0) {
      m.setFloat("lux", lux, 1);
      luxOk = true;
    }
  }
  m.setInt("bat", (long)readBatteryPercent());
  if (!ahtOk || !luxOk) m.set("err", !ahtOk && !luxOk ? "aht+bh1750" : (!ahtOk ? "aht" : "bh1750"));

  int32_t seq = radio.send(m);
  EG_LOG("Air: sent seq %ld t=%s h=%s lux=%s bat=%s%s%s", (long)seq, m.has("t") ? m.get("t") : "-",
         m.has("h") ? m.get("h") : "-", m.has("lux") ? m.get("lux") : "-", m.get("bat"), m.has("err") ? " err=" : "",
         m.has("err") ? m.get("err") : "");
  sleepNow();
}

void loop() {}  // never reached: setup() always ends in deep sleep
