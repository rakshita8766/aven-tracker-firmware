#include <WiFi.h>

// ===== STA (Mobile Hotspot) =====
const char* sta_ssid = "ESP32_TEST";
const char* sta_password = "12345678";

// ===== AP (ESP32 WiFi) =====
const char* ap_ssid = "ESP32_AP";
const char* ap_password = "12345678";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP_STA);// it enables both wifi modes

  WiFi.softAP(ap_ssid, ap_password); // Start ESP32 Hotspot
  Serial.println("ESP32 AP Started");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.println("Connecting to Hotspot...");
  WiFi.begin(sta_ssid, sta_password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {//this loop will run,untill WiFi gets connected
    delay(1000);
    Serial.println("Connecting...");
    retry++;

    if (retry > 20) {
      Serial.println("Failed to connect to hotspot");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Hotspot!");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  // Check devices connected to ESP32 AP
  int clients = WiFi.softAPgetStationNum();

  if (clients > 0) {
    Serial.print("Device connected to ESP32 AP: ");
    Serial.println(clients);
  } else {
    Serial.println("No device connected to AP");
  }

  delay(3000);
}