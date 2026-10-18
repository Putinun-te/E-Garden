#include "EGardenProtocol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace eg {
namespace proto {

namespace {

const char* const kTypeNames[] = {"AIR", "WTR", "CMD", "ACK", "STA"};

bool validKey(const char* key) {
  size_t n = strlen(key);
  if (n == 0 || n > MAX_KEY_LEN) return false;
  for (size_t i = 0; i < n; i++) {
    char c = key[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
  }
  return true;
}

bool validValue(const char* value) {
  size_t n = strlen(value);
  if (n == 0 || n > MAX_VALUE_LEN) return false;
  for (size_t i = 0; i < n; i++) {
    char c = value[i];
    if (c < 0x21 || c > 0x7E || c == ',' || c == ';' || c == '*' || c == '=') return false;
  }
  return true;
}

// Parses an unsigned decimal number in [begin, end) that must not exceed max.
bool parseUnsigned(const char* begin, const char* end, unsigned long max, unsigned long& out) {
  if (begin == end || end - begin > 5) return false;
  unsigned long v = 0;
  for (const char* p = begin; p < end; p++) {
    if (*p < '0' || *p > '9') return false;
    v = v * 10 + (unsigned long)(*p - '0');
  }
  if (v > max) return false;
  out = v;
  return true;
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

void Message::clear() {
  src = dst = 0;
  seq = 0;
  type = MsgType::Unknown;
  fieldCount = 0;
}

bool Message::set(const char* key, const char* value) {
  if (!validKey(key) || !validValue(value)) return false;
  for (uint8_t i = 0; i < fieldCount; i++) {
    if (strcmp(fields[i].key, key) == 0) {
      strcpy(fields[i].value, value);
      return true;
    }
  }
  if (fieldCount >= MAX_FIELDS) return false;
  strcpy(fields[fieldCount].key, key);
  strcpy(fields[fieldCount].value, value);
  fieldCount++;
  return true;
}

bool Message::setInt(const char* key, long value) {
  char buf[MAX_VALUE_LEN + 1];
  int n = snprintf(buf, sizeof(buf), "%ld", value);
  if (n <= 0 || (size_t)n > MAX_VALUE_LEN) return false;
  return set(key, buf);
}

bool Message::setFloat(const char* key, float value, int decimals) {
  if (isnan(value) || isinf(value)) return false;
  char buf[MAX_VALUE_LEN + 1];
  int n = snprintf(buf, sizeof(buf), "%.*f", decimals, (double)value);
  if (n <= 0 || (size_t)n > MAX_VALUE_LEN) return false;
  return set(key, buf);
}

const char* Message::get(const char* key) const {
  for (uint8_t i = 0; i < fieldCount; i++) {
    if (strcmp(fields[i].key, key) == 0) return fields[i].value;
  }
  return nullptr;
}

bool Message::getInt(const char* key, long& out) const {
  const char* v = get(key);
  if (!v) return false;
  char* end = nullptr;
  long r = strtol(v, &end, 10);
  if (end == v || *end != '\0') return false;
  out = r;
  return true;
}

bool Message::getFloat(const char* key, float& out) const {
  const char* v = get(key);
  if (!v) return false;
  char* end = nullptr;
  float r = strtof(v, &end);
  if (end == v || *end != '\0' || isnan(r) || isinf(r)) return false;
  out = r;
  return true;
}

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

const char* typeName(MsgType type) {
  uint8_t i = (uint8_t)type;
  return i < 5 ? kTypeNames[i] : "UNKNOWN";
}

const char* errorName(ParseError err) {
  switch (err) {
    case ParseError::None: return "none";
    case ParseError::TooLong: return "too_long";
    case ParseError::BadHeader: return "bad_header";
    case ParseError::BadCrc: return "bad_crc";
    case ParseError::BadField: return "bad_field";
    case ParseError::UnknownType: return "unknown_type";
  }
  return "?";
}

size_t encode(const Message& msg, char* out, size_t capacity) {
  if (msg.type == MsgType::Unknown || capacity == 0) return 0;
  size_t cap = capacity < MAX_FRAME_LEN + 1 ? capacity : MAX_FRAME_LEN + 1;
  int n = snprintf(out, cap, "EG2,%u,%u,%u,%s,", msg.src, msg.dst, msg.seq, typeName(msg.type));
  if (n < 0 || (size_t)n >= cap) return 0;
  size_t len = (size_t)n;
  for (uint8_t i = 0; i < msg.fieldCount; i++) {
    n = snprintf(out + len, cap - len, "%s%s=%s", i ? ";" : "", msg.fields[i].key, msg.fields[i].value);
    if (n < 0 || (size_t)n >= cap - len) return 0;
    len += (size_t)n;
  }
  uint16_t crc = crc16((const uint8_t*)out, len);
  n = snprintf(out + len, cap - len, "*%04X", crc);
  if (n < 0 || (size_t)n >= cap - len) return 0;
  return len + (size_t)n;
}

ParseError decode(const char* frame, size_t len, Message& out) {
  out.clear();
  if (len > MAX_FRAME_LEN) return ParseError::TooLong;
  if (len < 4 || strncmp(frame, "EG2,", 4) != 0) return ParseError::BadHeader;

  // Checksum: the last 5 characters are "*XXXX".
  if (len < 10 || frame[len - 5] != '*') return ParseError::BadCrc;
  uint16_t expected = 0;
  for (size_t i = len - 4; i < len; i++) {
    int d = hexDigit(frame[i]);
    if (d < 0) return ParseError::BadCrc;
    expected = (uint16_t)((expected << 4) | (uint16_t)d);
  }
  size_t bodyEnd = len - 5;  // index of '*'
  if (crc16((const uint8_t*)frame, bodyEnd) != expected) return ParseError::BadCrc;

  // Header: EG2,src,dst,seq,type,
  const char* p = frame + 4;
  const char* end = frame + bodyEnd;
  const char* parts[4];
  const char* partEnds[4];
  for (int i = 0; i < 4; i++) {
    const char* comma = (const char*)memchr(p, ',', (size_t)(end - p));
    if (!comma) return ParseError::BadHeader;
    parts[i] = p;
    partEnds[i] = comma;
    p = comma + 1;
  }
  unsigned long src, dst, seq;
  if (!parseUnsigned(parts[0], partEnds[0], 255, src) || !parseUnsigned(parts[1], partEnds[1], 255, dst) ||
      !parseUnsigned(parts[2], partEnds[2], 65535, seq)) {
    return ParseError::BadHeader;
  }
  MsgType type = MsgType::Unknown;
  size_t typeLen = (size_t)(partEnds[3] - parts[3]);
  for (uint8_t i = 0; i < 5; i++) {
    if (strlen(kTypeNames[i]) == typeLen && strncmp(parts[3], kTypeNames[i], typeLen) == 0) {
      type = (MsgType)i;
    }
  }
  if (type == MsgType::Unknown) return ParseError::UnknownType;
  out.src = (uint8_t)src;
  out.dst = (uint8_t)dst;
  out.seq = (uint16_t)seq;
  out.type = type;

  // Body: key=value;key=value (may be empty)
  while (p < end) {
    const char* semi = (const char*)memchr(p, ';', (size_t)(end - p));
    const char* pairEnd = semi ? semi : end;
    const char* eq = (const char*)memchr(p, '=', (size_t)(pairEnd - p));
    if (!eq) return ParseError::BadField;
    size_t klen = (size_t)(eq - p), vlen = (size_t)(pairEnd - eq - 1);
    if (klen == 0 || klen > MAX_KEY_LEN || vlen == 0 || vlen > MAX_VALUE_LEN) return ParseError::BadField;
    char key[MAX_KEY_LEN + 1], value[MAX_VALUE_LEN + 1];
    memcpy(key, p, klen);
    key[klen] = '\0';
    memcpy(value, eq + 1, vlen);
    value[vlen] = '\0';
    if (out.has(key) || !out.set(key, value)) return ParseError::BadField;
    if (!semi) break;
    p = semi + 1;
    if (p == end) return ParseError::BadField;  // trailing ';'
  }
  return ParseError::None;
}

}  // namespace proto
}  // namespace eg
