// E-Garden hub node. Ported from the v1-original sketch; logic unchanged.
#include <Arduino.h>
#include <EGarden.h>
// Import libraries

#ifdef ESP32
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#else
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Hash.h>
#include <AsyncTCP.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#endif
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <LoRa.h>
#include "EEPROM.h"
#include "time.h"

const int output1 = eg::hub::PIN_OUTPUT1;

const char* ntpServer = eg::hub::NTP_SERVER;
const long  gmtOffset_sec = eg::hub::NTP_GMT_OFFSET_SEC;
const int   daylightOffset_sec = eg::hub::NTP_DAYLIGHT_OFFSET_SEC;
char timeHour[3];
char timeMinute[3];

const char* PARAM_STRING = "inputString";
const char* PARAM_INT = "inputInt";
const char* PARAM_FLOAT = "inputFloat";
int H;
int T;
long now = millis();
long lastMeasure = 0;

unsigned long lasttime ;
unsigned long lasttime2 ;

int addr_ph = 0;
int addr_ec = 1;

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found: building with placeholder Wi-Fi credentials from secrets.h.example"
#include "secrets.h.example"
#endif
// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//const char* ssid = WIFI_SSID_ALT;
//const char* password = WIFI_PASSWORD_ALT;


// LoRa Module pin definition
// define the pins used by the LoRa transceiver module

// Initialize variables to get and save LoRa data
int counter = 0; int readingID = 0;
int rssi;
String localAddress = String(eg::ADDR_HUB);
String message;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
//byte localAddress_master = 0xBB;     // address of this device
//byte localAddress_water = 0x01;     // address of this device
//byte localAddress_air = 0x02;     // address of this device
//byte destination = 0xAA;      // destination to send to

String localAddress_master = String(eg::ADDR_HUB);     // address of this device
String localAddress_water = String(eg::ADDR_WATER);     // address of this device
String localAddress_air = String(eg::ADDR_AIR);     // address of this device
String destination = "0xAB";      // destination to send to

String loRaMessage;
String Local_ID;

String temperature_air;
String humidity;
String light;
String batteryLevel_air;

String temperature_water;
String batteryLevel_water;
String ph;
String ec;

String yourInputString; String yourInputInt; String yourInputFloat;

String serverName = "";

String Data1;
String Data2;

float ph_value;
float ec_value;

// Set web server port number to 80
AsyncWebServer server(eg::hub::WEB_PORT);

// Variable to store the HTTP request
String header;

bool Send_API_AIR = false;
bool Send_API_WATER = false;

bool CoC = false;
bool Boolsit2 = true;
bool Boolsit3 = true;
bool Boolsit1 = true;
bool boolet = false;
bool boolet2 = false;
bool boolet3 = false;

// Forward declarations (the Arduino IDE generates these for .ino files, .cpp needs them)
String processor(const String& var);
void notFound(AsyncWebServerRequest *request);
String readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void printLocalTime();
void Sending(String Cont);
void SendingP(String Cont, String NUM);
void onReceive(int packetSize);
void send_API_AIR();
void send_API_WATER();

