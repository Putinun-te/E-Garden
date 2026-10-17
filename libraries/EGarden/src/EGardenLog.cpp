#ifdef ARDUINO
#include "EGardenLog.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace eg {
namespace log {

namespace {
char lines[EG_LOG_LINES][EG_LOG_LINE_LEN];
uint32_t newestId = 0;  // id of lines[(newestId - 1) % EG_LOG_LINES]
SemaphoreHandle_t lock = nullptr;
}  // namespace

void begin(unsigned long baud) {
  if (!lock) lock = xSemaphoreCreateMutex();
  Serial.begin(baud);
}

void printf(const char* fmt, ...) {
  char buf[EG_LOG_LINE_LEN];
  unsigned long ms = millis();
  int n = snprintf(buf, sizeof(buf), "[%6lu.%03lu] ", ms / 1000, ms % 1000);
  va_list args;
  va_start(args, fmt);
  if (n > 0 && (size_t)n < sizeof(buf)) vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
  va_end(args);
  Serial.println(buf);
  if (!lock) return;
  if (xSemaphoreTake(lock, pdMS_TO_TICKS(20)) == pdTRUE) {
    strcpy(lines[newestId % EG_LOG_LINES], buf);
    newestId++;
    xSemaphoreGive(lock);
  }
}

uint32_t lastId() { return newestId; }

uint32_t forEachSince(uint32_t since, void (*fn)(uint32_t, const char*, void*), void* ctx) {
  if (!lock) return since;
  char copy[EG_LOG_LINE_LEN];
  uint32_t last = since;
  for (;;) {
    bool have = false;
    uint32_t id = 0;
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(20)) != pdTRUE) break;
    uint32_t oldest = newestId > EG_LOG_LINES ? newestId - EG_LOG_LINES + 1 : 1;
    id = last + 1 < oldest ? oldest : last + 1;
    if (id <= newestId) {
      strcpy(copy, lines[(id - 1) % EG_LOG_LINES]);
      have = true;
    }
    xSemaphoreGive(lock);
    if (!have) break;
    fn(id, copy, ctx);  // called without holding the lock
    last = id;
  }
  return last;
}

}  // namespace log
}  // namespace eg
#endif  // ARDUINO
