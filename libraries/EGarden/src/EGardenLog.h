// Logging to Serial plus an in-memory ring buffer (the hub serves it as a web
// console). Safe to call from any FreeRTOS task.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef EG_LOG_LINES
#define EG_LOG_LINES 8  // the hub raises this with -DEG_LOG_LINES=...
#endif
#define EG_LOG_LINE_LEN 120

namespace eg {
namespace log {

void begin(unsigned long baud = 115200);
void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Id of the newest buffered line (0 = nothing logged yet). Ids keep increasing.
uint32_t lastId();
// Copies buffered lines with id > since, oldest first, by calling fn for each.
// Returns the id of the last line passed to fn (or `since` if none).
uint32_t forEachSince(uint32_t since, void (*fn)(uint32_t id, const char* line, void* ctx), void* ctx);

}  // namespace log
}  // namespace eg

#define EG_LOG(...) ::eg::log::printf(__VA_ARGS__)