String processor(const String& var) {
  //Serial.println(var);
  if (var == "Temp") {
    return temperature_air;
  }
  else if (var == "Humid") {
    return humidity;
  }
  else if (var == "Light") {
    return light;
  }
  else if (var == "BATT") {
    return batteryLevel_air;
  }
  else if (var == "Temp2") {
    return temperature_water;
  }
  else if (var == "EC") {
    return ec;
  }
  else if (var == "PH") {
    return ph;
  }
  else if (var == "inputString") {
    return readFile(SPIFFS, "/inputString.txt");
  }
  else if (var == "inputInt") {
    return readFile(SPIFFS, "/inputInt.txt");
  }
  else if (var == "inputFloat") {
    return readFile(SPIFFS, "/inputFloat.txt");
  }
  else if (var == "RRSI") {
    return String(rssi);
  }
  return String();
}
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
String readFile(fs::FS &fs, const char * path) {
  //  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    //    Serial.println("- empty file or failed to open file");
    return String();
  }
  //  Serial.println("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  //  Serial.println(fileContent);
  return fileContent;
}
void writeFile(fs::FS &fs, const char * path, const char * message) {
  //  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    //    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    //    Serial.println("- file written");
  } else {
    //    Serial.println("- write failed");
  }
}
void setup() {
  pinMode(output1, OUTPUT);

  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.begin(115200);

  LoRa.setPins(eg::hub::PIN_LORA_SS, eg::hub::PIN_LORA_RST, eg::hub::PIN_LORA_DIO0);
  LoRa.begin(eg::LORA_FREQUENCY_HZ);
  while (!LoRa.begin(eg::LORA_FREQUENCY_HZ)) {
    Serial.println(".");
    delay(500);
  }

  LoRa.setSyncWord(eg::LORA_SYNC_WORD);

  Serial.println("LoRa Initializing OK!");
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
#else
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
#endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  server.on("/Temp", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", temperature_air.c_str());
  });
  server.on("/Humid", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", humidity.c_str());
  });
  server.on("/Light", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", light.c_str());
  });
  server.on("/Temp2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", temperature_water.c_str());
  });
  server.on("/EC", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", ec.c_str());
  });
  server.on("/PH", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", ph.c_str());
  });
  server.on("/BATT", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", batteryLevel_air.c_str());
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", temperature_air.c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", humidity.c_str());
  });


  server.on("/on1", HTTP_GET, [] (AsyncWebServerRequest * request) {
    Sending("On");
    Serial.println("Sending1 " + message);
    LoRa.receive();
    request->send(200, "text/plain", "ok");
  });
  server.on("/off1", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "ok");
  });



  server.on("/on2", HTTP_GET, [] (AsyncWebServerRequest * request) {
    Sending("oFF");
    Serial.println("Sending2 " + message);
    LoRa.receive();
    request->send(200, "text/plain", "ok");
  });
  server.on("/off2", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "ok");
  });


  server.on("/on3", HTTP_GET, [] (AsyncWebServerRequest * request) {
    //        Serial.println(Boolsit1);
    if (Boolsit1 == true) {
      Sending("Add(A)");
      Serial.println("Sending1 " + message);
      Serial.println("It on A");
      LoRa.receive();
    } else {
      Sending("Stop(A)");
      Serial.println("Sending1 " + message);
      Serial.println("It on !A");
      LoRa.receive();
    }
    Boolsit1 = !Boolsit1;
    //    LoRa.receive();
    request->send(200, "text/plain", "ok");
  });
  server.on("/off3", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "ok");
  });


  server.on("/on4", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (Boolsit2 == true) {
      Sending("Add(B)");
      Serial.println("Sending1 " + message);
      Serial.println("It on B");
      LoRa.receive();
    } else {
      Sending("Stop(B)");
      Serial.println("Sending1 " + message);
      Serial.println("It on !B");
      LoRa.receive();
    }
    Boolsit2 = !Boolsit2;
    //    LoRa.receive();

    request->send(200, "text/plain", "ok");
  });
  server.on("/off4", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "ok");
  });


  server.on("/on5", HTTP_GET, [] (AsyncWebServerRequest * request) {
    if (Boolsit3 == true) {
      Sending("Add(Acid)");
      Serial.println("Sending1 " + message);
      Serial.println("It on Acid");
      LoRa.receive();
    } else {
      Sending("Stop(Acid)");
      Serial.println("Sending1 " + message);
      Serial.println("It on !Acid");
      LoRa.receive();
    }
    Boolsit3 = !Boolsit3;
    LoRa.receive();
    request->send(200, "text/plain", "ok");
  });
  server.on("/off5", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "ok");
  });


  server.on("/rssi", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(rssi).c_str());
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    if (request->hasParam(PARAM_STRING)) {
      inputMessage = request->getParam(PARAM_STRING)->value();
      writeFile(SPIFFS, "/inputString.txt", inputMessage.c_str());
      yourInputString = readFile(SPIFFS, "/inputString.txt");
      boolet = true;
    }
    else if (request->hasParam(PARAM_INT)) {
      inputMessage = request->getParam(PARAM_INT)->value();
      writeFile(SPIFFS, "/inputInt.txt", inputMessage.c_str());
      yourInputInt = readFile(SPIFFS, "/inputInt.txt");
      boolet2 = true;
    }
    else if (request->hasParam(PARAM_FLOAT)) {
      inputMessage = request->getParam(PARAM_FLOAT)->value();
      writeFile(SPIFFS, "/inputFloat.txt", inputMessage.c_str());
      yourInputFloat = readFile(SPIFFS, "/inputFloat.txt");
      boolet3 = true;

    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    if (boolet3 == true) {
      SendingP("Add(Acid)", inputMessage);
      boolet3 = false;
      LoRa.receive();
    }
    if (boolet2 == true) {
      SendingP("Add(B)", inputMessage);
      boolet2 = false;
      LoRa.receive();
    }
    if (boolet == true) {
      SendingP("Add(A)", inputMessage);
      boolet = false;
      Serial.println(message);
      LoRa.receive();
    }

    request->send(200, "text/text", inputMessage);
  });
  server.onNotFound(notFound);
  server.begin();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //  printLocalTime();

  LoRa.onReceive(onReceive);
  LoRa.receive();
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

  Serial.print(H); Serial.print(":"); Serial.println(T);

  if ((H == 8) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 8) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 9) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 9) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 10) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 10) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 11) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 11) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 12) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 12) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 13) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 13) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 14) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 14) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 15) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 15) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 16) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 16) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    LoRa.receive();

  } else if ((H == 17) && (T == 0)) {
    Sending("On");
    Serial.println("on");
    LoRa.receive();
  } else if ((H == 17) && (T == 1)) {
    Sending("oFF");
    Serial.println("oFF");
    //    Serial.println(" ");
    LoRa.receive();
  }
}

