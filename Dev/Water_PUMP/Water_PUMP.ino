#include <LoRa.h>

RTC_DATA_ATTR int readingID = 0;

const int RELAY_PIN = 27;
byte msgCount = 0;
String message;
String localAddress = "4";

//Lora
#define ss 5
#define rst 15
#define dio0 2

String localAddress_master = "1";     // address of this device
String destination = "0xAB";      // destination to send to

String loRaMessage;
String Local_ID;
String State;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  LoRa.setPins(ss, rst, dio0);
  Serial.println("initializing LoRa");
  int counter = 0;
  while (!LoRa.begin(433E6) && counter < 30) {
    Serial.print(".");
    counter++;
    delay(500);
  }
  if (counter == 30) {
    // Increment readingID on every new reading
    readingID++;
    // Start deep sleep
    Serial.println("Failed to initialize LoRa. Going to sleep now");
    esp_deep_sleep_start();
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  LoRa.setSyncWord(0xAB);
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa initializing OK!");
// sendMessage();

}

void loop() {
 // put your main code here, to run repeatedly:

}

void onReceive(int packetSize) {
  Serial.print("Received packet '");
  // read packet
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    // LoRaData format: readingID/temperature&humidity$light#batterylevel
    // String example: 1/27.43&654$555#95.34
    Serial.print(LoRaData);
    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(", Snr: ");
    Serial.println(LoRa.packetSnr());
    int pos1 = LoRaData.indexOf('@');
    int pos2 = LoRaData.indexOf('/');
    int pos3 = LoRaData.indexOf('&');
    Local_ID = LoRaData.substring(pos1 + 1, pos2);
    State = LoRaData.substring(pos2 + 1, pos3);
       if (Local_ID == "1"){
      Serial.println(State);
      if (State == "Start"){
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("PUMP START");
      delay(1000);
    }
    else if(State == "Stop")
    {
      digitalWrite(RELAY_PIN, LOW); 
      Serial.println("PUMP STOP");
      delay(1000);
    }
    }
  }
}
void sendMessage(){
  message = String(readingID) + "@" + String(localAddress) + "/" + "PUMP Start" + "&";
  LoRa.beginPacket();                   // start packet
  LoRa.print(message);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}
