#include "Automation.h"

#include "HubState.h"

using namespace eg;

namespace hubapp {
namespace automation {

namespace {

long minuteKey(const struct tm& t) { return (long)t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min; }

// v1 sent "On" at HH:00 and "oFF" at HH:01 for HH = 8..17, repeating the
// command every second of that minute. Now it is sent once per minute.
void pumpSchedule(const struct tm& now) {
  static long lastKey = -1;
  long key = minuteKey(now);
  if (key == lastKey) return;
  lastKey = key;
  if (now.tm_hour < eg::hub::PUMP_SCHEDULE_FIRST_HOUR || now.tm_hour > eg::hub::PUMP_SCHEDULE_LAST_HOUR) return;
  if (now.tm_min != eg::hub::PUMP_SCHEDULE_ON_MINUTE && now.tm_min != eg::hub::PUMP_SCHEDULE_OFF_MINUTE) return;
  Command c;
  c.op = Op::Pump;
  c.on = now.tm_min == eg::hub::PUMP_SCHEDULE_ON_MINUTE;
  c.source = Source::Schedule;
  if (!queueCommand(c)) EG_LOG("Schedule: command queue full");
}

#ifdef ENABLE_AUTO_DOSING
// Automatic dosing from the v1 Wed_server_test_Auto sketch, as it was meant to
// work. Two v1 bugs are not reproduced (see the PR description):
// - v1 never toggled from A to B, so it resent "Add(A)" every second;
// - v1 only ran the pH phase after an EC phase.
// Every dose still goes through the hub and water-node safety checks.
enum class Phase : uint8_t { Idle, Ec, Ph };
Phase phase = Phase::Idle;
bool phPending = false;
bool nextIsA = true;
int count = 0;
uint32_t lastStepMs = 0;

void queueDose(safety::Channel ch) {
  Command c;
  c.op = Op::Dose;
  c.ch = ch;
  c.units = eg::hub::AUTO_DOSE_UNITS;
  c.source = Source::Auto;
  if (!queueCommand(c)) EG_LOG("Auto: command queue full");
}

void autoDosing(const struct tm& now, uint32_t nowMs) {
  static long lastKey = -1;
  float ph = 0, ec = 0;
  safety::Refusal readingState;
  {
    StateLock l;
    readingState = state.guard.check(nowMs);
    ph = state.water.r.ph;
    ec = state.water.r.ec;
  }

  long key = minuteKey(now);
  if (now.tm_hour == eg::hub::AUTO_DOSE_HOUR && now.tm_min == eg::hub::AUTO_DOSE_MINUTE && key != lastKey) {
    lastKey = key;
    if (readingState != safety::Refusal::None) {
      EG_LOG("Auto: skipped, water reading %s", safety::refusalName(readingState));
      return;
    }
    phPending = ph > eg::hub::AUTO_PH_START_ABOVE;
    count = 0;
    if (ec < eg::hub::AUTO_EC_START_BELOW) {
      phase = Phase::Ec;
      nextIsA = true;
      lastStepMs = nowMs - eg::hub::AUTO_EC_STEP_MS;  // first dose right away, as in v1
      EG_LOG("Auto: EC %.0f < %.0f, dosing A/B", ec, eg::hub::AUTO_EC_START_BELOW);
    } else if (phPending) {
      phase = Phase::Ph;
      lastStepMs = nowMs - eg::hub::AUTO_PH_STEP_MS;
      EG_LOG("Auto: pH %.2f > %.2f, dosing acid", ph, eg::hub::AUTO_PH_START_ABOVE);
    }
  }

  if (phase == Phase::Ec && nowMs - lastStepMs >= eg::hub::AUTO_EC_STEP_MS) {
    lastStepMs = nowMs;
    queueDose(nextIsA ? safety::Channel::A : safety::Channel::B);
    if (!nextIsA) {
      count++;
      if (count >= eg::hub::AUTO_MAX_ROUNDS || ec > eg::hub::AUTO_EC_STOP_ABOVE) {
        EG_LOG("Auto: EC phase done after %d rounds (EC %.0f)", count, ec);
        count = 0;
        phase = phPending ? Phase::Ph : Phase::Idle;
        lastStepMs = nowMs;  // v1 waited one pH interval before the first acid dose
      }
    }
    nextIsA = !nextIsA;
  } else if (phase == Phase::Ph && nowMs - lastStepMs >= eg::hub::AUTO_PH_STEP_MS) {
    lastStepMs = nowMs;
    if (ph < eg::hub::AUTO_PH_STOP_BELOW || count >= eg::hub::AUTO_MAX_ROUNDS) {
      EG_LOG("Auto: pH phase done (pH %.2f)", ph);
      phase = Phase::Idle;
      return;
    }
    queueDose(safety::Channel::Acid);
    count++;
  }
}
#endif  // ENABLE_AUTO_DOSING

}  // namespace

bool autoDosingCompiled() {
#ifdef ENABLE_AUTO_DOSING
  return true;
#else
  return false;
#endif
}

void tick(const struct tm& now, uint32_t nowMs) {
  pumpSchedule(now);
#ifdef ENABLE_AUTO_DOSING
  autoDosing(now, nowMs);
#else
  (void)nowMs;
#endif
}

}  // namespace automation
}  // namespace hubapp
