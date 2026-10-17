#include "EGardenSafety.h"

#include <string.h>

namespace eg {
namespace safety {

namespace {
constexpr uint32_t HOUR_MS = 60UL * 60 * 1000;
}

const char* refusalName(Refusal r) {
  switch (r) {
    case Refusal::None: return "ok";
    case Refusal::BadCommand: return "bad_cmd";
    case Refusal::LimitCommand: return "limit_cmd";
    case Refusal::LimitHour: return "limit_hour";
    case Refusal::TooSoon: return "too_soon";
    case Refusal::NoReading: return "no_reading";
    case Refusal::Stale: return "stale";
    case Refusal::OutOfRange: return "out_of_range";
    case Refusal::Busy: return "busy";
  }
  return "?";
}

const char* channelName(Channel c) {
  switch (c) {
    case Channel::A: return "A";
    case Channel::B: return "B";
    case Channel::Acid: return "ACID";
  }
  return "?";
}

bool parseChannel(const char* s, Channel& out) {
  if (!s) return false;
  if (strcmp(s, "A") == 0) { out = Channel::A; return true; }
  if (strcmp(s, "B") == 0) { out = Channel::B; return true; }
  if (strcmp(s, "ACID") == 0) { out = Channel::Acid; return true; }
  return false;
}

bool inRange(const WaterReading& r) {
  if (r.hasPh && (r.ph < PH_MIN || r.ph > PH_MAX)) return false;
  if (r.hasEc && (r.ec < EC_MIN || r.ec > EC_MAX)) return false;
  if (r.hasTemp && (r.temp < WATER_TEMP_MIN || r.temp > WATER_TEMP_MAX)) return false;
  return true;
}

void ReadingGuard::update(const WaterReading& r, uint32_t nowMs) {
  latest_ = r;
  atMs_ = nowMs;
  received_ = true;
  bool good = r.hasPh && r.hasEc && inRange(r);
  if (!good) {
    goodNeeded_ = GOOD_READINGS_TO_UNLOCK;
  } else if (goodNeeded_ > 0) {
    goodNeeded_--;
  }
}

Refusal ReadingGuard::check(uint32_t nowMs) const {
  if (!received_) return Refusal::NoReading;
  if ((uint32_t)(nowMs - atMs_) > READING_MAX_AGE_MS) return Refusal::Stale;
  if (!latest_.hasPh || !latest_.hasEc) return Refusal::NoReading;
  if (goodNeeded_ > 0 || !inRange(latest_)) return Refusal::OutOfRange;
  return Refusal::None;
}

Refusal DoseLimiter::check(Channel ch, int units, uint32_t nowMs) const {
  int idx = (int)ch;
  if (idx < 0 || idx >= CHANNEL_COUNT || units <= 0) return Refusal::BadCommand;
  if (units > MAX_UNITS_PER_COMMAND) return Refusal::LimitCommand;
  const Log& log = logs_[idx];
  if (log.any && (uint32_t)(nowMs - log.lastMs) < MIN_DOSE_INTERVAL_MS) return Refusal::TooSoon;
  if (unitsInLastHour(ch, nowMs) + units > MAX_UNITS_PER_CHANNEL_PER_HOUR) return Refusal::LimitHour;
  return Refusal::None;
}

void DoseLimiter::record(Channel ch, int units, uint32_t nowMs) {
  Log& log = logs_[(int)ch];
  // Drop events older than an hour, then append (the oldest is dropped if full).
  int kept = 0;
  for (int i = 0; i < log.count; i++) {
    if ((uint32_t)(nowMs - log.events[i].atMs) < HOUR_MS) log.events[kept++] = log.events[i];
  }
  log.count = kept;
  if (log.count == MAX_EVENTS) {
    for (int i = 1; i < MAX_EVENTS; i++) log.events[i - 1] = log.events[i];
    log.count--;
  }
  log.events[log.count++] = {nowMs, (int16_t)units};
  log.any = true;
  log.lastMs = nowMs;
}

int DoseLimiter::unitsInLastHour(Channel ch, uint32_t nowMs) const {
  const Log& log = logs_[(int)ch];
  int sum = 0;
  for (int i = 0; i < log.count; i++) {
    if ((uint32_t)(nowMs - log.events[i].atMs) < HOUR_MS) sum += log.events[i].units;
  }
  return sum;
}

void DoseLimiter::restoreConservative(Channel ch, int units, uint32_t nowMs) {
  if (units <= 0) return;
  if (units > MAX_UNITS_PER_CHANNEL_PER_HOUR) units = MAX_UNITS_PER_CHANNEL_PER_HOUR;
  record(ch, units, nowMs);
}

void PumpTimer::switchOn(uint32_t nowMs) {
  // v1 behaviour: the timer starts when the pump first turns on; repeated
  // "on" commands while running do not extend it.
  if (!on_) startMs_ = nowMs;
  on_ = true;
}

bool PumpTimer::expired(uint32_t nowMs) {
  if (on_ && (uint32_t)(nowMs - startMs_) >= maxOnMs_) {
    on_ = false;
    return true;
  }
  return false;
}

uint32_t PumpTimer::remainingMs(uint32_t nowMs) const {
  if (!on_) return 0;
  uint32_t elapsed = nowMs - startMs_;
  return elapsed >= maxOnMs_ ? 0 : maxOnMs_ - elapsed;
}

}  // namespace safety
}  // namespace eg
