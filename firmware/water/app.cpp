// E-Garden water sensor node: reads pH, water temperature and EC from the RS485
// probes every 30 s, reports them to the hub, and runs the dosing pumps on
// command.
//
// v1 behaviour kept: probe request bytes and x10 scaling, 30 s interval, pins,
// step counts per unit, pulse timing, channel-to-pin mapping, drivers enabled
// at boot.
// Changed (approved): protocol v2 with ACK, Stop works, Modbus on a hardware
// UART with reply timeout and CRC check, non-blocking stepping, and dosing is
// refused when it breaks a safety rule (see EGardenSafety.h).
#include <Arduino.h>
#include <EGarden.h>
#include <Preferences.h>

using namespace eg;

namespace {

LoRaLink radio;
ModbusProbe probe;
StepperDoser doser;
safety::ReadingGuard guard;
safety::DoseLimiter limiter;
Preferences prefs;

const char* const kPrefKeys[safety::CHANNEL_COUNT] = {"uA", "uB", "uACID"};

// Measurement sequence: pH/temperature probe, then EC probe, then report.
enum class Step : uint8_t { Wait, ReadPhTemp, ReadEc };
Step step = Step::Wait;
uint32_t lastMeasureMs = 0;
bool firstMeasureDone = false;
safety::WaterReading reading;
bool phFailed = false, ecFailed = false;

void saveDoseHistory() {
  // Stored so a reboot cannot reset the hourly limit (see restoreConservative).
  uint32_t now = millis();
  for (int i = 0; i < safety::CHANNEL_COUNT; i++) {
    prefs.putUChar(kPrefKeys[i], (uint8_t)limiter.unitsInLastHour((safety::Channel)i, now));
  }
}

void sendStatus() {
  proto::Message sta;
  sta.type = proto::MsgType::Status;
  sta.dst = ADDR_HUB;
  sta.set("dose", doser.busy() ? safety::channelName(doser.channel()) : "none");
  sta.setInt("left", doser.unitsLeft());
  radio.send(sta);
}

void sendReading() {
  proto::Message m;
  m.type = proto::MsgType::Water;
  m.dst = ADDR_HUB;
  if (reading.hasPh) m.setFloat("ph", reading.ph, 1);
  if (reading.hasEc) m.setInt("ec", (long)reading.ec);
  if (reading.hasTemp) m.setFloat("t", reading.temp, 1);
  if (phFailed || ecFailed) m.set("err", phFailed && ecFailed ? "ph+ec" : (phFailed ? "ph" : "ec"));
  radio.send(m);
  EG_LOG("Water: pH %s EC %s T %s%s", m.has("ph") ? m.get("ph") : "-", m.has("ec") ? m.get("ec") : "-",
         m.has("t") ? m.get("t") : "-", guard.locked() ? " (dosing locked: out of range)" : "");
}

void runMeasurement() {
  uint32_t now = millis();
  switch (step) {
    case Step::Wait:
      // v1 took the first reading 30 s after boot, then every 30 s.
      if (now - lastMeasureMs >= water::MEASURE_INTERVAL_MS || (!firstMeasureDone && now >= water::MEASURE_INTERVAL_MS)) {
        reading = safety::WaterReading();
        phFailed = ecFailed = false;
        probe.start(modbus::PH_TEMP_SLAVE, modbus::PH_TEMP_FIRST, modbus::PH_TEMP_COUNT);
        step = Step::ReadPhTemp;
      }
      break;
    case Step::ReadPhTemp:
      if (!probe.poll()) break;
      if (probe.result() == modbus::FrameError::None) {
        reading.hasTemp = reading.hasPh = true;
        reading.temp = probe.reg(0) / 10.0f;  // v1 scaling
        reading.ph = probe.reg(1) / 10.0f;
      } else {
        phFailed = true;
        EG_LOG("Water: pH probe read failed (%s)", modbus::frameErrorName(probe.result()));
      }
      probe.start(modbus::EC_SLAVE, modbus::EC_FIRST, modbus::EC_COUNT);
      step = Step::ReadEc;
      break;
    case Step::ReadEc:
      if (!probe.poll()) break;
      if (probe.result() == modbus::FrameError::None) {
        reading.hasEc = true;
        reading.ec = (float)(probe.reg(0) / 10);  // v1 used integer division
      } else {
        ecFailed = true;
        EG_LOG("Water: EC probe read failed (%s)", modbus::frameErrorName(probe.result()));
      }
      guard.update(reading, now);
      sendReading();
      lastMeasureMs = now;
      firstMeasureDone = true;
      step = Step::Wait;
      break;
  }
}

void handleCommand(const proto::Message& msg) {
  const char* op = msg.get("op");
  const char* chName = msg.get("ch");
  if (op && strcmp(op, "stop") == 0) {
    safety::Channel ch;
    bool all = chName && strcmp(chName, "ALL") == 0;
    if (!all && !safety::parseChannel(chName, ch)) {
      radio.ack(msg, false, "bad_cmd");
      return;
    }
    if (doser.busy() && (all || doser.channel() == ch)) doser.stop();
    radio.ack(msg, true);
    sendStatus();
    return;
  }
  if (!op || strcmp(op, "dose") != 0) {
    radio.ack(msg, false, "bad_cmd");
    return;
  }
  safety::Channel ch;
  long units;
  if (!safety::parseChannel(chName, ch) || !msg.getInt("n", units)) {
    radio.ack(msg, false, "bad_cmd");
    return;
  }
  uint32_t now = millis();
  safety::Refusal r = doser.busy() ? safety::Refusal::Busy : guard.check(now);
  if (r == safety::Refusal::None) r = limiter.check(ch, (int)units, now);
  if (r != safety::Refusal::None) {
    EG_LOG("Dose: refused %s x%ld (%s)", safety::channelName(ch), units, safety::refusalName(r));
    radio.ack(msg, false, safety::refusalName(r));
    return;
  }
  limiter.record(ch, (int)units, now);
  saveDoseHistory();
  doser.start(ch, (int)units);
  radio.ack(msg, true);
  sendStatus();
}

void onMessage(const proto::Message& msg, int, void*) {
  if (msg.type != proto::MsgType::Command || msg.src != ADDR_HUB) return;
  if (radio.replayIfDuplicate(msg)) return;
  handleCommand(msg);
}

}  // namespace

