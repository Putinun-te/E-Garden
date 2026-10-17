// E-Garden LoRa message protocol, version 2.
// Plain C++ with no Arduino dependency so it can be unit tested on a PC.
//
// Frame (ASCII, one LoRa packet):
//   EG2,<src>,<dst>,<seq>,<type>,<key=value;key=value...>*<crc>
//   src/dst : node address 0..255 (dst 0 = broadcast)
//   seq     : 0..65535, sender's counter; an ACK carries the acknowledged seq in "ack"
//   type    : AIR | WTR | CMD | ACK | STA
//   crc     : CRC-16/CCITT-FALSE of every byte before '*', 4 upper-case hex digits
// See docs/PROTOCOL.md for the message types and fields.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace eg {
namespace proto {

constexpr size_t MAX_FRAME_LEN = 200;  // well below the 255-byte LoRa payload limit
constexpr size_t MAX_FIELDS = 8;
constexpr size_t MAX_KEY_LEN = 7;
constexpr size_t MAX_VALUE_LEN = 15;
constexpr uint8_t BROADCAST = 0;

enum class MsgType : uint8_t { Air, Water, Command, Ack, Status, Unknown };

enum class ParseError : uint8_t {
  None,
  TooLong,      // frame longer than MAX_FRAME_LEN
  BadHeader,    // missing "EG2", bad address/seq, wrong number of header parts
  BadCrc,       // missing or wrong checksum
  BadField,     // malformed key=value pair or too many fields
  UnknownType,  // type is not one of AIR/WTR/CMD/ACK/STA
};

struct Field {
  char key[MAX_KEY_LEN + 1];
  char value[MAX_VALUE_LEN + 1];
};

struct Message {
  uint8_t src = 0;
  uint8_t dst = 0;
  uint16_t seq = 0;
  MsgType type = MsgType::Unknown;
  Field fields[MAX_FIELDS];
  uint8_t fieldCount = 0;

  void clear();
  // Setters return false if the key/value is invalid or there is no room.
  bool set(const char* key, const char* value);
  bool setInt(const char* key, long value);
  bool setFloat(const char* key, float value, int decimals = 2);  // NaN is rejected
  // Getters return nullptr / false when the field is missing or not a number.
  const char* get(const char* key) const;
  bool getInt(const char* key, long& out) const;
  bool getFloat(const char* key, float& out) const;
  bool has(const char* key) const { return get(key) != nullptr; }
};

uint16_t crc16(const uint8_t* data, size_t len);
const char* typeName(MsgType type);
const char* errorName(ParseError err);

// Writes a NUL-terminated frame into out. Returns its length, or 0 if it does not fit.
size_t encode(const Message& msg, char* out, size_t capacity);
ParseError decode(const char* frame, size_t len, Message& out);

}  // namespace proto
}  // namespace eg