void loop() {
  now = millis();
  if (now - lastMeasure > eg::hub::CLOCK_TICK_MS) {
    lastMeasure = now;
    printLocalTime();
  }
}


  void Sending(String Cont) {
    message = String(readingID) + "@" + String(localAddress) + "/" + String(Cont) + "&";
    LoRa.beginPacket();                   // start packet
    LoRa.print(message);                 // add payload
    LoRa.endPacket();                     // finish packet and send it
    msgCount++;                           // increment message ID
  }
  void SendingP(String Cont, String NUM) {
    message = String(readingID) + "@" + String(localAddress) + "/" + String(Cont) + "&" + String(NUM) + "$" ;
    LoRa.beginPacket();                   // start packet
    LoRa.print(message);                 // add payload
    LoRa.endPacket();                     // finish packet and send it
    msgCount++;                           // increment message ID
  }


  void onReceive(int packetSize) {
    Serial.print("Received packet '");
    if (packetSize == 0) return;          // if there's no packet, return
    // read packet
    while (LoRa.available()) {
      String LoRaData = LoRa.readString();
      // LoRaData format: readingID/temperature&humidity$light#batterylevel
      // String example: 1/27.43&654$555#95.34
      Serial.print(LoRaData);
      // print RSSI of packet
      rssi = LoRa.packetRssi();
      Serial.print("' with RSSI ");
      Serial.print(LoRa.packetRssi());
      Serial.print(", Snr: ");
      Serial.println(LoRa.packetSnr());


      int pos1 = LoRaData.indexOf('@');
      int pos2 = LoRaData.indexOf('/');
      Local_ID = LoRaData.substring(pos1 + 1, pos2);
      //    Serial.print("     "); Serial.print(Local_ID);
      if (Local_ID == localAddress_water) {
        Serial.println("Water ID '2'");
        //      int pos1 = LoRaData.indexOf('/');
        int pos3 = LoRaData.indexOf('&');
        int pos4 = LoRaData.indexOf('$');
        int pos5 = LoRaData.indexOf('#');
        int pos6 = LoRaData.indexOf('^');
        int pos7 = LoRaData.indexOf('*');
        //      readingID = LoRaData.substring(pos2 + 1, pos3);
        ph = LoRaData.substring(pos2 + 1, pos3);
        ec = LoRaData.substring(pos3 + 1, pos4);
        temperature_water = LoRaData.substring(pos4 + 1, pos5);
        batteryLevel_water = LoRaData.substring(pos5 + 1, LoRaData.length());
        Serial.print("PH : "); Serial.println(ph);
        Serial.print("EC : "); Serial.println(ec);
        Serial.print("Temp2 : "); Serial.println(temperature_water);
        Serial.print("BATT : "); Serial.println(batteryLevel_water);
        Send_API_WATER = true;
      }
      else if (Local_ID == localAddress_air) {
        Serial.println("Air ID '3'");
        int pos3 = LoRaData.indexOf('&');
        int pos4 = LoRaData.indexOf('$');
        int pos5 = LoRaData.indexOf('#');
        int pos6 = LoRaData.indexOf('^');
        //      readingID = LoRaData.substring(pos2 + 1, pos3);
        temperature_air = LoRaData.substring(pos2 + 1, pos3);
        humidity = LoRaData.substring(pos3 + 1, pos4);
        light = LoRaData.substring(pos4 + 1, pos5);
        batteryLevel_air = LoRaData.substring(pos5 + 1, pos6);
        Serial.print("Temp : "); Serial.println(temperature_air);
        Serial.print("Humid : "); Serial.println(humidity);
        Serial.print("Light : "); Serial.println(light);
        Serial.print("BATT : "); Serial.println(batteryLevel_air);

        Send_API_AIR = true;
      }
      else if (Local_ID == String(eg::ADDR_PUMP)) {
        Serial.println("Air ID '3'");
        int pos3 = LoRaData.indexOf('&');
        int pos4 = LoRaData.indexOf('$');
        int pos5 = LoRaData.indexOf('#');
        int pos6 = LoRaData.indexOf('^');

        Data1 = LoRaData.substring(pos2 + 1, LoRaData.length());
        Serial.println(Data1);
      }
    }
  }
  void send_API_AIR() {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      //      String T18 = String(Temp_DS18B20);
      //      T18.replace(".","@");
      String ID = String(Local_ID);
      ID.replace(".", "@");
      String B_P = String(batteryLevel_air);
      B_P.replace(".", "@");
      String L17 = String(light);
      L17.replace(".", "@");
      String T23 = String(temperature_air);
      T23.replace(".", "@");
      String H23 = String(humidity);
      H23.replace(".", "@");

      String text_All = ID + "|" + T23 + "|" + H23 + "|" + L17 + "|" + B_P;

      String serverPath = serverName + text_All;//เอาค่า sensor มาใส่ตรงนี้ครับ

      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
  }

  void send_API_WATER() {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      //      String T18 = String(Temp_DS18B20);
      //      T18.replace(".","@");
      String ID = String(Local_ID);
      ID.replace(".", "@");
      String B_P = String(batteryLevel_water);
      B_P.replace(".", "@");
      String PH = String(ph);
      PH.replace(".", "@");
      String EC = String(ec);
      EC.replace(".", "@");
      String T18 = String(temperature_water);
      T18.replace(".", "@");

      String text_All = ID + "|" + PH + "|" + EC + "|" + T18 + "|" + B_P;

      String serverPath = serverName + text_All;//เอาค่า sensor มาใส่ตรงนี้ครับ

      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
  }