void setup() {
  eg::log::begin(115200);
  EG_LOG("E-Garden water node, address %u", ADDR_WATER);

  const StepperDoser::ChannelPins channels[safety::CHANNEL_COUNT] = {
      {water::PIN_DOSE_STEP_A, water::STEPS_PER_UNIT_A},
      {water::PIN_DOSE_STEP_B, water::STEPS_PER_UNIT_B},
      {water::PIN_DOSE_STEP_ACID, water::STEPS_PER_UNIT_ACID},
  };
  doser.begin(channels, water::PIN_DOSE_DIR, water::PIN_DOSE_EN, water::DOSE_DIRECTION, water::STEP_HALF_PERIOD_US);
  probe.begin(Serial2, water::PIN_RS485_RX, water::PIN_RS485_TX, water::RS485_BAUD, water::PIN_RS485_DE,
              water::PIN_RS485_RE, water::MODBUS_SETTLE_MS);

  prefs.begin("egwater", false);
  for (int i = 0; i < safety::CHANNEL_COUNT; i++) {
    uint8_t units = prefs.getUChar(kPrefKeys[i], 0);
    if (units) {
      limiter.restoreConservative((safety::Channel)i, units, millis());
      EG_LOG("Dose: %u units on %s counted from before the reboot", units, safety::channelName((safety::Channel)i));
    }
  }

  radio.onMessage(onMessage);
  radio.begin({water::PIN_LORA_SS, water::PIN_LORA_RST, water::PIN_LORA_DIO0}, ADDR_WATER);
}

void loop() {
  radio.poll();
  doser.poll();
  if (doser.takeFinished()) sendStatus();
  runMeasurement();
}
