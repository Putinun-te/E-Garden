// Time-based actions on the hub: the v1 pump schedule and, when built with
// -DENABLE_AUTO_DOSING, the v1 automatic pH/EC dosing.
#pragma once
#include <time.h>

namespace hubapp {
namespace automation {

// Call once per second with the local time (only when the clock is synced and
// the hub is not in safe mode).
void tick(const struct tm& now, uint32_t nowMs);
bool autoDosingCompiled();

}  // namespace automation
}  // namespace hubapp
