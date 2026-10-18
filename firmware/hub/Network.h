// Wi-Fi for the hub: station mode with automatic reconnect, and a setup access
// point ("EGarden-Setup") with a captive portal when no network is reachable.
// Credentials saved from the portal live in NVS and survive power cuts;
// secrets.h only provides defaults. Never blocks the loop.
#pragma once
#include <Arduino.h>

namespace hubapp {
namespace net {

constexpr const char* AP_SSID = "EGarden-Setup";
constexpr const char* MDNS_NAME = "egarden";  // http://egarden.local:8888

void begin();
void poll();
bool connected();
bool apActive();

// Saves new station credentials (NVS) and reconnects. Returns false if invalid.
bool saveCredentials(const String& ssid, const String& password);
// JSON object with the current Wi-Fi state, for the web API.
String statusJson();

// Web UI login (HTTP basic auth, user "admin").
const char* webUser();
String webPassword();
bool saveWebPassword(const String& password);
bool usingDefaultWebPassword();

}  // namespace net
}  // namespace hubapp
