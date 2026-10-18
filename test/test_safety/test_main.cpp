// Native unit tests for the dosing/pump safety rules: pio test -e native
#include <unity.h>

#include "EGardenSafety.h"

using namespace eg::safety;

void setUp() {}
void tearDown() {}

static WaterReading good() {
  WaterReading r;
  r.hasPh = r.hasEc = r.hasTemp = true;
  r.ph = 6.5f;
  r.ec = 1500;
  r.temp = 26;
  return r;
}

void test_units_per_command() {
  DoseLimiter d;
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::A, 5, 0));
  TEST_ASSERT_EQUAL((int)Refusal::LimitCommand, (int)d.check(Channel::A, 6, 0));
  TEST_ASSERT_EQUAL((int)Refusal::BadCommand, (int)d.check(Channel::A, 0, 0));
}

void test_min_interval() {
  DoseLimiter d;
  d.record(Channel::B, 5, 1000);
  TEST_ASSERT_EQUAL((int)Refusal::TooSoon, (int)d.check(Channel::B, 1, 1000 + 19999));
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::B, 1, 1000 + 20000));
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::A, 1, 1001));  // channels are independent
}

void test_hourly_limit_rolls() {
  DoseLimiter d;
  uint32_t t = 0;
  for (int i = 0; i < 5; i++, t += 60000) {  // the v1 auto mode pattern: 5 x 5 units
    TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::A, 5, t));
    d.record(Channel::A, 5, t);
  }
  TEST_ASSERT_EQUAL(25, d.unitsInLastHour(Channel::A, t));
  TEST_ASSERT_EQUAL((int)Refusal::LimitHour, (int)d.check(Channel::A, 1, t));
  // The first dose (t=0) leaves the window one hour later.
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::A, 5, 3600000));
}

void test_wraparound() {
  DoseLimiter d;
  uint32_t t = 0xFFFFFFFFu - 5000;
  d.record(Channel::Acid, 5, t);
  TEST_ASSERT_EQUAL((int)Refusal::TooSoon, (int)d.check(Channel::Acid, 1, t + 10000));  // wrapped
  TEST_ASSERT_EQUAL(5, d.unitsInLastHour(Channel::Acid, t + 10000));
}

void test_restore_after_reboot_is_conservative() {
  DoseLimiter d;
  d.restoreConservative(Channel::A, 20, 0);
  TEST_ASSERT_EQUAL((int)Refusal::TooSoon, (int)d.check(Channel::A, 5, 0));
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)d.check(Channel::A, 5, 20000));
  d.record(Channel::A, 5, 20000);
  TEST_ASSERT_EQUAL((int)Refusal::LimitHour, (int)d.check(Channel::A, 1, 40000));
}

void test_reading_guard_no_reading_and_stale() {
  ReadingGuard g;
  TEST_ASSERT_EQUAL((int)Refusal::NoReading, (int)g.check(0));
  g.update(good(), 1000);
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)g.check(1000 + 600000));
  TEST_ASSERT_EQUAL((int)Refusal::Stale, (int)g.check(1000 + 600001));
}

void test_reading_guard_missing_field() {
  ReadingGuard g;
  WaterReading r = good();
  r.hasEc = false;  // EC read failed
  g.update(r, 0);
  TEST_ASSERT_EQUAL((int)Refusal::NoReading, (int)g.check(0));
}

void test_out_of_range_lockout_needs_three_good() {
  ReadingGuard g;
  WaterReading bad = good();
  bad.ph = 6553.5f;  // what v1 computed from a failed Modbus read (0xFFFF / 10)
  g.update(bad, 0);
  TEST_ASSERT_EQUAL((int)Refusal::OutOfRange, (int)g.check(0));
  g.update(good(), 1);
  g.update(good(), 2);
  TEST_ASSERT_EQUAL((int)Refusal::OutOfRange, (int)g.check(2));
  g.update(good(), 3);
  TEST_ASSERT_EQUAL((int)Refusal::None, (int)g.check(3));
}

void test_channel_names() {
  Channel c;
  TEST_ASSERT_TRUE(parseChannel("ACID", c));
  TEST_ASSERT_EQUAL((int)Channel::Acid, (int)c);
  TEST_ASSERT_FALSE(parseChannel("acid", c));
  TEST_ASSERT_FALSE(parseChannel(nullptr, c));
  TEST_ASSERT_EQUAL_STRING("B", channelName(Channel::B));
  TEST_ASSERT_EQUAL_STRING("limit_hour", refusalName(Refusal::LimitHour));
}

void test_pump_timer() {
  PumpTimer p(90000);
  p.switchOn(1000);
  p.switchOn(50000);  // repeated "on" does not extend the timer (v1 behaviour)
  TEST_ASSERT_FALSE(p.expired(90999));
  TEST_ASSERT_EQUAL(1, p.remainingMs(90999));
  TEST_ASSERT_TRUE(p.expired(91000));
  TEST_ASSERT_FALSE(p.on());
  TEST_ASSERT_FALSE(p.expired(200000));  // fires once
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_units_per_command);
  RUN_TEST(test_min_interval);
  RUN_TEST(test_hourly_limit_rolls);
  RUN_TEST(test_wraparound);
  RUN_TEST(test_restore_after_reboot_is_conservative);
  RUN_TEST(test_reading_guard_no_reading_and_stale);
  RUN_TEST(test_reading_guard_missing_field);
  RUN_TEST(test_out_of_range_lockout_needs_three_good);
  RUN_TEST(test_channel_names);
  RUN_TEST(test_pump_timer);
  return UNITY_END();
}
