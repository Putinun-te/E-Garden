// Native unit tests for the Modbus RTU helpers: pio test -e native
#include <string.h>
#include <unity.h>

#include "EGardenModbusFrame.h"

using namespace eg::modbus;

void setUp() {}
void tearDown() {}

void test_requests_match_v1_bytes() {
  // The exact frames hard-coded in the v1 water sketch.
  const uint8_t v1PhTemp[8] = {0x05, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0x8F};
  const uint8_t v1Ec[8] = {0x0A, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD4, 0xB1};
  uint8_t req[8];
  buildReadRequest(PH_TEMP_SLAVE, PH_TEMP_FIRST, PH_TEMP_COUNT, req);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(v1PhTemp, req, 8);
  buildReadRequest(EC_SLAVE, EC_FIRST, EC_COUNT, req);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(v1Ec, req, 8);
}

static size_t makeResponse(uint8_t slave, const uint16_t* regs, uint16_t count, uint8_t* out) {
  out[0] = slave;
  out[1] = 0x03;
  out[2] = (uint8_t)(2 * count);
  for (uint16_t i = 0; i < count; i++) {
    out[3 + 2 * i] = (uint8_t)(regs[i] >> 8);
    out[4 + 2 * i] = (uint8_t)regs[i];
  }
  size_t n = 3 + 2 * count;
  uint16_t crc = crc16(out, n);
  out[n] = (uint8_t)crc;
  out[n + 1] = (uint8_t)(crc >> 8);
  return n + 2;
}

void test_parse_ph_temp() {
  uint16_t in[2] = {275, 65};  // 27.5 C, pH 6.5 (both x10, v1 scaling)
  uint8_t frame[16];
  size_t n = makeResponse(PH_TEMP_SLAVE, in, 2, frame);
  TEST_ASSERT_EQUAL(responseLength(2), n);
  uint16_t regs[2];
  TEST_ASSERT_EQUAL((int)FrameError::None, (int)parseReadResponse(frame, n, PH_TEMP_SLAVE, 2, regs));
  TEST_ASSERT_EQUAL(275, regs[0]);
  TEST_ASSERT_EQUAL(65, regs[1]);
}

void test_errors() {
  uint16_t in[1] = {16500};
  uint8_t frame[16];
  size_t n = makeResponse(EC_SLAVE, in, 1, frame);
  uint16_t regs[1];
  TEST_ASSERT_EQUAL((int)FrameError::Short, (int)parseReadResponse(frame, 0, EC_SLAVE, 1, regs));  // no answer
  TEST_ASSERT_EQUAL((int)FrameError::Short, (int)parseReadResponse(frame, n - 1, EC_SLAVE, 1, regs));
  TEST_ASSERT_EQUAL((int)FrameError::WrongSlave, (int)parseReadResponse(frame, n, PH_TEMP_SLAVE, 1, regs));
  frame[3] ^= 0x01;
  TEST_ASSERT_EQUAL((int)FrameError::BadCrc, (int)parseReadResponse(frame, n, EC_SLAVE, 1, regs));
  uint8_t ff[9];
  memset(ff, 0xFF, sizeof(ff));  // what v1 read from an empty buffer
  TEST_ASSERT_EQUAL((int)FrameError::BadCrc, (int)parseReadResponse(ff, 9, PH_TEMP_SLAVE, 2, regs));
}

void test_exception_response() {
  uint8_t ex[5] = {0x05, 0x83, 0x02, 0, 0};
  uint16_t crc = crc16(ex, 3);
  ex[3] = (uint8_t)crc;
  ex[4] = (uint8_t)(crc >> 8);
  uint16_t regs[2];
  TEST_ASSERT_EQUAL((int)FrameError::Exception, (int)parseReadResponse(ex, 5, PH_TEMP_SLAVE, 2, regs));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_requests_match_v1_bytes);
  RUN_TEST(test_parse_ph_temp);
  RUN_TEST(test_errors);
  RUN_TEST(test_exception_response);
  return UNITY_END();
}
