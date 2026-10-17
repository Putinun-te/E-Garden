// Central configuration for every E-Garden node.
// Values marked "v1" are copied from the original sketches (tag v1-original)
// so the hardware behaves the same unless a change was explicitly approved.
#pragma once
#include <stdint.h>

namespace eg {

// ---------------------------------------------------------------- LoRa link
constexpr long    LORA_FREQUENCY_HZ = 433E6;  // v1
constexpr uint8_t LORA_SYNC_WORD    = 0xAB;   // v1 hub/water/pump (air used 0xAA in v1, changed: approved A)
constexpr int     LORA_INIT_ATTEMPTS = 30;    // v1: 30 tries, 500 ms apart
constexpr unsigned long LORA_INIT_RETRY_MS = 500;

// Node addresses (v1 used the strings "1", "2", "3" and "123" for the pump)
constexpr uint8_t ADDR_HUB   = 1;
constexpr uint8_t ADDR_WATER = 2;
constexpr uint8_t ADDR_AIR   = 3;  // v1 air sketch sent "2" (collided with water), changed: approved A
constexpr uint8_t ADDR_PUMP  = 4;  // v1 pump used "123"; numeric address for protocol v2

// ---------------------------------------------------------------- Hub (Master node)
namespace hub {
constexpr int PIN_LORA_SS   = 5;   // v1
constexpr int PIN_LORA_RST  = 15;  // v1
constexpr int PIN_LORA_DIO0 = 2;   // v1
constexpr int PIN_OUTPUT1   = 27;  // v1: configured as output, not otherwise used
constexpr uint16_t WEB_PORT = 8888;  // v1

constexpr const char* NTP_SERVER = "pool.ntp.org";  // v1
constexpr long NTP_GMT_OFFSET_SEC = 3600L * 7;      // v1 (Thailand, UTC+7)
// v1 value. Thailand has no DST; with a non-zero value arduino-esp32 builds a
// TZ string with a DST part, which may shift the clock. Kept until approved.
constexpr int  NTP_DAYLIGHT_OFFSET_SEC = 3600 * 7;

// v1 schedule: water pump ON at HH:00 and OFF at HH:01, for HH = 8..17
constexpr int PUMP_SCHEDULE_FIRST_HOUR = 8;
constexpr int PUMP_SCHEDULE_LAST_HOUR  = 17;
constexpr int PUMP_SCHEDULE_ON_MINUTE  = 0;
constexpr int PUMP_SCHEDULE_OFF_MINUTE = 1;
constexpr unsigned long CLOCK_TICK_MS = 1000;  // v1: time checked once per second

// v1 auto dosing (Wed_server_test_Auto), compiled in only with ENABLE_AUTO_DOSING
constexpr int   AUTO_DOSE_HOUR = 7;
constexpr int   AUTO_DOSE_MINUTE = 0;
constexpr float AUTO_PH_START_ABOVE = 6.2f;   // start acid dosing when pH > 6.2
constexpr float AUTO_PH_STOP_BELOW  = 6.0f;   // stop when pH < 6.0
constexpr float AUTO_EC_START_BELOW = 1600;   // start A/B dosing when EC < 1600
constexpr float AUTO_EC_STOP_ABOVE  = 1700;   // stop when EC > 1700
constexpr int   AUTO_DOSE_UNITS     = 5;      // units per Add() command
constexpr int   AUTO_MAX_ROUNDS     = 5;
constexpr unsigned long AUTO_EC_STEP_MS = 30000;  // A/B alternate every 30 s
constexpr unsigned long AUTO_PH_STEP_MS = 20000;  // acid every 20 s
}  // namespace hub

// ---------------------------------------------------------------- Water sensor node
namespace water {
constexpr int PIN_LORA_SS   = 5;   // v1
constexpr int PIN_LORA_RST  = 15;  // v1
constexpr int PIN_LORA_DIO0 = 17;  // v1
constexpr int PIN_RS485_RE  = 32;  // v1
constexpr int PIN_RS485_DE  = 33;  // v1
constexpr int PIN_RS485_RX  = 26;  // v1 (SoftwareSerial mod(26, 27))
constexpr int PIN_RS485_TX  = 27;  // v1
constexpr long RS485_BAUD   = 9600;  // v1

// Dosing pumps: stepper drivers sharing one DIR and one EN line.
// Mapping is the v1 behaviour: Add(A) pulses the pin v1 called Pump_C, and
// Add(Acid) pulses the pin v1 called Pump_A.
constexpr int PIN_DOSE_STEP_A    = 0;   // v1 "Pump_C"
constexpr int PIN_DOSE_STEP_B    = 2;   // v1 "Pump_B"
constexpr int PIN_DOSE_STEP_ACID = 21;  // v1 "Pump_A"
constexpr int PIN_DOSE_DIR       = 16;  // v1 X_DIR
constexpr int PIN_DOSE_EN        = 4;   // v1 X_EN (LOW = enabled)
constexpr long STEPS_PER_UNIT_A    = 4100;  // v1
constexpr long STEPS_PER_UNIT_B    = 3900;  // v1
constexpr long STEPS_PER_UNIT_ACID = 4050;  // v1
constexpr unsigned STEP_HALF_PERIOD_US = 150;  // v1: 150 us high, 150 us low
constexpr bool DOSE_DIRECTION = false;          // v1: step(false, ...)

constexpr unsigned long MEASURE_INTERVAL_MS = 30000;  // v1
constexpr unsigned long MODBUS_SETTLE_MS = 100;       // v1 delay before sending a request
}  // namespace water

// ---------------------------------------------------------------- Air sensor node
namespace air {
constexpr int PIN_LORA_SS   = 5;   // v1
constexpr int PIN_LORA_RST  = 15;  // v1
constexpr int PIN_LORA_DIO0 = 2;   // v1
constexpr int PIN_BATTERY   = 36;  // v1 ADC
constexpr uint64_t SLEEP_SECONDS = 10;  // v1 deep sleep between readings
constexpr int   BATTERY_SAMPLES = 20;   // v1
constexpr unsigned long BATTERY_SAMPLE_GAP_MS = 50;  // v1
constexpr float BATTERY_FULL_VOLTS = 4.2f;           // v1
}  // namespace air

// ---------------------------------------------------------------- Pump node
namespace pump {
constexpr int PIN_LORA_SS   = 5;   // v1
constexpr int PIN_LORA_RST  = 14;  // v1
constexpr int PIN_LORA_DIO0 = 2;   // v1
constexpr int PIN_RELAY     = 26;  // v1 "IED"
constexpr unsigned long MAX_ON_MS = 90000;  // v1 auto-off timer
}  // namespace pump

// ---------------------------------------------------------------- Dosing safety (approved B)
namespace safety {
constexpr int   MAX_UNITS_PER_COMMAND = 5;
constexpr int   MAX_UNITS_PER_CHANNEL_PER_HOUR = 25;
constexpr unsigned long MIN_DOSE_INTERVAL_MS = 20000;
constexpr float PH_MIN = 0.0f,  PH_MAX = 14.0f;
constexpr float EC_MIN = 0.0f,  EC_MAX = 5000.0f;
constexpr float WATER_TEMP_MIN = 0.0f, WATER_TEMP_MAX = 50.0f;
constexpr unsigned long READING_MAX_AGE_MS = 10UL * 60 * 1000;
}  // namespace safety

}  // namespace eg
