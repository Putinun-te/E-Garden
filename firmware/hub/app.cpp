// E-Garden Hub (Master node): receives sensor readings over LoRa, serves the
// dashboard, runs the pump schedule, and sends commands to the other nodes.
//
// Kept from v1: LoRa pins, web port 8888, NTP settings, pump schedule
// (on at HH:00, off at HH:01, 08-17), the dashboard page and its routes.
// Changed (approved): protocol v2 with ACK/retry, commands go through a queue
// executed by the loop task (web handlers never touch the radio), Wi-Fi and
// LoRa reconnect instead of blocking forever, scheduled commands are sent
// once, dose commands are checked against the safety rules, a watchdog, and
// a safe mode after repeated crashes. Auto dosing and cloud upload are build
// flags (ENABLE_AUTO_DOSING, ENABLE_CLOUD_UPLOAD), both off by default.
#include <Arduino.h>
#include <EGarden.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

#include "Automation.h"
#include "CloudUpload.h"
#include "HubState.h"
#include "Network.h"
#include "WebUi.h"

using namespace eg;
using hubapp::state;
using hubapp::StateLock;

namespace {

LoRaLink radio;
Preferences prefs;

constexpr uint32_t SAFE_MODE_BOOTS = 3;           // boots in a row without 60 s uptime
constexpr uint32_t STABLE_UPTIME_MS = 60000;
constexpr time_t CLOCK_VALID_AFTER = 1700000000;  // any time before Nov 2023 means "not synced"

// Commands waiting for an ACK, so the ACK callback knows what it confirms.
struct InFlight {
  bool used = false;
  uint16_t seq = 0;
  hubapp::Command cmd;
};
InFlight inFlight[LoRaLink::MAX_PENDING];

const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power_on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_EXT: return "reset_pin";
    default: return "other";
  }
}

time_t clockNow() { return state.timeSynced ? time(nullptr) : 0; }

void onAck(uint16_t seq, bool delivered, const proto::Message* ack, void*) {
  for (InFlight& f : inFlight) {
    if (!f.used || f.seq != seq) continue;
    f.used = false;
    long ok = 0;
    const char* err = nullptr;
    if (delivered && ack) {
      ack->getInt("ok", ok);
      err = ack->get("err");
    }
    StateLock l;
    if (!delivered) {
      hubapp::setRecordResult(f.cmd.id, "no_ack");
    } else if (ok) {
      hubapp::setRecordResult(f.cmd.id, "ok");
      if (f.cmd.op == hubapp::Op::Dose) state.limiter.record(f.cmd.ch, f.cmd.units, millis());
    } else {
      hubapp::setRecordResult(f.cmd.id, err ? err : "refused");
    }
    char text[32];
    hubapp::describe(f.cmd, text, sizeof(text));
    EG_LOG("Command #%lu (%s): %s", (unsigned long)f.cmd.id, text,
           !delivered ? "no ACK" : (ok ? "OK" : (err ? err : "refused")));
  }
}

void execute(const hubapp::Command& cmd) {
  proto::Message m;
  m.type = proto::MsgType::Command;
  if (cmd.op == hubapp::Op::Pump) {
    m.dst = ADDR_PUMP;
    m.set("op", "pump");
    m.setInt("on", cmd.on ? 1 : 0);
  } else {
    m.dst = ADDR_WATER;
    if (cmd.op == hubapp::Op::Dose) {
      // Refuse early on the hub too; the water node checks again.
      safety::Refusal r;
      {
        StateLock l;
        uint32_t now = millis();
        r = state.guard.check(now);
        if (r == safety::Refusal::None) r = state.limiter.check(cmd.ch, cmd.units, now);
        if (r != safety::Refusal::None) hubapp::setRecordResult(cmd.id, safety::refusalName(r));
      }
      if (r != safety::Refusal::None) {
        EG_LOG("Command #%lu refused on hub: %s", (unsigned long)cmd.id, safety::refusalName(r));
        return;
      }
      m.set("op", "dose");
      m.set("ch", safety::channelName(cmd.ch));
      m.setInt("n", cmd.units);
    } else {
      m.set("op", "stop");
      m.set("ch", cmd.allChannels ? "ALL" : safety::channelName(cmd.ch));
    }
  }
  int32_t seq = radio.sendReliable(m, onAck);
  StateLock l;
  if (seq < 0) {
    hubapp::setRecordResult(cmd.id, radio.ready() ? "busy_radio" : "no_radio");
    return;
  }
  hubapp::setRecordResult(cmd.id, "sent");
  for (InFlight& f : inFlight) {
    if (!f.used) {
      f.used = true;
      f.seq = (uint16_t)seq;
      f.cmd = cmd;
      break;
    }
  }
}

