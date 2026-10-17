// Optional upload of readings to an HTTP server in the v1 LoRa_Master format.
// Compiled in only with -DENABLE_CLOUD_UPLOAD and SERVER_URL in secrets.h.
// Requests run in their own task so a slow server cannot stall the hub.
#pragma once

namespace hubapp {
namespace cloud {

void begin();
// Queue one reading. path is the part appended to SERVER_URL.
void queueAir(float t, float h, float lux, long bat);
void queueWater(float ph, float ec, float temp);
bool compiled();

}  // namespace cloud
}  // namespace hubapp
