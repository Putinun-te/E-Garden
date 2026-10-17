#include "Network.h"

#include <Arduino.h>
#include <EGarden.h>
#include <WiFi.h>

#include "HubState.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found: building with placeholder Wi-Fi credentials from secrets.h.example"
#include "secrets.h.example"
#endif

namespace hubapp {
namespace net {

namespace {
constexpr uint32_t RECONNECT_EVERY_MS = 30000;
uint32_t lastAttemptMs = 0;
bool wasConnected = false;
}  // namespace

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastAttemptMs = millis();
  EG_LOG("Wi-Fi: connecting to %s", WIFI_SSID);
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

void poll() {
  bool now = connected();
  if (now != wasConnected) {
    wasConnected = now;
    {
      StateLock l;
      state.wifiConnected = now;
    }
    if (now) {
      EG_LOG("Wi-Fi: connected, IP %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      EG_LOG("Wi-Fi: disconnected");
    }
  }
  // v1 waited forever at boot and never reconnected. Retry every 30 s instead.
  if (!now && millis() - lastAttemptMs >= RECONNECT_EVERY_MS) {
    lastAttemptMs = millis();
    EG_LOG("Wi-Fi: retrying");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

}  // namespace net
}  // namespace hubapp
