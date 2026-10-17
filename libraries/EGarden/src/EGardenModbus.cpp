#ifdef ARDUINO
#include "EGardenModbus.h"

namespace eg {

void ModbusProbe::begin(HardwareSerial& port, int rxPin, int txPin, long baud, int dePin, int rePin,
                        uint32_t settleMs) {
  port_ = &port;
  dePin_ = dePin;
  rePin_ = rePin;
  settleMs_ = settleMs;
  pinMode(dePin_, OUTPUT);
  pinMode(rePin_, OUTPUT);
  setTransmit(false);
  port_->begin(baud, SERIAL_8N1, rxPin, txPin);
}

void ModbusProbe::setTransmit(bool on) {
  // v1 drove DE and RE together: HIGH to transmit, LOW to receive.
  digitalWrite(dePin_, on ? HIGH : LOW);
  digitalWrite(rePin_, on ? HIGH : LOW);
}

bool ModbusProbe::start(uint8_t slave, uint16_t firstRegister, uint16_t count) {
  if (state_ != State::Idle || count == 0 || count > MAX_REGS) return false;
  slave_ = slave;
  first_ = firstRegister;
  count_ = count;
  len_ = 0;
  while (port_->available()) port_->read();  // drop stale bytes
  setTransmit(true);
  state_ = State::Settle;  // v1 waited 100 ms between enabling the driver and sending
  since_ = millis();
  return true;
}

bool ModbusProbe::poll() {
  uint32_t now = millis();
  switch (state_) {
    case State::Idle:
      return false;
    case State::Settle: {
      if (now - since_ < settleMs_) return false;
      uint8_t req[8];
      modbus::buildReadRequest(slave_, first_, count_, req);
      port_->write(req, sizeof(req));
      port_->flush();  // wait until the 8 bytes are on the wire (~8 ms at 9600 baud)
      setTransmit(false);
      state_ = State::WaitReply;
      since_ = now;
      return false;
    }
    case State::WaitReply: {
      size_t want = modbus::responseLength(count_);
      while (port_->available() && len_ < sizeof(buf_)) buf_[len_++] = (uint8_t)port_->read();
      bool exception = len_ >= 5 && buf_[1] == 0x83;
      if (len_ < want && !exception && now - since_ < REPLY_TIMEOUT_MS) return false;
      result_ = modbus::parseReadResponse(buf_, len_, slave_, count_, regs_);
      state_ = State::Idle;
      return true;
    }
  }
  return false;
}

}  // namespace eg
#endif  // ARDUINO
