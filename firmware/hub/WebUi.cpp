// Hub web server (port 8888, as in v1): the dashboard page (compiled into the
// firmware), a JSON API, a live serial console (Server-Sent Events), and the
// Wi-Fi / password setup. All pages and API calls need the web login.
//
// Rules that keep the web server from destabilising the hub:
// - handlers never touch the radio; they queue a Command for the loop task;
// - they read shared state only while holding StateLock, and briefly;
// - large responses are refused when free heap is low;
// - the number of console (SSE) clients is capped.
#include "WebUi.h"

#include <Arduino.h>
#include <EGarden.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "Automation.h"
#include "CloudUpload.h"
#include "HubState.h"
#include "Network.h"
#include "Storage.h"
#include "web_index.h"

using namespace eg;

extern eg::LoRaLink& hubRadio();  // defined in app.cpp

namespace hubapp {
namespace web {

namespace {

constexpr size_t MIN_FREE_HEAP = 24 * 1024;
constexpr size_t MAX_EVENT_CLIENTS = 3;
constexpr int MAX_LOG_LINES_PER_POLL = 20;

AsyncWebServer server(eg::hub::WEB_PORT);
AsyncWebServer portal(80);  // captive-portal redirect to port 8888
AsyncEventSource events("/events");
volatile bool readingChanged = false;
uint32_t lastLogSent = 0;

bool authed(AsyncWebServerRequest* req) {
  if (req->authenticate(net::webUser(), net::webPassword().c_str())) return true;
  req->requestAuthentication();
  return false;
}

void jsonString(String& out, const char* s) {
  out += '"';
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  out += '"';
}

void jsonNumber(String& out, bool has, float v, int decimals) {
  if (has) {
    out += String(v, decimals);
  } else {
    out += "null";
  }
}

long ageSeconds(bool valid, uint32_t atMs, uint32_t now) { return valid ? (long)((now - atMs) / 1000) : -1; }

void jsonAge(String& out, long age) {
  if (age < 0) {
    out += "null";
  } else {
    out += age;
  }
}

String stateJson() {
  uint32_t now = millis();
  String j;
  j.reserve(2048);
  {
    StateLock l;
    const AirReading& a = state.air;
    const WaterReading& w = state.water;
    j += "{\"water\":{\"ph\":";
    jsonNumber(j, w.r.hasPh, w.r.ph, 2);
    j += ",\"ec\":";
    jsonNumber(j, w.r.hasEc, w.r.ec, 0);
    j += ",\"t\":";
    jsonNumber(j, w.r.hasTemp, w.r.temp, 1);
    j += ",\"err\":";
    jsonString(j, w.err);
    j += ",\"age\":";
    jsonAge(j, ageSeconds(w.valid, w.atMs, now));
    j += ",\"rssi\":";
    j += w.rssi;
    j += ",\"dosing\":";
    jsonString(j, safety::refusalName(state.guard.check(now)));
    j += "},\"air\":{\"t\":";
    jsonNumber(j, a.hasT, a.t, 2);
    j += ",\"h\":";
    jsonNumber(j, a.hasH, a.h, 1);
    j += ",\"lux\":";
    jsonNumber(j, a.hasLux, a.lux, 1);
    j += ",\"bat\":";
    jsonNumber(j, a.hasBat, (float)a.bat, 0);
    j += ",\"err\":";
    jsonString(j, a.err);
    j += ",\"age\":";
    jsonAge(j, ageSeconds(a.valid, a.atMs, now));
    j += "},\"pump\":{\"known\":";
    j += state.pump.known ? "true" : "false";
    j += ",\"on\":";
    j += state.pump.on ? "true" : "false";
    long left = state.pump.left - (long)((now - state.pump.atMs) / 1000);
    j += ",\"left\":";
    j += state.pump.on && left > 0 ? left : 0;
    j += "},\"dose\":{\"known\":";
    j += state.dose.known ? "true" : "false";
    j += ",\"dose\":";
    jsonString(j, state.dose.dose);
    j += ",\"left\":";
    j += state.dose.left;
    j += "},\"limits\":{\"used\":[";
    for (int i = 0; i < safety::CHANNEL_COUNT; i++) {
      if (i) j += ',';
      j += state.limiter.unitsInLastHour((safety::Channel)i, now);
    }
    j += "],\"perHour\":";
    j += safety::MAX_UNITS_PER_CHANNEL_PER_HOUR;
    j += "},\"commands\":[";
    bool first = true;
    for (int k = 0; k < RECORD_COUNT; k++) {
      int i = (state.nextRecord - 1 - k + RECORD_COUNT) % RECORD_COUNT;  // newest first
      const CommandRecord& r = state.records[i];
      if (!r.id) continue;
      if (!first) j += ',';
      first = false;
      j += "{\"id\":";
      j += r.id;
      j += ",\"text\":";
      jsonString(j, r.text);
      j += ",\"result\":";
      jsonString(j, r.result);
      j += ",\"age\":";
      j += (now - r.atMs) / 1000;
      j += '}';
    }
    j += "],\"clock\":{\"synced\":";
    j += state.timeSynced ? "true" : "false";
    char local[8] = "";
    if (state.timeSynced) {
      time_t t = time(nullptr);
      struct tm tmv;
      localtime_r(&t, &tmv);
      strftime(local, sizeof(local), "%H:%M", &tmv);
    }
    j += ",\"local\":";
    jsonString(j, local);
    j += "},\"system\":{\"uptime\":";
    j += now / 1000;
    j += ",\"resetReason\":";
    jsonString(j, state.resetReason);
    j += ",\"boots\":";
    j += state.bootCount;
    j += ",\"safeMode\":";
    j += state.safeMode ? "true" : "false";
  }
  j += ",\"heap\":";
  j += ESP.getFreeHeap();
  j += ",\"historyBytes\":";
  j += storage::historyBytes();
  j += ",\"defaultPassword\":";
  j += net::usingDefaultWebPassword() ? "true" : "false";
  j += ",\"autoDosing\":";
  j += automation::autoDosingCompiled() ? "true" : "false";
  j += ",\"cloud\":";
  j += cloud::compiled() ? "true" : "false";
  LoRaLink& radio = hubRadio();
  j += "},\"lora\":{\"ready\":";
  j += radio.ready() ? "true" : "false";
  j += ",\"rx\":";
  j += radio.rxOk;
  j += ",\"bad\":";
  j += radio.rxBad;
  j += ",\"tx\":";
  j += radio.txCount;
  j += ",\"retries\":";
  j += radio.retries;
  j += ",\"undelivered\":";
  j += radio.undelivered;
  j += ",\"rssi\":";
  j += radio.lastRssi;
  j += "},\"wifi\":";
  j += net::statusJson();
  j += '}';
  return j;
}

bool parseCommand(AsyncWebServerRequest* req, Command& c, String& error) {
  auto param = [req](const char* name) -> String {
    return req->hasParam(name, true) ? req->getParam(name, true)->value() : String();
  };
  String op = param("op");
  if (op == "pump") {
    String on = param("on");
    if (on != "0" && on != "1") return error = "on must be 0 or 1", false;
    c.op = Op::Pump;
    c.on = on == "1";
    return true;
  }
  if (op == "dose" || op == "stop") {
    String ch = param("ch");
    c.op = op == "dose" ? Op::Dose : Op::Stop;
    if (c.op == Op::Stop && ch == "ALL") {
      c.allChannels = true;
    } else if (!safety::parseChannel(ch.c_str(), c.ch)) {
      return error = "ch must be A, B or ACID", false;
    }
    if (c.op == Op::Dose) {
      String n = param("n");
      c.units = n.toInt();
      if (n.length() == 0 || c.units <= 0) return error = "n must be a whole number of units", false;
    }
    return true;
  }
  error = "op must be pump, dose or stop";
  return false;
}

void addLogLine(uint32_t id, const char* line, void* ctx) {
  String& out = *static_cast<String*>(ctx);
  if (out.length() > 1) out += ',';
  out += '[';
  out += id;
  out += ',';
  jsonString(out, line);
  out += ']';
}

void sendLogEvent(uint32_t id, const char* line, void*) {
  String data = String(id) + " " + line;
  events.send(data.c_str(), "log", id);
}

}  // namespace

void begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    AsyncWebServerResponse* res = req->beginResponse(200, "text/html", WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
    res->addHeader("Content-Encoding", "gzip");
    req->send(res);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    req->send(200, "application/json", stateJson());
  });

