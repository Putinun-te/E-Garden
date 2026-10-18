#ifdef ARDUINO
#include "EGardenLoRa.h"

#include <LoRa.h>
#include <SPI.h>
#include <string.h>

#include "EGardenConfig.h"
#include "EGardenLog.h"

namespace eg {

bool LoRaLink::begin(const LoRaPins& pins, uint8_t address, int attempts) {
  pins_ = pins;
  address_ = address;
  started_ = true;
  seq_ = (uint16_t)esp_random();  // a reboot should not reuse recent sequence numbers
  for (int i = 0; i < attempts; i++) {
    if (tryInit()) return true;
    if (i + 1 < attempts) delay(LORA_INIT_RETRY_MS);  // only used by the air node before deep sleep
  }
  EG_LOG("LoRa: radio not found, retrying every %lus", (unsigned long)(INIT_RETRY_MS / 1000));
  nextInitMs_ = millis() + INIT_RETRY_MS;
  return false;
}

bool LoRaLink::tryInit() {
  LoRa.setPins(pins_.ss, pins_.rst, pins_.dio0);
  if (!LoRa.begin(LORA_FREQUENCY_HZ)) {
    ready_ = false;
    return false;
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();  // radio-level CRC in addition to the frame CRC
  ready_ = true;
  EG_LOG("LoRa: ready, address %u, sync 0x%02X", address_, LORA_SYNC_WORD);
  return true;
}

void LoRaLink::poll() {
  uint32_t now = millis();
  if (!ready_) {
    if (started_ && (int32_t)(now - nextInitMs_) >= 0) {
      if (!tryInit()) nextInitMs_ = now + INIT_RETRY_MS;
    }
    return;
  }
  int size = LoRa.parsePacket();  // polling: also (re)arms receive mode
  if (size > 0) {
    char buf[proto::MAX_FRAME_LEN + 1];
    size_t len = 0;
    while (LoRa.available()) {
      int c = LoRa.read();
      if (len < proto::MAX_FRAME_LEN) buf[len] = (char)c;
      len++;
    }
    int rssi = LoRa.packetRssi();
    lastRssi = rssi;
    if (len > proto::MAX_FRAME_LEN) {
      rxBad++;
      EG_LOG("LoRa: dropped %u-byte packet (too long)", (unsigned)len);
    } else {
      buf[len] = '\0';
      handleFrame(buf, len, rssi);
    }
  }
  resendDue(now);
}

void LoRaLink::handleFrame(const char* frame, size_t len, int rssi) {
  proto::Message msg;
  proto::ParseError err = proto::decode(frame, len, msg);
  if (err != proto::ParseError::None) {
    rxBad++;
    EG_LOG("LoRa: dropped frame (%s), rssi %d", proto::errorName(err), rssi);
    return;
  }
  if (msg.dst != address_ && msg.dst != proto::BROADCAST) return;  // not for us
  rxOk++;
  if (msg.type == proto::MsgType::Ack) {
    long acked;
    if (msg.getInt("ack", acked)) {
      for (Pending& p : pending_) {
        if (p.used && p.seq == (uint16_t)acked && p.dst == msg.src) {
          p.used = false;
          if (p.cb) p.cb(p.seq, true, &msg, p.ctx);
        }
      }
    }
  }
  if (handler_) handler_(msg, rssi, handlerCtx_);
}

bool LoRaLink::transmit(const char* frame, size_t len) {
  if (!ready_) return false;
  if (!LoRa.beginPacket()) return false;
  LoRa.write((const uint8_t*)frame, len);
  bool ok = LoRa.endPacket() == 1;  // blocks for the air time only
  if (ok) txCount++;
  return ok;
}

int32_t LoRaLink::send(proto::Message& msg) {
  msg.src = address_;
  msg.seq = ++seq_;
  char frame[proto::MAX_FRAME_LEN + 1];
  size_t len = proto::encode(msg, frame, sizeof(frame));
  if (len == 0) {
    EG_LOG("LoRa: message did not fit in a frame");
    return -1;
  }
  return transmit(frame, len) ? msg.seq : -1;
}

int32_t LoRaLink::sendReliable(proto::Message& msg, AckHandler cb, void* ctx) {
  if (!ready_) return -1;
  Pending* slot = nullptr;
  for (Pending& p : pending_) {
    if (!p.used) {
      slot = &p;
      break;
    }
  }
  if (!slot) return -1;
  msg.src = address_;
  msg.seq = ++seq_;
  slot->len = proto::encode(msg, slot->frame, sizeof(slot->frame));
  if (slot->len == 0) return -1;
  slot->used = true;
  slot->seq = msg.seq;
  slot->dst = msg.dst;
  slot->tries = 1;
  slot->cb = cb;
  slot->ctx = ctx;
  slot->nextMs = millis() + RETRY_BASE_MS + (esp_random() % RETRY_JITTER_MS);
  transmit(slot->frame, slot->len);
  return msg.seq;
}

void LoRaLink::resendDue(uint32_t now) {
  for (Pending& p : pending_) {
    if (!p.used || (int32_t)(now - p.nextMs) < 0) continue;
    if (p.tries > MAX_RETRIES) {
      p.used = false;
      undelivered++;
      EG_LOG("LoRa: no ACK from %u for seq %u", p.dst, p.seq);
      if (p.cb) p.cb(p.seq, false, nullptr, p.ctx);
      continue;
    }
    p.tries++;
    retries++;
    p.nextMs = now + RETRY_BASE_MS + (esp_random() % RETRY_JITTER_MS);
    transmit(p.frame, p.len);
  }
}

bool LoRaLink::replayIfDuplicate(const proto::Message& cmd) {
  for (Seen& s : seen_) {
    if (s.used && s.src == cmd.src && s.seq == cmd.seq) {
      EG_LOG("LoRa: duplicate seq %u from %u, ACK resent", cmd.seq, cmd.src);
      sendAck(cmd.src, cmd.seq, s.ok, s.err[0] ? s.err : nullptr);
      return true;
    }
  }
  return false;
}

void LoRaLink::ack(const proto::Message& cmd, bool ok, const char* err) {
  Seen* slot = nullptr;
  for (Seen& s : seen_) {
    if (s.used && s.src == cmd.src) slot = &s;  // one entry per sender
  }
  if (!slot) {
    slot = &seen_[seenNext_];
    seenNext_ = (seenNext_ + 1) % MAX_SEEN;
  }
  slot->used = true;
  slot->src = cmd.src;
  slot->seq = cmd.seq;
  slot->ok = ok;
  strncpy(slot->err, err ? err : "", sizeof(slot->err) - 1);
  slot->err[sizeof(slot->err) - 1] = '\0';
  sendAck(cmd.src, cmd.seq, ok, err);
}

void LoRaLink::sendAck(uint8_t dst, uint16_t ackSeq, bool ok, const char* err) {
  proto::Message a;
  a.type = proto::MsgType::Ack;
  a.dst = dst;
  a.setInt("ack", ackSeq);
  a.setInt("ok", ok ? 1 : 0);
  if (err) a.set("err", err);
  send(a);
}

}  // namespace eg
#endif  // ARDUINO
