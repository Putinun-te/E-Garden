#include "Storage.h"

#include <EGarden.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

#include "HubState.h"

namespace hubapp {
namespace storage {

namespace {
bool isMounted = false;
uint32_t lastAirMs = 0, lastWaterMs = 0;
bool anyAir = false, anyWater = false;
Preferences prefs;
const char* const kDoseKeys[3] = {"dA", "dB", "dACID"};

void appendNumber(char* line, size_t len, bool has, float v, int decimals) {
  size_t n = strlen(line);
  if (has) {
    snprintf(line + n, len - n, ",%.*f", decimals, (double)v);
  } else {
    snprintf(line + n, len - n, ",");
  }
}

void rotateIfNeeded() {
  File f = LittleFS.open(HISTORY_CURRENT, "r");
  size_t size = f ? f.size() : 0;
  if (f) f.close();
  if (size < HISTORY_FILE_MAX) return;
  LittleFS.remove(HISTORY_PREVIOUS);
  LittleFS.rename(HISTORY_CURRENT, HISTORY_PREVIOUS);
  EG_LOG("Storage: history rotated");
}

void appendLine(const char* line) {
  if (!isMounted) return;
  rotateIfNeeded();
  File f = LittleFS.open(HISTORY_CURRENT, "a");
  if (!f) {
    EG_LOG("Storage: cannot open history file");
    return;
  }
  f.println(line);
  f.close();  // closing commits the write, so a power cut loses at most this row
}

void startLine(char* line, size_t len, char type) {
  time_t now = time(nullptr);
  long epoch = now > 1700000000 ? (long)now : 0;
  uint32_t boot;
  {
    StateLock l;
    boot = state.bootCount;
  }
  snprintf(line, len, "%ld,%lu,%lu,%c", epoch, (unsigned long)(millis() / 1000), (unsigned long)boot, type);
}
}  // namespace

bool begin() {
  isMounted = LittleFS.begin(true);
  if (!isMounted) {
    EG_LOG("Storage: LittleFS mount failed, history is not saved");
  } else {
    EG_LOG("Storage: LittleFS %u/%u bytes used", (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  }
  prefs.begin("eghist", false);
  return isMounted;
}

fs::FS& fs() { return LittleFS; }
bool mounted() { return isMounted; }

void recordAir(bool hasT, float t, bool hasH, float h, bool hasLux, float lux, bool hasBat, long bat) {
  uint32_t now = millis();
  if (anyAir && now - lastAirMs < HISTORY_MIN_GAP_MS) return;
  anyAir = true;
  lastAirMs = now;
  char line[96];
  startLine(line, sizeof(line), 'A');
  appendNumber(line, sizeof(line), hasT, t, 2);
  appendNumber(line, sizeof(line), hasH, h, 1);
  appendNumber(line, sizeof(line), hasLux, lux, 1);
  appendNumber(line, sizeof(line), hasBat, (float)bat, 0);
  appendLine(line);
}

void recordWater(bool hasPh, float ph, bool hasEc, float ec, bool hasT, float t) {
  uint32_t now = millis();
  if (anyWater && now - lastWaterMs < HISTORY_MIN_GAP_MS) return;
  anyWater = true;
  lastWaterMs = now;
  char line[96];
  startLine(line, sizeof(line), 'W');
  appendNumber(line, sizeof(line), hasPh, ph, 2);
  appendNumber(line, sizeof(line), hasEc, ec, 0);
  appendNumber(line, sizeof(line), hasT, t, 1);
  strncat(line, ",", sizeof(line) - strlen(line) - 1);
  appendLine(line);
}

void saveDoseUnits(const int unitsPerChannel[3]) {
  for (int i = 0; i < 3; i++) prefs.putUChar(kDoseKeys[i], (uint8_t)unitsPerChannel[i]);
}

void loadDoseUnits(int unitsPerChannel[3]) {
  for (int i = 0; i < 3; i++) unitsPerChannel[i] = prefs.getUChar(kDoseKeys[i], 0);
}

size_t historyBytes() {
  if (!isMounted) return 0;
  size_t total = 0;
  for (const char* p : {HISTORY_PREVIOUS, HISTORY_CURRENT}) {
    File f = LittleFS.open(p, "r");
    if (f) {
      total += f.size();
      f.close();
    }
  }
  return total;
}

}  // namespace storage
}  // namespace hubapp
