#include "CloudUpload.h"

#include <Arduino.h>
#include <EGarden.h>

#ifdef ENABLE_CLOUD_UPLOAD
#include <HTTPClient.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

#ifndef SERVER_URL
#error "ENABLE_CLOUD_UPLOAD needs SERVER_URL in secrets.h (see secrets.example.h)"
#endif
#endif

namespace hubapp {
namespace cloud {

#ifdef ENABLE_CLOUD_UPLOAD
namespace {
struct Item {
  char path[96];
};
QueueHandle_t queue = nullptr;

// v1 replaced '.' with '@' in every value and joined them with '|'.
void appendValue(char* out, size_t len, float v, bool first) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.2f", v);
  for (char* p = buf; *p; p++)
    if (*p == '.') *p = '@';
  if (!first) strncat(out, "|", len - strlen(out) - 1);
  strncat(out, buf, len - strlen(out) - 1);
}

void uploadTask(void*) {
  Item item;
  for (;;) {
    if (xQueueReceive(queue, &item, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) continue;
    HTTPClient http;
    http.setTimeout(5000);
    String url = String(SERVER_URL) + item.path;
    if (http.begin(url)) {
      int code = http.GET();
      EG_LOG("Cloud: GET %s -> %d", item.path, code);
      http.end();
    }
  }
}

void enqueue(const char* path) {
  if (!queue) return;
  Item item;
  strncpy(item.path, path, sizeof(item.path) - 1);
  item.path[sizeof(item.path) - 1] = '\0';
  xQueueSend(queue, &item, 0);  // dropped if the server is too slow
}
}  // namespace

void begin() {
  queue = xQueueCreate(6, sizeof(Item));
  xTaskCreatePinnedToCore(uploadTask, "cloud", 6144, nullptr, 1, nullptr, 0);
}

void queueAir(float t, float h, float lux, long bat) {
  char path[96];
  snprintf(path, sizeof(path), "%u", eg::ADDR_AIR);
  appendValue(path, sizeof(path), t, false);
  appendValue(path, sizeof(path), h, false);
  appendValue(path, sizeof(path), lux, false);
  appendValue(path, sizeof(path), (float)bat, false);
  enqueue(path);
}

void queueWater(float ph, float ec, float temp) {
  char path[96];
  snprintf(path, sizeof(path), "%u", eg::ADDR_WATER);
  appendValue(path, sizeof(path), ph, false);
  appendValue(path, sizeof(path), ec, false);
  appendValue(path, sizeof(path), temp, false);
  strncat(path, "|", sizeof(path) - strlen(path) - 1);  // v1: empty battery field
  enqueue(path);
}

bool compiled() { return true; }
#else
void begin() {}
void queueAir(float, float, float, long) {}
void queueWater(float, float, float) {}
bool compiled() { return false; }
#endif

}  // namespace cloud
}  // namespace hubapp
