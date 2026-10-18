// Hub web server (port 8888).
#pragma once

namespace hubapp {
namespace web {

void begin();
void poll();
void onAirReading();
void onWaterReading();

}  // namespace web
}  // namespace hubapp
