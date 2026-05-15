#include <WiFi.h>

// Create your own WiFi network
const char* ssid = "ESP32_AP";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Start ESP32 as Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("ESP32 Hotspot Started!");
  Serial.print("SSID: ");
  Serial.println(ssid);

  Serial.print("IP Address: ");

  Serial.println(WiFi.softAPIP());
  Serial.print("wifi is connected");
}

void loop() {
  // Nothing needed
}