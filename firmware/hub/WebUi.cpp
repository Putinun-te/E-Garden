// v1-compatible web layer: serves the original dashboard (data/index.html on
// SPIFFS) and its routes, but every action is queued as a Command for the loop
// task instead of driving the radio from the web server task.
#include "WebUi.h"

#include <Arduino.h>
#include <EGarden.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include "HubState.h"

using namespace eg;

namespace hubapp {
namespace web {

namespace {

AsyncWebServer server(eg::hub::WEB_PORT);
bool toggleA = true, toggleB = true, toggleAcid = true;  // v1 Boolsit1..3

String fmt(bool has, float v, int decimals = 2) { return has ? String(v, decimals) : String(); }

String value(const String& var) {
  StateLock l;
  const AirReading& a = state.air;
  const WaterReading& w = state.water;
  if (var == "Temp" || var == "temperature") return fmt(a.hasT, a.t);
  if (var == "Humid" || var == "humidity") return fmt(a.hasH, a.h);
  if (var == "Light") return fmt(a.hasLux, a.lux);
  if (var == "BATT") return a.hasBat ? String(a.bat) : String();
  if (var == "Temp2") return fmt(w.r.hasTemp, w.r.temp);
  if (var == "EC") return w.r.hasEc ? String((long)w.r.ec) : String();
  if (var == "PH") return fmt(w.r.hasPh, w.r.ph);
  if (var == "RRSI" || var == "rssi") return String(w.valid ? w.rssi : a.rssi);
  return String();
}

String readFile(const char* path) {
  File f = SPIFFS.open(path, "r");
  if (!f || f.isDirectory()) return String();
  String s = f.readString();
  f.close();
  return s;
}

void writeFile(const char* path, const String& text) {
  File f = SPIFFS.open(path, "w");
  if (f) {
    f.print(text);
    f.close();
  }
}

String processor(const String& var) {
  if (var == "inputString") return readFile("/inputString.txt");
  if (var == "inputInt") return readFile("/inputInt.txt");
  if (var == "inputFloat") return readFile("/inputFloat.txt");
  return value(var);
}

void queue(Command c, AsyncWebServerRequest* req) {
  bool ok = queueCommand(c);
  req->send(ok ? 200 : 503, "text/plain", ok ? "ok" : "busy");
}

void pumpRoute(const char* path, bool on) {
  server.on(path, HTTP_GET, [on](AsyncWebServerRequest* req) {
    Command c;
    c.op = Op::Pump;
    c.on = on;
    queue(c, req);
  });
}

// v1 toggled between "Add(X)" without an amount (which dosed nothing) and
// "Stop(X)" (which the water node ignored). Now the stop half works and the
// add half only explains how to dose.
void toggleRoute(const char* path, safety::Channel ch, bool& addNext) {
  server.on(path, HTTP_GET, [ch, &addNext](AsyncWebServerRequest* req) {
    bool add = addNext;
    addNext = !addNext;
    if (add) {
      EG_LOG("Web: %s has no amount; use the dosing form", safety::channelName(ch));
      req->send(200, "text/plain", "ok");
      return;
    }
    Command c;
    c.op = Op::Stop;
    c.ch = ch;
    queue(c, req);
  });
}

void okRoute(const char* path) {
  server.on(path, HTTP_GET, [](AsyncWebServerRequest* req) { req->send(200, "text/plain", "ok"); });
}

}  // namespace

void begin() {
  if (!SPIFFS.begin(true)) EG_LOG("Web: SPIFFS mount failed, dashboard page unavailable");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(SPIFFS, "/index.html", String(), false, processor);
  });
  for (const char* v : {"Temp", "Humid", "Light", "BATT", "Temp2", "EC", "PH", "rssi", "temperature", "humidity"}) {
    String path = String("/") + v;
    String var = v;
    server.on(path.c_str(), HTTP_GET, [var](AsyncWebServerRequest* req) {
      req->send(200, "text/plain", value(var));
    });
  }
  pumpRoute("/on1", true);
  pumpRoute("/on2", false);
  toggleRoute("/on3", safety::Channel::A, toggleA);
  toggleRoute("/on4", safety::Channel::B, toggleB);
  toggleRoute("/on5", safety::Channel::Acid, toggleAcid);
  for (const char* p : {"/off1", "/off2", "/off3", "/off4", "/off5"}) okRoute(p);

  // v1 form: inputString -> A, inputInt -> B, inputFloat -> Acid, value = units.
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest* req) {
    struct {
      const char* param;
      const char* file;
      safety::Channel ch;
    } const fields[] = {{"inputString", "/inputString.txt", safety::Channel::A},
                        {"inputInt", "/inputInt.txt", safety::Channel::B},
                        {"inputFloat", "/inputFloat.txt", safety::Channel::Acid}};
    for (const auto& f : fields) {
      if (!req->hasParam(f.param)) continue;
      String v = req->getParam(f.param)->value();
      writeFile(f.file, v);
      Command c;
      c.op = Op::Dose;
      c.ch = f.ch;
      c.units = v.toInt();
      if (c.units <= 0) {
        req->send(400, "text/plain", "amount must be a whole number of units (1-5)");
        return;
      }
      queue(c, req);
      return;
    }
    req->send(200, "text/plain", "No message sent");
  });
  server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "Not found"); });
  server.begin();
  EG_LOG("Web: listening on port %u", eg::hub::WEB_PORT);
}

void poll() {}
void onAirReading() {}
void onWaterReading() {}

}  // namespace web
}  // namespace hubapp
