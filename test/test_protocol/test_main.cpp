// Native unit tests for the v2 message protocol: pio test -e native
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "EGardenProtocol.h"

using namespace eg::proto;

void setUp() {}
void tearDown() {}

static Message waterReading() {
  Message m;
  m.src = 2;
  m.dst = 1;
  m.seq = 42;
  m.type = MsgType::Water;
  m.setFloat("ph", 6.1f, 1);
  m.setInt("ec", 1650);
  m.setFloat("t", 27.5f, 1);
  return m;
}

void test_crc16_known_vector() {
  // CRC-16/CCITT-FALSE check value for "123456789"
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16((const uint8_t*)"123456789", 9));
}

void test_encode_format() {
  char buf[MAX_FRAME_LEN + 1];
  size_t n = encode(waterReading(), buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_STRING_LEN("EG2,2,1,42,WTR,ph=6.1;ec=1650;t=27.5*", buf, n - 4);
  TEST_ASSERT_EQUAL(strlen(buf), n);
}

void test_roundtrip() {
  char buf[MAX_FRAME_LEN + 1];
  size_t n = encode(waterReading(), buf, sizeof(buf));
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::None, (int)decode(buf, n, m));
  TEST_ASSERT_EQUAL(2, m.src);
  TEST_ASSERT_EQUAL(1, m.dst);
  TEST_ASSERT_EQUAL(42, m.seq);
  TEST_ASSERT_EQUAL((int)MsgType::Water, (int)m.type);
  float ph;
  long ec;
  TEST_ASSERT_TRUE(m.getFloat("ph", ph));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.1f, ph);
  TEST_ASSERT_TRUE(m.getInt("ec", ec));
  TEST_ASSERT_EQUAL(1650, ec);
  TEST_ASSERT_NULL(m.get("bat"));
}

void test_empty_body() {
  Message m;
  m.src = 1;
  m.dst = 4;
  m.seq = 7;
  m.type = MsgType::Status;
  char buf[MAX_FRAME_LEN + 1];
  size_t n = encode(m, buf, sizeof(buf));
  Message out;
  TEST_ASSERT_EQUAL((int)ParseError::None, (int)decode(buf, n, out));
  TEST_ASSERT_EQUAL(0, out.fieldCount);
}

void test_corrupted_byte_fails_crc() {
  char buf[MAX_FRAME_LEN + 1];
  size_t n = encode(waterReading(), buf, sizeof(buf));
  buf[16] = (buf[16] == '6') ? '9' : '6';  // flip a digit of the pH value
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::BadCrc, (int)decode(buf, n, m));
}

void test_v1_frame_is_rejected() {
  const char* v1 = "3@2/6.50&1650$27.50#";
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::BadHeader, (int)decode(v1, strlen(v1), m));
}

void test_unknown_type() {
  char frame[64] = "EG2,1,2,3,XYZ,a=1";
  size_t len = strlen(frame);
  snprintf(frame + len, sizeof(frame) - len, "*%04X", crc16((const uint8_t*)frame, len));
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::UnknownType, (int)decode(frame, strlen(frame), m));
}

void test_bad_address_range() {
  char frame[64] = "EG2,300,2,3,CMD,op=stop";
  size_t len = strlen(frame);
  snprintf(frame + len, sizeof(frame) - len, "*%04X", crc16((const uint8_t*)frame, len));
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::BadHeader, (int)decode(frame, strlen(frame), m));
}

void test_duplicate_and_malformed_fields() {
  const char* bodies[] = {"EG2,1,2,3,CMD,op=a;op=b", "EG2,1,2,3,CMD,op", "EG2,1,2,3,CMD,op=a;", "EG2,1,2,3,CMD,=a"};
  for (const char* body : bodies) {
    char frame[64];
    strcpy(frame, body);
    size_t len = strlen(frame);
    snprintf(frame + len, sizeof(frame) - len, "*%04X", crc16((const uint8_t*)frame, len));
    Message m;
    TEST_ASSERT_EQUAL_MESSAGE((int)ParseError::BadField, (int)decode(frame, strlen(frame), m), body);
  }
}

void test_setters_reject_bad_input() {
  Message m;
  TEST_ASSERT_FALSE(m.set("Bad", "1"));          // upper case key
  TEST_ASSERT_FALSE(m.set("toolongkey", "1"));   // > 7 chars
  TEST_ASSERT_FALSE(m.set("k", "a;b"));          // separator in value
  TEST_ASSERT_FALSE(m.setFloat("k", NAN));       // failed sensor reading
  for (int i = 0; i < (int)MAX_FIELDS; i++) {
    char key[4];
    snprintf(key, sizeof(key), "k%d", i);
    TEST_ASSERT_TRUE(m.setInt(key, i));
  }
  TEST_ASSERT_FALSE(m.setInt("extra", 1));      // no room
  TEST_ASSERT_TRUE(m.setInt("k0", 9));          // overwrite existing key still works
}

void test_too_long_and_short_buffers() {
  char big[MAX_FRAME_LEN + 10];
  memset(big, 'A', sizeof(big));
  Message m;
  TEST_ASSERT_EQUAL((int)ParseError::TooLong, (int)decode(big, sizeof(big), m));
  char small[10];
  TEST_ASSERT_EQUAL(0, encode(waterReading(), small, sizeof(small)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc16_known_vector);
  RUN_TEST(test_encode_format);
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_empty_body);
  RUN_TEST(test_corrupted_byte_fails_crc);
  RUN_TEST(test_v1_frame_is_rejected);
  RUN_TEST(test_unknown_type);
  RUN_TEST(test_bad_address_range);
  RUN_TEST(test_duplicate_and_malformed_fields);
  RUN_TEST(test_setters_reject_bad_input);
  RUN_TEST(test_too_long_and_short_buffers);
  return UNITY_END();
}