  server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    Command c;
    c.source = Source::Web;
    String error;
    if (!parseCommand(req, c, error)) return req->send(400, "text/plain", error);
    if (!queueCommand(c)) return req->send(503, "text/plain", "command queue full, try again");
    req->send(202, "application/json", "{\"queued\":true}");
  });

  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    uint32_t since = req->hasParam("since") ? (uint32_t)req->getParam("since")->value().toInt() : 0;
    String out = "{\"lines\":[";
    eg::log::forEachSince(since, addLogLine, &out);
    out += "]}";
    req->send(200, "application/json", out);
  });

  server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    if (ESP.getFreeHeap() < MIN_FREE_HEAP) return req->send(503, "text/plain", "low memory, try again");
    bool previous = req->hasParam("part") && req->getParam("part")->value() == "1";
    const char* path = previous ? storage::HISTORY_PREVIOUS : storage::HISTORY_CURRENT;
    if (!storage::mounted() || !storage::fs().exists(path)) return req->send(200, "text/csv", "");
    req->send(storage::fs(), path, "text/csv");
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    String ssid = req->hasParam("ssid", true) ? req->getParam("ssid", true)->value() : String();
    String pass = req->hasParam("password", true) ? req->getParam("password", true)->value() : String();
    if (!net::saveCredentials(ssid, pass)) {
      return req->send(400, "text/plain", "Invalid: SSID 1-32 characters, password empty or 8-64 characters");
    }
    req->send(200, "text/plain", "Saved. The hub is connecting; find it at http://egarden.local:8888 once it is online.");
  });

  server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!authed(req)) return;
    String pw = req->hasParam("password", true) ? req->getParam("password", true)->value() : String();
    if (!net::saveWebPassword(pw)) return req->send(400, "text/plain", "Password must be 8-64 characters");
    events.setAuthentication(net::webUser(), net::webPassword().c_str());
    req->send(200, "text/plain", "Password changed. The browser will ask you to log in again.");
  });

  events.setAuthentication(net::webUser(), net::webPassword().c_str());
  events.onConnect([](AsyncEventSourceClient* client) {
    if (events.count() > MAX_EVENT_CLIENTS) client->close();
  });
  server.addHandler(&events);

  server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "Not found"); });
  server.begin();

  // Phones and laptops probe port 80 when joining the setup access point.
  portal.onNotFound([](AsyncWebServerRequest* req) {
    IPAddress ip = net::apActive() ? WiFi.softAPIP() : WiFi.localIP();
    req->redirect(String("http://") + ip.toString() + ":" + eg::hub::WEB_PORT + "/");
  });
  portal.begin();
  EG_LOG("Web: dashboard on port %u", eg::hub::WEB_PORT);
}

void poll() {
  if (events.count() == 0) {
    lastLogSent = eg::log::lastId();
    readingChanged = false;
    return;
  }
  // Push new console lines (bounded per call so the loop stays responsive).
  uint32_t target = eg::log::lastId();
  if (target - lastLogSent > (uint32_t)MAX_LOG_LINES_PER_POLL) lastLogSent = target - MAX_LOG_LINES_PER_POLL;
  lastLogSent = eg::log::forEachSince(lastLogSent, sendLogEvent, nullptr);
  if (readingChanged) {
    readingChanged = false;
    events.send("1", "reading");
  }
}

void onAirReading() { readingChanged = true; }
void onWaterReading() { readingChanged = true; }

}  // namespace web
}  // namespace hubapp
