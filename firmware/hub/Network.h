// Wi-Fi station with automatic reconnect. Never blocks the loop.
#pragma once

namespace hubapp {
namespace net {

void begin();
void poll();
bool connected();

}  // namespace net
}  // namespace hubapp
