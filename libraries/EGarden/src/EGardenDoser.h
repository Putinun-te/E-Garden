// Stepper-driven dosing pumps (water node). Non-blocking: poll() generates
// step pulses for at most a short time budget per call, so the node keeps
// receiving LoRa commands (including stop) while a dose runs.
#pragma once
#ifdef ARDUINO
#include <Arduino.h>

#include "EGardenSafety.h"

namespace eg {

class StepperDoser {
 public:
  struct ChannelPins {
    int stepPin;
    long stepsPerUnit;
  };
  static constexpr uint32_t BUDGET_US = 2000;  // max stepping time per poll()

  void begin(const ChannelPins (&channels)[safety::CHANNEL_COUNT], int dirPin, int enPin, bool direction,
             unsigned halfPeriodUs);
  bool start(safety::Channel ch, int units);
  void stop();  // stops whatever is running
  void poll();
  bool busy() const { return stepsLeft_ > 0; }
  safety::Channel channel() const { return channel_; }
  int unitsLeft() const;
  // True once after a dose finished or was stopped (for status reports).
  bool takeFinished() {
    bool f = finished_;
    finished_ = false;
    return f;
  }

 private:
  ChannelPins ch_[safety::CHANNEL_COUNT]{};
  int dirPin_ = -1, enPin_ = -1;
  bool direction_ = false;
  unsigned halfUs_ = 150;
  safety::Channel channel_ = safety::Channel::A;
  long stepsLeft_ = 0;
  bool finished_ = false;
};

}  // namespace eg
#endif  // ARDUINO
