// Dosing and pump safety rules. Plain C++ (no Arduino dependency) so the rules
// are unit tested on a PC. Times are millis()-style uint32_t values; all
// comparisons use unsigned subtraction so they survive the 49-day wraparound.
#pragma once
#include <stdint.h>

#include "EGardenConfig.h"

namespace eg {
namespace safety {

enum class Channel : uint8_t { A = 0, B = 1, Acid = 2 };
constexpr int CHANNEL_COUNT = 3;

enum class Refusal : uint8_t {
  None,
  BadCommand,   // unknown channel, units <= 0
  LimitCommand, // more than MAX_UNITS_PER_COMMAND
  LimitHour,    // would exceed MAX_UNITS_PER_CHANNEL_PER_HOUR in the last 60 min
  TooSoon,      // less than MIN_DOSE_INTERVAL_MS since the last dose on this channel
  NoReading,    // no pH/EC reading received yet
  Stale,        // latest reading older than READING_MAX_AGE_MS
  OutOfRange,   // latest reading outside the valid sensor range, or lockout not cleared yet
  Busy,         // a dose is already running
};

const char* refusalName(Refusal r);    // matches the ACK err codes in docs/PROTOCOL.md
const char* channelName(Channel c);    // "A", "B", "ACID"
bool parseChannel(const char* s, Channel& out);

struct WaterReading {
  bool hasPh = false, hasEc = false, hasTemp = false;
  float ph = 0, ec = 0, temp = 0;
};

bool inRange(const WaterReading& r);  // every present value within its valid range

// Tracks the latest water reading and locks dosing out after an out-of-range
// reading until GOOD_READINGS_TO_UNLOCK valid readings in a row have arrived.
class ReadingGuard {
 public:
  static constexpr int GOOD_READINGS_TO_UNLOCK = 3;
  void update(const WaterReading& r, uint32_t nowMs);
  Refusal check(uint32_t nowMs) const;
  bool locked() const { return goodNeeded_ > 0; }
  const WaterReading& latest() const { return latest_; }
  bool hasReading() const { return received_; }

 private:
  WaterReading latest_;
  uint32_t atMs_ = 0;
  bool received_ = false;
  int goodNeeded_ = 0;
};

// Per-channel dose accounting: units per command, per rolling hour, and
// minimum spacing between doses.
class DoseLimiter {
 public:
  static constexpr int MAX_EVENTS = 32;  // >= max doses that fit in one hour
  Refusal check(Channel ch, int units, uint32_t nowMs) const;
  void record(Channel ch, int units, uint32_t nowMs);
  int unitsInLastHour(Channel ch, uint32_t nowMs) const;
  // After a reboot millis() restarts, so units remembered from before the
  // reboot are counted as if they were dosed right now (the safe direction).
  void restoreConservative(Channel ch, int units, uint32_t nowMs);

 private:
  struct Event {
    uint32_t atMs;
    int16_t units;
  };
  struct Log {
    Event events[MAX_EVENTS];
    int count = 0;
    bool any = false;
    uint32_t lastMs = 0;
  };
  Log logs_[CHANNEL_COUNT];
};

// Pump node auto-off: once switched on, the pump must turn off after maxOnMs
// even if the "off" command is lost.
class PumpTimer {
 public:
  explicit PumpTimer(uint32_t maxOnMs = pump::MAX_ON_MS) : maxOnMs_(maxOnMs) {}
  void switchOn(uint32_t nowMs);
  void switchOff() { on_ = false; }
  // Returns true exactly when the timer forces the pump off.
  bool expired(uint32_t nowMs);
  bool on() const { return on_; }
  uint32_t remainingMs(uint32_t nowMs) const;

 private:
  uint32_t maxOnMs_;
  uint32_t startMs_ = 0;
  bool on_ = false;
};

}  // namespace safety
}  // namespace eg
