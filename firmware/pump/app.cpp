// E-Garden pump node: switches the water pump relay on LoRa commands from the hub.
//
// v1 behaviour kept: relay on GPIO26, the pump turns itself off 90 s after it
// was switched on, and a repeated "on" does not extend that time.
// Changed (approved): protocol v2 with ACK, status reports, no work inside the
// radio interrupt, and the radio is re-initialised if it fails to start.
#include <Arduino.h>
#include <EGarden.h>

using namespace eg;

namespace {

LoRaLink radio;
safety::PumpTimer timer;

void setRelay(bool on) { digitalWrite(pump::PIN_RELAY, on ? HIGH : LOW); }

void sendStatus() {
  proto::Message sta;
  sta.type = proto::MsgType::Status;
  sta.dst = ADDR_HUB;
  sta.setInt("on", timer.on() ? 1 : 0);
  sta.setInt("left", (long)(timer.remainingMs(millis()) / 1000));
  radio.send(sta);
}

void onMessage(const proto::Message& msg, int rssi, void*) {
  if (msg.type != proto::MsgType::Command || msg.src != ADDR_HUB) return;
  if (radio.replayIfDuplicate(msg)) return;

  const char* op = msg.get("op");
  long on;
  if (!op || strcmp(op, "pump") != 0 || !msg.getInt("on", on) || (on != 0 && on != 1)) {
    EG_LOG("Pump: refused command (bad_cmd)");
    radio.ack(msg, false, "bad_cmd");
    return;
  }
  if (on) {
    timer.switchOn(millis());
    setRelay(true);
    EG_LOG("Pump: ON (auto-off in %lus), rssi %d", (unsigned long)(timer.remainingMs(millis()) / 1000), rssi);
  } else {
    timer.switchOff();
    setRelay(false);
    EG_LOG("Pump: OFF, rssi %d", rssi);
  }
  radio.ack(msg, true);
  sendStatus();
}

}  // namespace

void setup() {
  pinMode(pump::PIN_RELAY, OUTPUT);
  setRelay(false);  // always start with the pump off
  eg::log::begin(115200);
  EG_LOG("E-Garden pump node, address %u", ADDR_PUMP);
  radio.onMessage(onMessage);
  radio.begin({pump::PIN_LORA_SS, pump::PIN_LORA_RST, pump::PIN_LORA_DIO0}, ADDR_PUMP);
}

void loop() {
  radio.poll();
  if (timer.expired(millis())) {
    setRelay(false);
    EG_LOG("Pump: OFF (auto-off after %lus)", (unsigned long)(pump::MAX_ON_MS / 1000));
    sendStatus();
  }
}
