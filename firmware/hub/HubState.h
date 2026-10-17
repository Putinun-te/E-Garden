// Shared hub state. The loop task owns it; web handlers (another FreeRTOS
// task) must hold StateLock while reading it, and only change the system by
// queueing a Command.
#pragma once
#include <Arduino.h>
#include <EGarden.h>
#include <time.h>

namespace hubapp {

struct AirReading {
  bool valid = false;
  bool hasT = false, hasH = false, hasLux = false, hasBat = false;
  float t = 0, h = 0, lux = 0;
  long bat = 0;
  char err[16] = "";
  uint32_t atMs = 0;
  time_t at = 0;  // 0 when the clock was not synced yet
  int rssi = 0;
};

struct WaterReading {
  bool valid = false;
  eg::safety::WaterReading r;
  char err[16] = "";
  uint32_t atMs = 0;
  time_t at = 0;
  int rssi = 0;
};

struct PumpStatus {
  bool known = false;
  bool on = false;
  long left = 0;
  uint32_t atMs = 0;
};

struct DoseStatus {
  bool known = false;
  char dose[6] = "none";
  long left = 0;
  uint32_t atMs = 0;
};

enum class Op : uint8_t { Pump, Dose, Stop };
enum class Source : uint8_t { Web, Schedule, Auto };

struct Command {
  Op op = Op::Stop;
  bool on = false;                                  // Pump
  eg::safety::Channel ch = eg::safety::Channel::A;  // Dose, Stop
  bool allChannels = false;                         // Stop
  int units = 0;                                    // Dose
  Source source = Source::Web;
  uint32_t id = 0;  // set by queueCommand
};

struct CommandRecord {
  uint32_t id = 0;
  char text[32] = "";
  char result[20] = "";  // "queued", "sent", "ok", "no_ack", or a refusal reason
  uint32_t atMs = 0;
};

constexpr int RECORD_COUNT = 12;

struct State {
  AirReading air;
  WaterReading water;
  PumpStatus pump;
  DoseStatus dose;
  eg::safety::ReadingGuard guard;    // hub-side copy of the water node's checks
  eg::safety::DoseLimiter limiter;   // counts doses the water node accepted
  CommandRecord records[RECORD_COUNT];
  int nextRecord = 0;
  bool timeSynced = false;
  bool safeMode = false;
  bool wifiConnected = false;
  uint32_t bootCount = 0;
  char resetReason[20] = "";
};

extern State state;

void stateInit();
void lock();
void unlock();

struct StateLock {
  StateLock() { lock(); }
  ~StateLock() { unlock(); }
};

// Any task may queue; the loop task executes in order. Returns false when full.
bool queueCommand(Command cmd);
bool nextCommand(Command& out);

// Command history shown in the web UI. Call with the lock held.
void describe(const Command& cmd, char* out, size_t len);
void setRecordResult(uint32_t id, const char* result);
const char* sourceName(Source s);

}  // namespace hubapp