void onMessage(const proto::Message& msg, int rssi, void*) {
  uint32_t now = millis();
  StateLock l;
  if (msg.type == proto::MsgType::Air && msg.src == ADDR_AIR) {
    hubapp::AirReading& a = state.air;
    a.valid = true;
    a.hasT = msg.getFloat("t", a.t);
    a.hasH = msg.getFloat("h", a.h);
    a.hasLux = msg.getFloat("lux", a.lux);
    a.hasBat = msg.getInt("bat", a.bat);
    strncpy(a.err, msg.has("err") ? msg.get("err") : "", sizeof(a.err) - 1);
    a.atMs = now;
    a.at = clockNow();
    a.rssi = rssi;
    EG_LOG("Air: T %.2f H %.1f lux %.1f bat %ld%s%s (rssi %d)", a.t, a.h, a.lux, a.bat, a.err[0] ? " err=" : "", a.err,
           rssi);
    if (a.hasT && a.hasH && a.hasLux) hubapp::cloud::queueAir(a.t, a.h, a.lux, a.bat);
    hubapp::web::onAirReading();
  } else if (msg.type == proto::MsgType::Water && msg.src == ADDR_WATER) {
    hubapp::WaterReading& w = state.water;
    w.valid = true;
    w.r = safety::WaterReading();
    w.r.hasPh = msg.getFloat("ph", w.r.ph);
    w.r.hasEc = msg.getFloat("ec", w.r.ec);
    w.r.hasTemp = msg.getFloat("t", w.r.temp);
    strncpy(w.err, msg.has("err") ? msg.get("err") : "", sizeof(w.err) - 1);
    w.atMs = now;
    w.at = clockNow();
    w.rssi = rssi;
    state.guard.update(w.r, now);
    EG_LOG("Water: pH %.1f EC %.0f T %.1f%s%s (rssi %d)", w.r.ph, w.r.ec, w.r.temp, w.err[0] ? " err=" : "", w.err,
           rssi);
    if (w.r.hasPh && w.r.hasEc && w.r.hasTemp) hubapp::cloud::queueWater(w.r.ph, w.r.ec, w.r.temp);
    hubapp::web::onWaterReading();
  } else if (msg.type == proto::MsgType::Status && msg.src == ADDR_PUMP) {
    long on = 0;
    state.pump.known = msg.getInt("on", on);
    state.pump.on = on == 1;
    msg.getInt("left", state.pump.left);
    state.pump.atMs = now;
  } else if (msg.type == proto::MsgType::Status && msg.src == ADDR_WATER) {
    state.dose.known = true;
    strncpy(state.dose.dose, msg.has("dose") ? msg.get("dose") : "none", sizeof(state.dose.dose) - 1);
    msg.getInt("left", state.dose.left);
    state.dose.atMs = now;
  }
}

void clockTick() {
  static uint32_t lastTickMs = 0;
  uint32_t now = millis();
  if (now - lastTickMs < eg::hub::CLOCK_TICK_MS) return;
  lastTickMs = now;

  time_t t = time(nullptr);
  bool synced = t > CLOCK_VALID_AFTER;
  if (synced != state.timeSynced) {
    StateLock l;
    state.timeSynced = synced;
    EG_LOG("Clock: %s", synced ? "synced" : "not synced");
  }
  if (!synced) return;  // no schedule or auto dosing without a valid clock
  if (state.safeMode) return;
  struct tm local;
  localtime_r(&t, &local);
  hubapp::automation::tick(local, now);
}

void safeModeCheck() {
  static bool cleared = false;
  if (!cleared && millis() > STABLE_UPTIME_MS) {
    cleared = true;
    prefs.putUInt("boots", 0);
  }
}

}  // namespace

void setup() {
  eg::log::begin(115200);
  hubapp::stateInit();
  pinMode(eg::hub::PIN_OUTPUT1, OUTPUT);  // v1

  prefs.begin("eghub", false);
  uint32_t boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);
  {
    StateLock l;
    state.bootCount = boots;
    state.safeMode = boots >= SAFE_MODE_BOOTS;
    strncpy(state.resetReason, resetReasonName(esp_reset_reason()), sizeof(state.resetReason) - 1);
  }
  EG_LOG("E-Garden hub, address %u, reset reason: %s", ADDR_HUB, state.resetReason);
  if (state.safeMode) EG_LOG("SAFE MODE: %lu boots without 60 s uptime; schedule and auto dosing are off", (unsigned long)boots);

  radio.onMessage(onMessage);
  radio.begin({eg::hub::PIN_LORA_SS, eg::hub::PIN_LORA_RST, eg::hub::PIN_LORA_DIO0}, ADDR_HUB);

  hubapp::net::begin();
  configTime(eg::hub::NTP_GMT_OFFSET_SEC, eg::hub::NTP_DAYLIGHT_OFFSET_SEC, eg::hub::NTP_SERVER);
  hubapp::cloud::begin();
  hubapp::web::begin();

  enableLoopWDT();  // resets the hub if loop() stops running for 5 s
}

void loop() {
  radio.poll();
  hubapp::Command cmd;
  if (hubapp::nextCommand(cmd)) execute(cmd);
  hubapp::net::poll();
  clockTick();
  hubapp::web::poll();
  safeModeCheck();
}
