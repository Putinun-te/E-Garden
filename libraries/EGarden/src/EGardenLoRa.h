// LoRa link for E-Garden nodes: protocol v2 frames, polling receive (no work
// in the radio interrupt), ACK + retry for commands, duplicate detection, and
// automatic re-initialisation when the radio is not found.
//
// The SX127x radio is half-duplex. send() blocks only for the packet's air
// time (about 0.1 s for a short frame at the library defaults); poll() then
// puts the radio back into receive mode.
#pragma once
#ifdef ARDUINO
#include <Arduino.h>

#include "EGardenProtocol.h"

namespace eg {

struct LoRaPins {
  int ss, rst, dio0;
};

class LoRaLink {
 public:
  static constexpr int MAX_PENDING = 4;
  static constexpr int MAX_SEEN = 8;
  static constexpr int MAX_RETRIES = 3;
  static constexpr uint32_t RETRY_BASE_MS = 1500;
  static constexpr uint32_t RETRY_JITTER_MS = 500;
  static constexpr uint32_t INIT_RETRY_MS = 5000;

  using MessageHandler = void (*)(const proto::Message& msg, int rssi, void* ctx);
  // delivered=false means no ACK after all retries; ack is null in that case.
  using AckHandler = void (*)(uint16_t seq, bool delivered, const proto::Message* ack, void* ctx);

  // Tries to start the radio up to `attempts` times. If it still fails, poll()
  // keeps retrying every INIT_RETRY_MS. Returns true when the radio is up.
  bool begin(const LoRaPins& pins, uint8_t address, int attempts = 1);
  void poll();
  bool ready() const { return ready_; }
  uint8_t address() const { return address_; }

  void onMessage(MessageHandler fn, void* ctx = nullptr) {
    handler_ = fn;
    handlerCtx_ = ctx;
  }

  // Fills in src and seq, then transmits once. Returns the seq used, or -1.
  int32_t send(proto::Message& msg);
  // Like send(), but resends until the destination ACKs it. Returns seq or -1
  // when the radio is down or MAX_PENDING commands are already in flight.
  int32_t sendReliable(proto::Message& msg, AckHandler cb = nullptr, void* ctx = nullptr);

  // For receivers of CMD frames. Returns true if (src, seq) was already
  // handled; the stored ACK is sent again and the caller must not act on it.
  bool replayIfDuplicate(const proto::Message& cmd);
  // Sends an ACK for cmd and remembers it for duplicate replies.
  void ack(const proto::Message& cmd, bool ok, const char* err = nullptr);

  // Counters for diagnostics.
  uint32_t rxOk = 0, rxBad = 0, txCount = 0, retries = 0, undelivered = 0;
  int lastRssi = 0;

 private:
  struct Pending {
    bool used = false;
    uint16_t seq = 0;
    uint8_t dst = 0;
    uint8_t tries = 0;
    uint32_t nextMs = 0;
    char frame[proto::MAX_FRAME_LEN + 1];
    size_t len = 0;
    AckHandler cb = nullptr;
    void* ctx = nullptr;
  };
  struct Seen {
    bool used = false;
    uint8_t src = 0;
    uint16_t seq = 0;
    bool ok = false;
    char err[16] = {0};
  };

  bool tryInit();
  bool transmit(const char* frame, size_t len);
  void handleFrame(const char* frame, size_t len, int rssi);
  void resendDue(uint32_t now);
  void sendAck(uint8_t dst, uint16_t ackSeq, bool ok, const char* err);

  LoRaPins pins_{};
  uint8_t address_ = 0;
  bool ready_ = false;
  bool started_ = false;
  uint32_t nextInitMs_ = 0;
  uint16_t seq_ = 0;
  Pending pending_[MAX_PENDING];
  Seen seen_[MAX_SEEN];
  int seenNext_ = 0;
  MessageHandler handler_ = nullptr;
  void* handlerCtx_ = nullptr;
};

}  // namespace eg
#endif  // ARDUINO
