#ifdef ARDUINO
#include "EGardenDoser.h"

#include "EGardenLog.h"

namespace eg {

void StepperDoser::begin(const ChannelPins (&channels)[safety::CHANNEL_COUNT], int dirPin, int enPin,
                         bool direction, unsigned halfPeriodUs) {
  for (int i = 0; i < safety::CHANNEL_COUNT; i++) {
    ch_[i] = channels[i];
    pinMode(ch_[i].stepPin, OUTPUT);
  }
  dirPin_ = dirPin;
  enPin_ = enPin;
  direction_ = direction;
  halfUs_ = halfPeriodUs;
  pinMode(dirPin_, OUTPUT);
  pinMode(enPin_, OUTPUT);
  digitalWrite(enPin_, LOW);  // v1: drivers enabled (EN LOW) at boot and never disabled
}

bool StepperDoser::start(safety::Channel ch, int units) {
  if (busy() || units <= 0) return false;
  channel_ = ch;
  stepsLeft_ = (long)units * ch_[(int)ch].stepsPerUnit;
  digitalWrite(dirPin_, direction_ ? HIGH : LOW);
  EG_LOG("Dose: %s x%d started (%ld steps)", safety::channelName(ch), units, stepsLeft_);
  return true;
}

void StepperDoser::stop() {
  if (!busy()) return;
  EG_LOG("Dose: %s stopped, %ld steps not run", safety::channelName(channel_), stepsLeft_);
  stepsLeft_ = 0;
  finished_ = true;
}

int StepperDoser::unitsLeft() const {
  long per = ch_[(int)channel_].stepsPerUnit;
  return busy() ? (int)((stepsLeft_ + per - 1) / per) : 0;
}

void StepperDoser::poll() {
  if (!busy()) return;
  int pin = ch_[(int)channel_].stepPin;
  uint32_t start = micros();
  // Same pulse shape as v1: half period HIGH, half period LOW.
  while (stepsLeft_ > 0 && micros() - start < BUDGET_US) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(halfUs_);
    digitalWrite(pin, LOW);
    delayMicroseconds(halfUs_);
    stepsLeft_--;
  }
  if (stepsLeft_ == 0) {
    EG_LOG("Dose: %s finished", safety::channelName(channel_));
    finished_ = true;
  }
}

}  // namespace eg
#endif  // ARDUINO
