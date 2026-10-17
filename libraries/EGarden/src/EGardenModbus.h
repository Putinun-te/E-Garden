// Non-blocking Modbus RTU reader for the RS485 pH/EC probes, on a hardware UART.
// Replaces v1's SoftwareSerial code, which read the reply before it arrived
// and only worked because each probe was read twice.
#pragma once
#ifdef ARDUINO
#include <Arduino.h>

#include "EGardenModbusFrame.h"

namespace eg {

class ModbusProbe {
 public:
  static constexpr uint32_t REPLY_TIMEOUT_MS = 300;
  static constexpr uint16_t MAX_REGS = 4;

  void begin(HardwareSerial& port, int rxPin, int txPin, long baud, int dePin, int rePin, uint32_t settleMs);
  // Starts a read; returns false if a read is already running.
  bool start(uint8_t slave, uint16_t firstRegister, uint16_t count);
  // Call every loop. Returns true once, when the read has finished.
  bool poll();
  bool busy() const { return state_ != State::Idle; }
  modbus::FrameError result() const { return result_; }
  uint16_t reg(int i) const { return regs_[i]; }

 private:
  enum class State : uint8_t { Idle, Settle, WaitReply };
  void setTransmit(bool on);

  HardwareSerial* port_ = nullptr;
  int dePin_ = -1, rePin_ = -1;
  uint32_t settleMs_ = 0;
  State state_ = State::Idle;
  uint32_t since_ = 0;
  uint8_t slave_ = 0;
  uint16_t first_ = 0, count_ = 0;
  uint8_t buf_[modbus::responseLength(MAX_REGS)];
  size_t len_ = 0;
  uint16_t regs_[MAX_REGS] = {0};
  modbus::FrameError result_ = modbus::FrameError::Short;
};

}  // namespace eg
#endif  // ARDUINO
