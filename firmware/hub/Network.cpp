#include "Network.h"

#include <DNSServer.h>
#include <EGarden.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include "HubState.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found: building with placeholder values from secrets.h.example"
#include "secrets.h.example"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "egarden-setup"
#endif
#ifndef WEB_PASSWORD
#define WEB_PASSWORD "egarden"
#endif

namespace hubapp {
namespace net {

namespace {
constexpr uint32_t RECONNECT_EVERY_MS = 30000;
constexpr uint32_t AP_AFTER_OFFLINE_MS = 30000;  // open the setup AP after 30 s without Wi-Fi
constexpr uint32_t AP_OFF_AFTER_ONLINE_MS = 120000;

Preferences prefs;
DNSServer dns;
String ssid, password;
uint32_t lastAttemptMs = 0, offlineSinceMs = 0, onlineSinceMs = 0;
bool wasConnected = false, ap = false, mdnsUp = false;

bool isPlaceholder(const String& s) { return s.length() == 0 || s.startsWith("YOUR_"); }

void startStation() {
  if (isPlaceholder(ssid)) return;
  WiFi.begin(ssid.c_str(), password.c_str());
  lastAttemptMs = millis();
  EG_LOG("Wi-Fi: connecting to %s", ssid.c_str());
}

void startAp() {
  if (ap) return;
  WiFi.mode(WIFI_AP_STA);  // keep trying the station while the AP is up
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  dns.start(53, "*", WiFi.softAPIP());  // captive portal: every name resolves to the hub
  ap = true;
  EG_LOG("Wi-Fi: setup AP \"%s\" at http://%s:%u/setup", AP_SSID, WiFi.softAPIP().toString().c_str(),
         eg::hub::WEB_PORT);
}

void stopAp() {
  if (!ap) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap = false;
  EG_LOG("Wi-Fi: setup AP closed");
}
}  // namespace

void begin() {
  prefs.begin("egnet", false);
  ssid = prefs.getString("ssid", WIFI_SSID);
  password = prefs.getString("pass", WIFI_PASSWORD);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(MDNS_NAME);
  offlineSinceMs = millis();
  if (isPlaceholder(ssid)) {
    EG_LOG("Wi-Fi: no network configured");
    startAp();
  } else {
    startStation();
  }
  if (usingDefaultWebPassword()) EG_LOG("Web: using the default password, change it on the setup page");
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
bool apActive() { return ap; }

void poll() {
  uint32_t now = millis();
  if (ap) dns.processNextRequest();

  bool up = connected();
  if (up != wasConnected) {
    wasConnected = up;
    {
      StateLock l;
      state.wifiConnected = up;
    }
    if (up) {
      onlineSinceMs = now;
      EG_LOG("Wi-Fi: connected to %s, IP %s, RSSI %d", ssid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (!mdnsUp && MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", eg::hub::WEB_PORT);
        mdnsUp = true;
        EG_LOG("Wi-Fi: http://%s.local:%u", MDNS_NAME, eg::hub::WEB_PORT);
      }
    } else {
      offlineSinceMs = now;
      EG_LOG("Wi-Fi: disconnected");
    }
  }

  if (!up) {
    // v1 waited forever at boot and never reconnected.
    if (!isPlaceholder(ssid) && now - lastAttemptMs >= RECONNECT_EVERY_MS) {
      WiFi.disconnect();
      startStation();
    }
    if (!ap && now - offlineSinceMs >= AP_AFTER_OFFLINE_MS) startAp();
  } else if (ap && now - onlineSinceMs >= AP_OFF_AFTER_ONLINE_MS) {
    stopAp();  // give the user time to read the new address before closing the AP
  }
}

bool saveCredentials(const String& newSsid, const String& newPassword) {
  if (newSsid.length() == 0 || newSsid.length() > 32 || newPassword.length() > 64) return false;
  if (newPassword.length() > 0 && newPassword.length() < 8) return false;  // WPA2 minimum
  ssid = newSsid;
  password = newPassword;
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  EG_LOG("Wi-Fi: new network saved (%s)", ssid.c_str());
  WiFi.disconnect();
  startStation();
  return true;
}

String statusJson() {
  String s = "{\"connected\":";
  s += connected() ? "true" : "false";
  s += ",\"ssid\":\"";
  for (char c : ssid) {
    if (c == '"' || c == '\\') s += '\\';
    if ((uint8_t)c >= 0x20) s += c;
  }
  s += "\",\"ip\":\"" + (connected() ? WiFi.localIP().toString() : String()) + "\"";
  s += ",\"rssi\":" + String(connected() ? WiFi.RSSI() : 0);
  s += ",\"ap\":" + String(ap ? "true" : "false");
  s += ",\"apSsid\":\"" + String(AP_SSID) + "\"";
  s += ",\"mdns\":\"" + String(MDNS_NAME) + ".local\"}";
  return s;
}

const char* webUser() { return "admin"; }
String webPassword() { return prefs.getString("webpass", WEB_PASSWORD); }
bool usingDefaultWebPassword() { return !prefs.isKey("webpass"); }

bool saveWebPassword(const String& pw) {
  if (pw.length() < 8 || pw.length() > 64) return false;
  prefs.putString("webpass", pw);
  EG_LOG("Web: password changed");
  return true;
}

}  // namespace net
}  // namespace hubapp
