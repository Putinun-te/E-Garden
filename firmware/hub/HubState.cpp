#include "HubState.h"

#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace hubapp {

State state;

namespace {
SemaphoreHandle_t mutex = nullptr;
QueueHandle_t queue = nullptr;
uint32_t nextId = 1;
}  // namespace

void stateInit() {
  mutex = xSemaphoreCreateRecursiveMutex();
  queue = xQueueCreate(8, sizeof(Command));
}

void lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(mutex); }

const char* sourceName(Source s) {
  switch (s) {
    case Source::Web: return "web";
    case Source::Schedule: return "schedule";
    case Source::Auto: return "auto";
  }
  return "?";
}

void describe(const Command& cmd, char* out, size_t len) {
  switch (cmd.op) {
    case Op::Pump:
      snprintf(out, len, "%s: pump %s", sourceName(cmd.source), cmd.on ? "on" : "off");
      break;
    case Op::Dose:
      snprintf(out, len, "%s: dose %s x%d", sourceName(cmd.source), eg::safety::channelName(cmd.ch), cmd.units);
      break;
    case Op::Stop:
      snprintf(out, len, "%s: stop %s", sourceName(cmd.source), cmd.allChannels ? "ALL" : eg::safety::channelName(cmd.ch));
      break;
  }
}

bool queueCommand(Command cmd) {
  StateLock l;
  cmd.id = nextId++;
  if (xQueueSend(queue, &cmd, 0) != pdTRUE) return false;
  CommandRecord& r = state.records[state.nextRecord];
  state.nextRecord = (state.nextRecord + 1) % RECORD_COUNT;
  r.id = cmd.id;
  describe(cmd, r.text, sizeof(r.text));
  strcpy(r.result, "queued");
  r.atMs = millis();
  return true;
}

bool nextCommand(Command& out) { return xQueueReceive(queue, &out, 0) == pdTRUE; }

void setRecordResult(uint32_t id, const char* result) {
  for (CommandRecord& r : state.records) {
    if (r.id == id) {
      strncpy(r.result, result, sizeof(r.result) - 1);
      r.result[sizeof(r.result) - 1] = '\0';
    }
  }
}

}  // namespace hubapp
