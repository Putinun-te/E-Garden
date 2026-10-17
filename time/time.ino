#include <WiFi.h>
#include "time.h"
#include "secrets.h"
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 7;
const int   daylightOffset_sec = 3600 * 7;
int H;
int T;

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void loop() {
  delay(1000);
  printLocalTime();
  
  if ((H == 15) && (T == 43)) {
    Serial.println("Yes");
  } else {
    Serial.println("NO");
  }
}


void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);

  char timeminute[3];
  strftime(timeminute, 3, "%M", &timeinfo);

  char timeWeekDay[10];
  strftime(timeWeekDay, 10, "%A", &timeinfo);

  Serial.println();
  H = String(timeHour).toInt();
  T = String(timeminute).toInt();

  Serial.print(H); Serial.print(":"); Serial.print(T);
}
