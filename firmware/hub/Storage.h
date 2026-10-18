// Persistent storage on the hub's flash, so data survives a power cut:
// - LittleFS (power-loss safe, replaces v1's SPIFFS): sensor history as two
//   rotating CSV files, at most one air and one water row per minute.
// - NVS (Preferences): dose units of the last hour, so a reboot cannot reset
//   the hourly dosing limit.
#pragma once
#include <Arduino.h>
#include <FS.h>

namespace hubapp {
namespace storage {

constexpr const char* HISTORY_CURRENT = "/hist0.csv";
constexpr const char* HISTORY_PREVIOUS = "/hist1.csv";
constexpr size_t HISTORY_FILE_MAX = 128 * 1024;    // ~2 days at one row per minute per sensor
constexpr uint32_t HISTORY_MIN_GAP_MS = 60000;

bool begin();  // mounts LittleFS (formats it if it cannot be mounted)
fs::FS& fs();
bool mounted();

// CSV columns: epoch,uptime_s,boot,type,a,b,c,d
//   type A: a=temp b=humidity c=lux d=battery   type W: a=pH b=EC c=temp d=(empty)
// epoch is 0 when the clock was not synced yet; uptime_s and boot identify the row then.
void recordAir(bool hasT, float t, bool hasH, float h, bool hasLux, float lux, bool hasBat, long bat);
void recordWater(bool hasPh, float ph, bool hasEc, float ec, bool hasT, float t);

void saveDoseUnits(const int unitsPerChannel[3]);
void loadDoseUnits(int unitsPerChannel[3]);

size_t historyBytes();

}  // namespace storage
}  // namespace hubapp
