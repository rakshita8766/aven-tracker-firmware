#include <WiFi.h>

// ESP32 Hotspot credentials
const char* ssid = "ESP32_AP";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);// esp32 creates WiFi
  WiFi.softAP(ssid, password); //starts hotspot

  Serial.print("ESP32 Hotspot Started!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP()); //gives ip
}

void loop() {
  // Check number of connected devices
  int clients = WiFi.softAPgetStationNum(); // count devices

  if (clients > 0) {
    Serial.print("✅ Mobile Connected! Devices: ");
    Serial.println(clients);
  } else {
    Serial.println("No device connected...");
  }

  delay(2000);
}